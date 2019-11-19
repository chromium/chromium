// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_frame_helper.h"

#include <set>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/renderer/api/automation/automation_api_helper.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_renderer_messaging_service.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"

namespace extensions {

namespace {

constexpr int kMainWorldId = 0;

base::LazyInstance<std::set<const ExtensionFrameHelper*>>::DestructorAtExit
    g_frame_helpers = LAZY_INSTANCE_INITIALIZER;

// Returns true if the render frame corresponding with |frame_helper| matches
// the given criteria.
bool RenderFrameMatches(const ExtensionFrameHelper* frame_helper,
                        ViewType match_view_type,
                        int match_window_id,
                        int match_tab_id,
                        const std::string& match_extension_id) {
  if (match_view_type != VIEW_TYPE_INVALID &&
      frame_helper->view_type() != match_view_type)
    return false;

  // Not all frames have a valid ViewType, e.g. devtools, most GuestViews, and
  // unclassified detached WebContents.
  if (frame_helper->view_type() == VIEW_TYPE_INVALID)
    return false;

  // This logic matches ExtensionWebContentsObserver::GetExtensionFromFrame.
  blink::WebSecurityOrigin origin =
      frame_helper->render_frame()->GetWebFrame()->GetSecurityOrigin();
  if (origin.IsUnique() ||
      !base::EqualsASCII(origin.Protocol().Utf16(), kExtensionScheme) ||
      !base::EqualsASCII(origin.Host().Utf16(), match_extension_id.c_str()))
    return false;

  if (match_window_id != extension_misc::kUnknownWindowId &&
      frame_helper->browser_window_id() != match_window_id)
    return false;

  if (match_tab_id != extension_misc::kUnknownTabId &&
      frame_helper->tab_id() != match_tab_id)
    return false;

  return true;
}

// Runs every callback in |callbacks_to_be_run_and_cleared| while |frame_helper|
// is valid, and clears |callbacks_to_be_run_and_cleared|.
void RunCallbacksWhileFrameIsValid(
    base::WeakPtr<ExtensionFrameHelper> frame_helper,
    std::vector<base::Closure>* callbacks_to_be_run_and_cleared) {
  // The JavaScript code can cause re-entrancy. To avoid a deadlock, don't run
  // callbacks that are added during the iteration.
  std::vector<base::Closure> callbacks;
  callbacks_to_be_run_and_cleared->swap(callbacks);
  for (auto& callback : callbacks) {
    callback.Run();
    if (!frame_helper.get())
      return;  // Frame and ExtensionFrameHelper invalidated by callback.
  }
}

enum class PortType {
  EXTENSION,
  TAB,
  NATIVE_APP,
};

// Returns an extension hosted in the |render_frame| (or nullptr if the frame
// doesn't host an extension).
const Extension* GetExtensionFromFrame(content::RenderFrame* render_frame) {
  DCHECK(render_frame);
  ScriptContext* context =
      ScriptContextSet::GetMainWorldContextForFrame(render_frame);
  return context ? context->effective_extension() : nullptr;
}

}  // namespace

ExtensionFrameHelper::ExtensionFrameHelper(content::RenderFrame* render_frame,
                                           Dispatcher* extension_dispatcher)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<ExtensionFrameHelper>(render_frame),
      view_type_(VIEW_TYPE_INVALID),
      tab_id_(-1),
      browser_window_id_(-1),
      extension_dispatcher_(extension_dispatcher),
      did_create_current_document_element_(false) {
  g_frame_helpers.Get().insert(this);
  if (render_frame->IsMainFrame()) {
    // Manages its own lifetime.
    new AutomationApiHelper(render_frame);
  }
}

ExtensionFrameHelper::~ExtensionFrameHelper() {
  g_frame_helpers.Get().erase(this);
}

// static
std::vector<content::RenderFrame*> ExtensionFrameHelper::GetExtensionFrames(
    const std::string& extension_id,
    int browser_window_id,
    int tab_id,
    ViewType view_type) {
  std::vector<content::RenderFrame*> render_frames;
  for (const ExtensionFrameHelper* helper : g_frame_helpers.Get()) {
    if (RenderFrameMatches(helper, view_type, browser_window_id, tab_id,
                           extension_id))
      render_frames.push_back(helper->render_frame());
  }
  return render_frames;
}

// static
v8::Local<v8::Array> ExtensionFrameHelper::GetV8MainFrames(
    v8::Local<v8::Context> context,
    const std::string& extension_id,
    int browser_window_id,
    int tab_id,
    ViewType view_type) {
  // WebFrame::ScriptCanAccess uses the isolate's current context. We need to
  // make sure that the current context is the one we're expecting.
  DCHECK(context == context->GetIsolate()->GetCurrentContext());
  std::vector<content::RenderFrame*> render_frames =
      GetExtensionFrames(extension_id, browser_window_id, tab_id, view_type);
  v8::Local<v8::Array> v8_frames = v8::Array::New(context->GetIsolate());

  int v8_index = 0;
  for (content::RenderFrame* frame : render_frames) {
    if (!frame->IsMainFrame())
      continue;

    blink::WebLocalFrame* web_frame = frame->GetWebFrame();
    if (!blink::WebFrame::ScriptCanAccess(web_frame))
      continue;

    v8::Local<v8::Context> frame_context = web_frame->MainWorldScriptContext();
    if (!frame_context.IsEmpty()) {
      v8::Local<v8::Value> window = frame_context->Global();
      CHECK(!window.IsEmpty());
      v8::Maybe<bool> maybe =
          v8_frames->CreateDataProperty(context, v8_index++, window);
      CHECK(maybe.IsJust() && maybe.FromJust());
    }
  }

  return v8_frames;
}

// static
content::RenderFrame* ExtensionFrameHelper::GetBackgroundPageFrame(
    const std::string& extension_id) {
  for (const ExtensionFrameHelper* helper : g_frame_helpers.Get()) {
    if (RenderFrameMatches(helper, VIEW_TYPE_EXTENSION_BACKGROUND_PAGE,
                           extension_misc::kUnknownWindowId,
                           extension_misc::kUnknownTabId, extension_id)) {
      blink::WebLocalFrame* web_frame = helper->render_frame()->GetWebFrame();
      // Check if this is the top frame.
      if (web_frame->Top() == web_frame)
        return helper->render_frame();
    }
  }
  return nullptr;
}

v8::Local<v8::Value> ExtensionFrameHelper::GetV8BackgroundPageMainFrame(
    v8::Isolate* isolate,
    const std::string& extension_id) {
  content::RenderFrame* main_frame = GetBackgroundPageFrame(extension_id);

  v8::Local<v8::Value> background_page;
  blink::WebLocalFrame* web_frame =
      main_frame ? main_frame->GetWebFrame() : nullptr;
  if (web_frame && blink::WebFrame::ScriptCanAccess(web_frame))
    background_page = web_frame->MainWorldScriptContext()->Global();
  else
    background_page = v8::Undefined(isolate);

  return background_page;
}

// static
content::RenderFrame* ExtensionFrameHelper::FindFrame(
    content::RenderFrame* relative_to_frame,
    const std::string& name) {
  // Only pierce browsing instance boundaries if |relative_to_frame| is an
  // extension.
  const Extension* extension = GetExtensionFromFrame(relative_to_frame);
  if (!extension)
    return nullptr;

  for (const ExtensionFrameHelper* target : g_frame_helpers.Get()) {
    // Skip frames with a mismatched name.
    if (target->render_frame()->GetWebFrame()->AssignedName().Utf8() != name)
      continue;

    // Only pierce browsing instance boundaries if the target frame is from the
    // same extension (but not when another extension shares the same renderer
    // process because of reuse trigerred by process limit).
    if (extension != GetExtensionFromFrame(target->render_frame()))
      continue;

    return target->render_frame();
  }

  return nullptr;
}

// static
bool ExtensionFrameHelper::IsContextForEventPage(const ScriptContext* context) {
  content::RenderFrame* render_frame = context->GetRenderFrame();
  return context->extension() && render_frame &&
         BackgroundInfo::HasLazyBackgroundPage(context->extension()) &&
         ExtensionFrameHelper::Get(render_frame)->view_type() ==
              VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
}

void ExtensionFrameHelper::DidCreateDocumentElement() {
  did_create_current_document_element_ = true;
  extension_dispatcher_->DidCreateDocumentElement(
      render_frame()->GetWebFrame());
}

void ExtensionFrameHelper::DidCreateNewDocument() {
  did_create_current_document_element_ = false;
}

void ExtensionFrameHelper::RunScriptsAtDocumentStart() {
  DCHECK(did_create_current_document_element_);
  RunCallbacksWhileFrameIsValid(weak_ptr_factory_.GetWeakPtr(),
                                &document_element_created_callbacks_);
  // |this| might be dead by now.
}

void ExtensionFrameHelper::RunScriptsAtDocumentEnd() {
  RunCallbacksWhileFrameIsValid(weak_ptr_factory_.GetWeakPtr(),
                                &document_load_finished_callbacks_);
  // |this| might be dead by now.
}

void ExtensionFrameHelper::RunScriptsAtDocumentIdle() {
  RunCallbacksWhileFrameIsValid(weak_ptr_factory_.GetWeakPtr(),
                                &document_idle_callbacks_);
  // |this| might be dead by now.
}

void ExtensionFrameHelper::ScheduleAtDocumentStart(
    const base::Closure& callback) {
  document_element_created_callbacks_.push_back(callback);
}

void ExtensionFrameHelper::ScheduleAtDocumentEnd(
    const base::Closure& callback) {
  document_load_finished_callbacks_.push_back(callback);
}

void ExtensionFrameHelper::ScheduleAtDocumentIdle(
    const base::Closure& callback) {
  document_idle_callbacks_.push_back(callback);
}

void ExtensionFrameHelper::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  // New window created by chrome.app.window.create() must not start parsing the
  // document immediately. The chrome.app.window.create() callback (if any)
  // needs to be called prior to the new window's 'load' event. The parser will
  // be resumed when it happens. It doesn't apply to sandboxed pages.
  if (view_type_ == VIEW_TYPE_APP_WINDOW && render_frame()->IsMainFrame() &&
      !has_started_first_navigation_ &&
      GURL(document_loader->GetUrl()).SchemeIs(kExtensionScheme) &&
      !ScriptContext::IsSandboxedPage(document_loader->GetUrl())) {
    document_loader->BlockParser();
  }

