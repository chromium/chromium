// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_impl.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
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
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_main_frame_observer.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

namespace {

template <typename Range>
std::u16string GetJavascriptCallImpl(base::StringPiece function_name,
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

}  // namespace

const WebUI::TypeID WebUI::kNoWebUI = nullptr;

// static
std::u16string WebUI::GetJavascriptCall(
    base::StringPiece function_name,
    base::span<const base::ValueView> arg_list) {
  return GetJavascriptCallImpl(function_name, arg_list);
}

// static
std::u16string WebUI::GetJavascriptCall(base::StringPiece function_name,
                                        const base::Value::List& arg_list) {
  return GetJavascriptCallImpl(function_name, arg_list);
}

WebUIImpl::WebUIImpl(WebContents* web_contents)
    : bindings_(BINDINGS_POLICY_WEB_UI),
      requestable_schemes_({kChromeUIScheme, url::kFileScheme}),
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

void WebUIImpl::RenderFrameReused(RenderFrameHost* render_frame_host) {
  // This is expected to be called only for outermost main frames.
  if (!render_frame_host->GetParentOrOuterDocument()) {
    GURL site_url = render_frame_host->GetSiteInstance()->GetSiteURL();
    GetContentClient()->browser()->LogWebUIUrl(site_url);
  }
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

int WebUIImpl::GetBindings() {
  return bindings_;
}

void WebUIImpl::SetBindings(int bindings) {
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

void WebUIImpl::CallJavascriptFunctionUnsafe(base::StringPiece function_name) {
  DCHECK(base::IsStringASCII(function_name));
  std::u16string javascript =
      base::ASCIIToUTF16(base::StrCat({function_name, "();"}));
  ExecuteJavascript(javascript);
}

void WebUIImpl::CallJavascriptFunctionUnsafe(
    base::StringPiece function_name,
    base::span<const base::ValueView> args) {
  DCHECK(base::IsStringASCII(function_name));
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::RegisterMessageCallback(base::StringPiece message,
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

  NOTREACHED() << "Unhandled chrome.send(\"" << message << "\", " << args
               << "); from " << source_url;
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

}  // namespace content
