// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_impl.h"

#include <stddef.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_data_source_impl.h"
#include "content/browser/webui/web_ui_main_frame_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"

namespace content {

namespace {

template <typename Range>
std::u16string GetJavascriptCallImpl(std::string_view function_name,
                                     const Range& args) {
  std::vector<std::u16string> json_args;
  for (const auto& arg : args) {
    json_args.push_back(base::UTF8ToUTF16(*base::WriteJson(arg)));
  }

  std::u16string result(base::ASCIIToUTF16(function_name));
  result.push_back('(');
  result.append(base::JoinString(json_args, u","));
  result.push_back(')');
  result.push_back(';');
  return result;
}

blink::mojom::LocalResourceLoaderConfigPtr CreateLocalResourceLoaderConfig(
    URLDataManagerBackend* data_backend) {
  auto loader_config = blink::mojom::LocalResourceLoaderConfig::New();
  std::vector<blink::mojom::LocalResourceSourcePtr>& loader_source_list =
      loader_config->sources;
  for (auto const& [source_name, data_source] : data_backend->data_sources()) {
    // For a data source to be useful in the renderer process, it must have a
    // map from path to resource ID. Only WebUIDataSourceImpls have a map from
    // path to resource ID. Most URLDataSources are not WebUIDataSourceImpls,
    // e.g. favicon, image, etc.
    if (!data_source->IsWebUIDataSourceImpl()) {
      continue;
    }
    auto* webui_data_source =
        static_cast<WebUIDataSourceImpl*>(data_source.get());
    // We only support data sources that serve URLs of the form: chrome://*
    if (webui_data_source->GetScheme() != kChromeUIScheme) {
      continue;
    }
    auto loader_source = blink::mojom::LocalResourceSource::New();
    webui_data_source->EnsureLoadTimeDataDefaultsAdded();
    loader_source->name = source_name;
    loader_source->headers =
        URLDataManagerBackend::GetHeaders(webui_data_source, GURL("/"), "")
            ->raw_headers();
    loader_source->should_replace_i18n_in_js =
        data_source->source()->ShouldReplaceI18nInJS();
    loader_source->path_to_resource_id_map.insert(
        webui_data_source->path_to_idr_map().begin(),
        webui_data_source->path_to_idr_map().end());
    loader_source->replacement_strings.insert(
        webui_data_source->source()->GetReplacements()->begin(),
        webui_data_source->source()->GetReplacements()->end());
    loader_source_list.push_back(std::move(loader_source));
  }
  return loader_config;
}

}  // namespace

const WebUI::TypeID WebUI::kNoWebUI = nullptr;

// static
std::u16string WebUI::GetJavascriptCall(
    std::string_view function_name,
    base::span<const base::ValueView> arg_list) {
  return GetJavascriptCallImpl(function_name, arg_list);
}

// static
std::u16string WebUI::GetJavascriptCall(std::string_view function_name,
                                        const base::Value::List& arg_list) {
  return GetJavascriptCallImpl(function_name, arg_list);
}

WebUIImpl::WebUIImpl(WebContents* web_contents)
    : requestable_schemes_({kChromeUIScheme, url::kFileScheme}),
      web_contents_(web_contents),
      web_contents_observer_(
          std::make_unique<WebUIMainFrameObserver>(this, web_contents_)) {
  DCHECK(web_contents_);
}

WebUIImpl::WebUIImpl(NavigationRequest* request)
    : WebUIImpl(
          WebContents::FromFrameTreeNodeId(request->GetFrameTreeNodeId())) {}

WebUIImpl::~WebUIImpl() {
  // Delete the controller first, since it may also be keeping a pointer to some
  // of the handlers and can call them at destruction.
  // Note: Calling this might delete |web_content_| and |frame_host_|. The two
  // pointers are now potentially dangling.
  // See https://crbug.com/1308391
  controller_.reset();

  remote_.reset();
  receiver_.reset();
}

void WebUIImpl::SetProperty(const std::string& name, const std::string& value) {
  DCHECK(remote_);
  remote_->SetProperty(name, value);
}

void WebUIImpl::Send(const std::string& message, base::Value::List args) {
  const GURL& source_url = frame_host_->GetLastCommittedURL();
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          frame_host_->GetProcess()->GetID()) ||
      !WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
          web_contents_->GetBrowserContext(), source_url)) {
    bad_message::ReceivedBadMessage(
        frame_host_->GetProcess(),
        bad_message::WEBUI_SEND_FROM_UNAUTHORIZED_PROCESS);
    return;
  }

  if (base::EndsWith(message, "RequiringGesture",
                     base::CompareCase::SENSITIVE) &&
      !web_contents_->HasRecentInteraction()) {
    LOG(ERROR) << message << " received without recent user interaction";
    return;
  }

  ProcessWebUIMessage(source_url, message, std::move(args));
}

void WebUIImpl::SetRenderFrameHost(RenderFrameHost* render_frame_host) {
  frame_host_ =
      static_cast<RenderFrameHostImpl*>(render_frame_host)->GetWeakPtr();
  // Assert that we can only open WebUI for the active or speculative pages.
  DCHECK(frame_host_->lifecycle_state() ==
             RenderFrameHostImpl::LifecycleStateImpl::kActive ||
         frame_host_->lifecycle_state() ==
             RenderFrameHostImpl::LifecycleStateImpl::kSpeculative);
}