  has_started_first_navigation_ = true;

  if (!delayed_main_world_script_initialization_)
    return;

  delayed_main_world_script_initialization_ = false;
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  v8::Context::Scope context_scope(context);
  // Normally we would use Document's URL for all kinds of checks, e.g. whether
  // to inject a content script. However, when committing a navigation, we
  // should use the URL of a Document being committed instead. This URL is
  // accessible through WebDocumentLoader::GetURL().
  // The scope below temporary maps a frame to a document loader, so that places
  // which retrieve URL can use the right one. Ideally, we would plumb the
  // correct URL (or maybe WebDocumentLoader) through the callchain, but there
  // are many callers which will have to pass nullptr.
  ScriptContext::ScopedFrameDocumentLoader scoped_document_loader(
      render_frame()->GetWebFrame(), document_loader);
  extension_dispatcher_->DidCreateScriptContext(render_frame()->GetWebFrame(),
                                                context, kMainWorldId);
  // TODO(devlin): Add constants for main world id, no extension group.
}

void ExtensionFrameHelper::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  // Grant cross browsing instance frame lookup if we are an extension. This
  // should match the conditions in FindFrame.
  content::RenderFrame* frame = render_frame();
  if (GetExtensionFromFrame(frame))
    frame->SetAllowsCrossBrowsingInstanceFrameLookup();
}

