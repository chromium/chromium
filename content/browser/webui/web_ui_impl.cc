// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/web_ui_impl.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/debug/dump_without_crashing.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"

namespace content {

class WebUIImpl::MainFrameNavigationObserver : public WebContentsObserver {
 public:
  MainFrameNavigationObserver(WebUIImpl* web_ui, WebContents* contents)
      : WebContentsObserver(contents), web_ui_(web_ui) {}
  ~MainFrameNavigationObserver() override {}

 private:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    // Only disallow JavaScript on cross-document navigations in the main frame.
    if (!navigation_handle->IsInMainFrame() ||
        !navigation_handle->HasCommitted() ||
        navigation_handle->IsSameDocument()) {
      return;
    }

    web_ui_->DisallowJavascriptOnAllHandlers();
  }

  WebUIImpl* web_ui_;
};

const WebUI::TypeID WebUI::kNoWebUI = nullptr;

// static
base::string16 WebUI::GetJavascriptCall(
    const std::string& function_name,
    const std::vector<const base::Value*>& arg_list) {
  base::string16 result(base::ASCIIToUTF16(function_name));
  result.push_back('(');

  std::string json;
  for (size_t i = 0; i < arg_list.size(); ++i) {
    if (i > 0)
      result.push_back(',');

    base::JSONWriter::Write(*arg_list[i], &json);
    result.append(base::UTF8ToUTF16(json));
  }

  result.push_back(')');
  result.push_back(';');
  return result;
}

WebUIImpl::WebUIImpl(WebContentsImpl* contents)
    : bindings_(BINDINGS_POLICY_WEB_UI),
      web_contents_(contents),
      web_contents_observer_(new MainFrameNavigationObserver(this, contents)) {
  DCHECK(contents);
}

WebUIImpl::~WebUIImpl() {
  // Delete the controller first, since it may also be keeping a pointer to some
  // of the handlers and can call them at destruction.
  controller_.reset();
}

// WebUIImpl, public: ----------------------------------------------------------

bool WebUIImpl::OnMessageReceived(const IPC::Message& message,
                                  RenderFrameHost* sender) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(WebUIImpl, message, sender)
    IPC_MESSAGE_HANDLER(FrameHostMsg_WebUISend, OnWebUISend)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebUIImpl::OnWebUISend(RenderFrameHost* sender,
                            const std::string& message,
                            const base::ListValue& args) {
  // Ignore IPCs from frames that are pending deletion.  See also
  // https://crbug.com/780920.
  if (!sender->IsCurrent())
    return;

  const GURL& source_url = sender->GetLastCommittedURL();
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
          sender->GetProcess()->GetID()) ||
      !WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
          web_contents_->GetBrowserContext(), source_url)) {
    bad_message::ReceivedBadMessage(
        sender->GetProcess(),
        bad_message::WEBUI_SEND_FROM_UNAUTHORIZED_PROCESS);
    return;
  }

  if (base::EndsWith(message, "RequiringGesture",
                     base::CompareCase::SENSITIVE) &&
      !web_contents_->HasRecentInteractiveInputEvent()) {
    LOG(ERROR) << message << " received without recent user interaction";
    return;
  }

  ProcessWebUIMessage(source_url, message, args);
}

void WebUIImpl::RenderFrameCreated(RenderFrameHost* render_frame_host) {
  controller_->RenderFrameCreated(render_frame_host);
}

void WebUIImpl::RenderFrameReused(RenderFrameHost* render_frame_host) {
  if (!render_frame_host->GetParent()) {
    GURL site_url = render_frame_host->GetSiteInstance()->GetSiteURL();
    GetContentClient()->browser()->LogWebUIUrl(site_url);
  }
}

void WebUIImpl::RenderFrameHostSwappingOut() {
  DisallowJavascriptOnAllHandlers();
}

WebContents* WebUIImpl::GetWebContents() {
  return web_contents_;
}

float WebUIImpl::GetDeviceScaleFactor() {
  return GetScaleFactorForView(web_contents_->GetRenderWidgetHostView());
}

const base::string16& WebUIImpl::GetOverriddenTitle() {
  return overridden_title_;
}

void WebUIImpl::OverrideTitle(const base::string16& title) {
  overridden_title_ = title;
}

int WebUIImpl::GetBindings() {
  return bindings_;
}

void WebUIImpl::SetBindings(int bindings) {
  bindings_ = bindings;
}

WebUIController* WebUIImpl::GetController() {
  return controller_.get();
}

void WebUIImpl::SetController(std::unique_ptr<WebUIController> controller) {
  DCHECK(controller);
  controller_ = std::move(controller);
}

bool WebUIImpl::CanCallJavascript() {
  RenderFrameHost* frame_host = web_contents_->GetMainFrame();
  return frame_host &&
         (ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
              frame_host->GetProcess()->GetID()) ||
          // It's possible to load about:blank in a Web UI renderer.
          // See http://crbug.com/42547
          frame_host->GetLastCommittedURL().spec() == url::kAboutBlankURL);
}

void WebUIImpl::CallJavascriptFunctionUnsafe(const std::string& function_name) {
  DCHECK(base::IsStringASCII(function_name));
  base::string16 javascript = base::ASCIIToUTF16(function_name + "();");
  ExecuteJavascript(javascript);
}

void WebUIImpl::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg1,
                                             const base::Value& arg2) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg1,
                                             const base::Value& arg2,
                                             const base::Value& arg3) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::CallJavascriptFunctionUnsafe(const std::string& function_name,
                                             const base::Value& arg1,
                                             const base::Value& arg2,
                                             const base::Value& arg3,
                                             const base::Value& arg4) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  args.push_back(&arg4);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::CallJavascriptFunctionUnsafe(
    const std::string& function_name,
    const std::vector<const base::Value*>& args) {
  DCHECK(base::IsStringASCII(function_name));
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIImpl::RegisterMessageCallback(base::StringPiece message,
                                        const MessageCallback& callback) {
  message_callbacks_.emplace(message.as_string(), callback);
}

void WebUIImpl::ProcessWebUIMessage(const GURL& source_url,
                                    const std::string& message,
                                    const base::ListValue& args) {
  if (controller_->OverrideHandleWebUIMessage(source_url, message, args))
    return;

  // Look up the callback for this message.
  auto callback_pair = message_callbacks_.find(message);
  if (callback_pair != message_callbacks_.end()) {
    // Forward this message and content on.
    callback_pair->second.Run(&args);
  } else {
    NOTREACHED() << "Unhandled chrome.send(\"" << message << "\");";
  }
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

void WebUIImpl::ExecuteJavascript(const base::string16& javascript) {
  // Silently ignore the request. Would be nice to clean-up WebUI so we
  // could turn this into a CHECK(). http://crbug.com/516690.
  if (!CanCallJavascript())
    return;

  web_contents_->GetMainFrame()->ExecuteJavaScript(javascript,
                                                   base::NullCallback());
}

void WebUIImpl::DisallowJavascriptOnAllHandlers() {
  for (const std::unique_ptr<WebUIMessageHandler>& handler : handlers_)
    handler->DisallowJavascript();
}

}  // namespace content