void WebUIImpl::WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) {
  controller_->WebUIRenderFrameCreated(render_frame_host);
}

void WebUIImpl::RenderFrameHostUnloading() {
  DisallowJavascriptOnAllHandlers();
}

void WebUIImpl::RenderFrameDeleted() {
  DisallowJavascriptOnAllHandlers();
}

void WebUIImpl::SetUpMojoConnection() {
  // TODO(nasko): WebUI mojo might be useful to be registered for
  // subframes as well, though at this time there is no such usage but currently
  // this is expected to be called only for outermost main frames.
  if (frame_host_->GetParentOrOuterDocument())
    return;

  frame_host_->GetFrameBindingsControl()->BindWebUI(
      remote_.BindNewEndpointAndPassReceiver(),
      receiver_.BindNewEndpointAndPassRemote());
}

void WebUIImpl::TearDownMojoConnection() {
  // This is expected to be called only for outermost main frames.
  if (frame_host_->GetParentOrOuterDocument())
    return;

  remote_.reset();
  receiver_.reset();
}

WebContents* WebUIImpl::GetWebContents() {
  return web_contents_;
}

float WebUIImpl::GetDeviceScaleFactor() {
  return GetScaleFactorForView(web_contents_->GetRenderWidgetHostView());
}

const std::u16string& WebUIImpl::GetOverriddenTitle() {
  return overridden_title_;
}

void WebUIImpl::OverrideTitle(const std::u16string& title) {
  overridden_title_ = title;
}

BindingsPolicySet WebUIImpl::GetBindings() {
  return bindings_;
}

void WebUIImpl::SetBindings(BindingsPolicySet bindings) {
  bindings_ = bindings;
}

const std::vector<std::string>& WebUIImpl::GetRequestableSchemes() {
  return requestable_schemes_;
}

void WebUIImpl::AddRequestableScheme(const char* scheme) {
  requestable_schemes_.push_back(scheme);
}

WebUIController* WebUIImpl::GetController() {
  return controller_.get();
}

RenderFrameHost* WebUIImpl::GetRenderFrameHost() {
  return frame_host_.get();
}

bool WebUIImpl::HasRenderFrameHost() const {
  return !!frame_host_;
}

void WebUIImpl::SetController(std::unique_ptr<WebUIController> controller) {
  DCHECK(controller);
  controller_ = std::move(controller);
}

bool WebUIImpl::CanCallJavascript() {
  return (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
              frame_host_->GetProcess()->GetID()) ||
          // It's possible to load about:blank in a Web UI renderer.
          // See http://crbug.com/42547
          frame_host_->GetLastCommittedURL().spec() == url::kAboutBlankURL);
}

void WebUIImpl::CallJavascriptFunctionUnsafe(
    std::string_view function_name,
    base::span<const base::ValueView> args) {
  DCHECK(base::IsStringASCII(function_name));
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::RegisterMessageCallback(std::string_view message,
                                        MessageCallback callback) {
  message_callbacks_.emplace(message, std::move(callback));
}

void WebUIImpl::ProcessWebUIMessage(const GURL& source_url,
                                    const std::string& message,
                                    base::Value::List args) {
  if (controller_->OverrideHandleWebUIMessage(source_url, message, args))
    return;

  auto callback_pair = message_callbacks_.find(message);
  if (callback_pair != message_callbacks_.end()) {
    // Forward this message and content on.
    callback_pair->second.Run(args);
    return;
  }

  DUMP_WILL_BE_NOTREACHED() << "Unhandled chrome.send(\"" << message << "\", "
                            << args << "); from " << source_url;
}

std::vector<std::unique_ptr<WebUIMessageHandler>>*
WebUIImpl::GetHandlersForTesting() {
  return &handlers_;
}

// WebUIImpl, protected: -------------------------------------------------------

void WebUIImpl::AddMessageHandler(
    std::unique_ptr<WebUIMessageHandler> handler) {
  DCHECK(!handler->web_ui());
  handler->set_web_ui(this);
  handler->RegisterMessages();
  handlers_.push_back(std::move(handler));
}

void WebUIImpl::ExecuteJavascript(const std::u16string& javascript) {
  // Silently ignore the request. Would be nice to clean-up WebUI so we
  // could turn this into a CHECK(). http://crbug.com/516690.
  if (!CanCallJavascript())
    return;

  frame_host_->ExecuteJavaScript(javascript, base::NullCallback());
}

void WebUIImpl::DisallowJavascriptOnAllHandlers() {
  for (const std::unique_ptr<WebUIMessageHandler>& handler : handlers_)
    handler->DisallowJavascript();
}

blink::mojom::LocalResourceLoaderConfigPtr
WebUIImpl::GetLocalResourceLoaderConfig() {
  URLDataManagerBackend* data_backend =
      URLDataManagerBackend::GetForBrowserContext(
          web_contents_->GetBrowserContext());
  return CreateLocalResourceLoaderConfig(data_backend);
}

// static
blink::mojom::LocalResourceLoaderConfigPtr
WebUIImpl::GetLocalResourceLoaderConfigForTesting(
    URLDataManagerBackend* data_backend) {
  return CreateLocalResourceLoaderConfig(data_backend);
}

}  // namespace content