void ExtensionFrameHelper::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  if (world_id == kMainWorldId) {
    if (render_frame()->IsBrowserSideNavigationPending()) {
      // Defer initializing the extensions script context now because it depends
      // on having the URL of the provisional load which isn't available at this
      // point.
      // We can come here twice in the case of window.open(url): first for
      // about:blank empty document, then possibly for the actual url load
      // (depends on whoever triggers window proxy init), before getting
      // ReadyToCommitNavigation.
      delayed_main_world_script_initialization_ = true;
      return;
    }
    // Sometimes DidCreateScriptContext comes before ReadyToCommitNavigation.
    // In this case we don't have to wait until ReadyToCommitNavigation.
    // TODO(dgozman): ensure consistent call order between
    // DidCreateScriptContext and ReadyToCommitNavigation.
    delayed_main_world_script_initialization_ = false;
  }
  extension_dispatcher_->DidCreateScriptContext(render_frame()->GetWebFrame(),
                                                context, world_id);
}

void ExtensionFrameHelper::WillReleaseScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  extension_dispatcher_->WillReleaseScriptContext(
      render_frame()->GetWebFrame(), context, world_id);
}

bool ExtensionFrameHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionFrameHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ValidateMessagePort,
                        OnExtensionValidateMessagePort)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnConnect,
                        OnExtensionDispatchOnConnect)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DeliverMessage, OnExtensionDeliverMessage)
    IPC_MESSAGE_HANDLER(ExtensionMsg_DispatchOnDisconnect,
                        OnExtensionDispatchOnDisconnect)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetTabId, OnExtensionSetTabId)
    IPC_MESSAGE_HANDLER(ExtensionMsg_UpdateBrowserWindowId,
                        OnUpdateBrowserWindowId)
    IPC_MESSAGE_HANDLER(ExtensionMsg_NotifyRenderViewType,
                        OnNotifyRendererViewType)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Response, OnExtensionResponse)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnExtensionMessageInvoke)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetFrameName, OnSetFrameName)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AppWindowClosed, OnAppWindowClosed)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetSpatialNavigationEnabled,
                        OnSetSpatialNavigationEnabled)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionFrameHelper::OnExtensionValidateMessagePort(int worker_thread_id,
                                                          const PortId& id) {
  DCHECK_EQ(kMainThreadId, worker_thread_id);
  extension_dispatcher_->bindings_system()
      ->messaging_service()
      ->ValidateMessagePort(
          extension_dispatcher_->script_context_set_iterator(), id,
          render_frame());
}

void ExtensionFrameHelper::OnExtensionDispatchOnConnect(
    int worker_thread_id,
    const PortId& target_port_id,
    const std::string& channel_name,
    const ExtensionMsg_TabConnectionInfo& source,
    const ExtensionMsg_ExternalConnectionInfo& info) {
  DCHECK_EQ(kMainThreadId, worker_thread_id);
  extension_dispatcher_->bindings_system()
      ->messaging_service()
      ->DispatchOnConnect(extension_dispatcher_->script_context_set_iterator(),
                          target_port_id, channel_name, source, info,
                          render_frame());
}

void ExtensionFrameHelper::OnExtensionDeliverMessage(int worker_thread_id,
                                                     const PortId& target_id,
                                                     const Message& message) {
  DCHECK_EQ(kMainThreadId, worker_thread_id);
  extension_dispatcher_->bindings_system()->messaging_service()->DeliverMessage(
      extension_dispatcher_->script_context_set_iterator(), target_id, message,
      render_frame());
}

void ExtensionFrameHelper::OnExtensionDispatchOnDisconnect(
    int worker_thread_id,
    const PortId& id,
    const std::string& error_message) {
  DCHECK_EQ(kMainThreadId, worker_thread_id);
  extension_dispatcher_->bindings_system()
      ->messaging_service()
      ->DispatchOnDisconnect(
          extension_dispatcher_->script_context_set_iterator(), id,
          error_message, render_frame());
}

void ExtensionFrameHelper::OnExtensionSetTabId(int tab_id) {
  CHECK_EQ(tab_id_, -1);
  CHECK_GE(tab_id, 0);
  tab_id_ = tab_id;
}

void ExtensionFrameHelper::OnUpdateBrowserWindowId(int browser_window_id) {
  browser_window_id_ = browser_window_id;
}

void ExtensionFrameHelper::OnNotifyRendererViewType(ViewType type) {
  // TODO(devlin): It'd be really nice to be able to
  // DCHECK_EQ(VIEW_TYPE_INVALID, view_type_) here.
  view_type_ = type;
}

void ExtensionFrameHelper::OnExtensionResponse(int request_id,
                                               bool success,
                                               const base::ListValue& response,
                                               const std::string& error) {
  extension_dispatcher_->OnExtensionResponse(request_id,
                                             success,
                                             response,
                                             error);
}

void ExtensionFrameHelper::OnExtensionMessageInvoke(
    const std::string& extension_id,
    const std::string& module_name,
    const std::string& function_name,
    const base::ListValue& args) {
  extension_dispatcher_->InvokeModuleSystemMethod(
      render_frame(), extension_id, module_name, function_name, args);
}

void ExtensionFrameHelper::OnSetFrameName(const std::string& name) {
  render_frame()->GetWebFrame()->SetName(blink::WebString::FromUTF8(name));
}

void ExtensionFrameHelper::OnAppWindowClosed(bool send_onclosed) {
  DCHECK(render_frame()->IsMainFrame());

  if (!send_onclosed)
    return;

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Context> v8_context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(v8_context);
  if (!script_context)
    return;
  script_context->module_system()->CallModuleMethodSafe("app.window",
                                                        "onAppWindowClosed");
}

void ExtensionFrameHelper::OnSetSpatialNavigationEnabled(bool enabled) {
  render_frame()
      ->GetRenderView()
      ->GetWebView()
      ->GetSettings()
      ->SetSpatialNavigationEnabled(enabled);
}

void ExtensionFrameHelper::OnDestruct() {
  delete this;
}

void ExtensionFrameHelper::DraggableRegionsChanged() {
  if (!render_frame()->IsMainFrame())
    return;

  blink::WebVector<blink::WebDraggableRegion> webregions =
      render_frame()->GetWebFrame()->GetDocument().DraggableRegions();
  std::vector<DraggableRegion> regions;
  for (blink::WebDraggableRegion& webregion : webregions) {
    render_frame()->ConvertViewportToWindow(&webregion.bounds);

    regions.push_back(DraggableRegion());
    DraggableRegion& region = regions.back();
    region.bounds = webregion.bounds;
    region.draggable = webregion.draggable;
  }
  Send(new ExtensionHostMsg_UpdateDraggableRegions(routing_id(), regions));
}

}  // namespace extensions
