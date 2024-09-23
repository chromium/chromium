// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_frame_helper.h"

#include <set>

#include "base/feature_list.h"
#include "base/containers/map_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/renderer/api/messaging/native_renderer_messaging_service.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/ipc_message_sender.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

base::LazyInstance<std::set<const ExtensionFrameHelper*>>::DestructorAtExit
    g_frame_helpers = LAZY_INSTANCE_INITIALIZER;

// Returns true if the render frame corresponding with |frame_helper| matches
// the given criteria.
//
// We deliberately do not access any methods that require a v8::Context or
// ScriptContext.  See also comment below.
bool RenderFrameMatches(const ExtensionFrameHelper* frame_helper,
                        mojom::ViewType match_view_type,
                        int match_window_id,
                        int match_tab_id,
                        const ExtensionId& match_extension_id) {
  if (match_view_type != mojom::ViewType::kInvalid &&
      frame_helper->view_type() != match_view_type)
    return false;

  // Not all frames have a valid ViewType, e.g. devtools, most GuestViews, and
  // unclassified detached WebContents.
  if (frame_helper->view_type() == mojom::ViewType::kInvalid)
    return false;

  // This logic matches ExtensionWebContentsObserver::GetExtensionFromFrame.
  blink::WebSecurityOrigin origin =
      frame_helper->render_frame()->GetWebFrame()->GetSecurityOrigin();
  if (origin.IsOpaque() ||
      !base::EqualsASCII(origin.Protocol().Utf16(), kExtensionScheme) ||
      !base::EqualsASCII(origin.Host().Utf16(), match_extension_id.c_str()))
    return false;

  if (match_window_id != extension_misc::kUnknownWindowId &&
      frame_helper->browser_window_id() != match_window_id)
    return false;

  if (match_tab_id != extension_misc::kUnknownTabId &&
      frame_helper->tab_id() != match_tab_id)
    return false;

  // Returning handles to frames that haven't created a script context yet
  // can result in the caller "forcing" a script context (by accessing
  // properties on the window object). This, in turn, can cause the script
  // context to be initialized prematurely, with invalid values (e.g., the
  // inability to retrieve a valid URL from the frame). That then leads to
  // the ScriptContext being misclassified.
  // Don't return any frames until they have a valid ScriptContext to limit
  // the chances for bindings to prematurely initialize these contexts.
  // This fixes https://crbug.com/1021014.
  return frame_helper->did_create_script_context();
}

// Runs every callback in |callbacks_to_be_run_and_cleared| while |frame_helper|
// is valid, and clears |callbacks_to_be_run_and_cleared|.
void RunCallbacksWhileFrameIsValid(
    base::WeakPtr<ExtensionFrameHelper> frame_helper,
    std::vector<base::OnceClosure>* callbacks_to_be_run_and_cleared) {
  // The JavaScript code can cause re-entrancy. To avoid a deadlock, don't run
  // callbacks that are added during the iteration.
  std::vector<base::OnceClosure> callbacks;
  callbacks_to_be_run_and_cleared->swap(callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
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
      extension_dispatcher_(extension_dispatcher) {
  g_frame_helpers.Get().insert(this);
  if (render_frame->GetWebFrame()
          ->GetDocumentLoader()
          ->HasLoadedNonInitialEmptyDocument()) {
    // With RenderDocument, cross-document navigations create a new RenderFrame
    // (and thus, a new ExtensionFrameHelper). However, the frame tree node
    // itself may have already navigated and loaded documents, so set
    // `has_started_first_navigation_` to true in that case.
    has_started_first_navigation_ = true;
  }

  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::LocalFrame>(
          base::BindRepeating(&ExtensionFrameHelper::BindLocalFrame,
                              weak_ptr_factory_.GetWeakPtr()));
}

ExtensionFrameHelper::~ExtensionFrameHelper() {
  g_frame_helpers.Get().erase(this);
}

// static
std::vector<content::RenderFrame*> ExtensionFrameHelper::GetExtensionFrames(
    const ExtensionId& extension_id,
    int browser_window_id,
    int tab_id,
    mojom::ViewType view_type) {
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
    const ExtensionId& extension_id,
    int browser_window_id,
    int tab_id,
    mojom::ViewType view_type) {
  // WebFrame::ScriptCanAccess uses the isolate's current context. We need to
  // make sure that the current context is the one we're expecting.
  DCHECK(context == context->GetIsolate()->GetCurrentContext());
  std::vector<content::RenderFrame*> render_frames =
      GetExtensionFrames(extension_id, browser_window_id, tab_id, view_type);
  v8::Local<v8::Array> v8_frames = v8::Array::New(context->GetIsolate());

  int v8_index = 0;
  for (content::RenderFrame* frame : render_frames) {
    blink::WebLocalFrame* web_frame = frame->GetWebFrame();
    if (!web_frame->IsOutermostMainFrame())
      continue;

    if (!blink::WebFrame::ScriptCanAccess(context->GetIsolate(), web_frame)) {
      continue;
    }

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
    const ExtensionId& extension_id) {
  for (const ExtensionFrameHelper* helper : g_frame_helpers.Get()) {
    if (RenderFrameMatches(helper, mojom::ViewType::kExtensionBackgroundPage,
                           extension_misc::kUnknownWindowId,
                           extension_misc::kUnknownTabId, extension_id)) {
      blink::WebLocalFrame* web_frame = helper->render_frame()->GetWebFrame();
      // Check if this is the outermost main frame (do not return embedded
      // main frames like fenced frames).
      if (web_frame->IsOutermostMainFrame())
        return helper->render_frame();
    }
  }
  return nullptr;
}

v8::Local<v8::Value> ExtensionFrameHelper::GetV8BackgroundPageMainFrame(
    v8::Isolate* isolate,
    const ExtensionId& extension_id) {
  content::RenderFrame* main_frame = GetBackgroundPageFrame(extension_id);
  blink::WebLocalFrame* web_frame =
      main_frame ? main_frame->GetWebFrame() : nullptr;
  if (web_frame && blink::WebFrame::ScriptCanAccess(isolate, web_frame)) {
    return web_frame->MainWorldScriptContext()->Global();
  } else {
    return v8::Undefined(isolate);
  }
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
             mojom::ViewType::kExtensionBackgroundPage;
}

void ExtensionFrameHelper::BindLocalFrame(
    mojo::PendingAssociatedReceiver<mojom::LocalFrame> pending_receiver) {
  local_frame_receiver_.Bind(std::move(pending_receiver));
}

void ExtensionFrameHelper::DidCreateDocumentElement() {
  did_create_current_document_element_ = true;
  extension_dispatcher_->DidCreateDocumentElement(
      render_frame()->GetWebFrame());
}

void ExtensionFrameHelper::DidCreateNewDocument() {
  did_create_current_document_element_ = false;
  active_user_script_worlds_.clear();
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

void ExtensionFrameHelper::ScheduleAtDocumentStart(base::OnceClosure callback) {
  document_element_created_callbacks_.push_back(std::move(callback));
}

void ExtensionFrameHelper::ScheduleAtDocumentEnd(base::OnceClosure callback) {
  document_load_finished_callbacks_.push_back(std::move(callback));
}

void ExtensionFrameHelper::ScheduleAtDocumentIdle(base::OnceClosure callback) {
  document_idle_callbacks_.push_back(std::move(callback));
}

const std::set<std::optional<std::string>>*
ExtensionFrameHelper::GetActiveUserScriptWorlds(
      const ExtensionId& extension_id) {
  return base::FindOrNull(active_user_script_worlds_, extension_id);
}

void ExtensionFrameHelper::AddActiveUserScriptWorld(
    const ExtensionId& extension_id,
    const std::optional<std::string>& world_id) {
  active_user_script_worlds_[extension_id].insert(world_id);
}

mojom::LocalFrameHost* ExtensionFrameHelper::GetLocalFrameHost() {
  if (!local_frame_host_remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        local_frame_host_remote_.BindNewEndpointAndPassReceiver());
  }
  return local_frame_host_remote_.get();
}

mojom::RendererHost* ExtensionFrameHelper::GetRendererHost() {
  if (!renderer_host_remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        renderer_host_remote_.BindNewEndpointAndPassReceiver());
  }
  return renderer_host_remote_.get();
}

mojom::EventRouter* ExtensionFrameHelper::GetEventRouter() {
  if (!event_router_remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &event_router_remote_);
  }
  return event_router_remote_.get();
}

mojom::RendererAutomationRegistry*
ExtensionFrameHelper::GetRendererAutomationRegistry() {
  if (!renderer_automation_registry_remote_.is_bound()) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &renderer_automation_registry_remote_);
  }
  return renderer_automation_registry_remote_.get();
}

void ExtensionFrameHelper::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  // New window created by chrome.app.window.create() must not start parsing the
  // document immediately. The chrome.app.window.create() callback (if any)
  // needs to be called prior to the new window's 'load' event. The parser will
  // be resumed when it happens. It doesn't apply to sandboxed pages.
  if (view_type_ == mojom::ViewType::kAppWindow &&
      web_frame->IsOutermostMainFrame() && !has_started_first_navigation_ &&
      GURL(document_loader->GetUrl()).SchemeIs(kExtensionScheme) &&
      !ScriptContext::IsSandboxedPage(document_loader->GetUrl())) {
    document_loader->BlockParser();
  }

  has_started_first_navigation_ = true;

  if (!delayed_main_world_script_initialization_)
    return;

  base::AutoReset<bool> auto_reset(&is_initializing_main_world_script_context_,
                                   true);
  delayed_main_world_script_initialization_ = false;
  v8::HandleScope handle_scope(web_frame->GetAgentGroupScheduler()->Isolate());
  v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
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
      web_frame, document_loader);
  extension_dispatcher_->DidCreateScriptContext(web_frame, context,
                                                blink::kMainDOMWorldId);
  // TODO(devlin): Add constants for main world id, no extension group.
}

void ExtensionFrameHelper::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  if (base::FeatureList::IsEnabled(
          extensions_features::kAvoidEarlyExtensionScriptContextCreation)) {
    return;
  }
  // Grant cross browsing instance frame lookup if we are an extension. This
  // should match the conditions in FindFrame.
  content::RenderFrame* frame = render_frame();
  if (GetExtensionFromFrame(frame))
    frame->SetAllowsCrossBrowsingInstanceFrameLookup();
}

void ExtensionFrameHelper::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  if (world_id == blink::kMainDOMWorldId) {
    // Accessing MainWorldScriptContext() in ReadyToCommitNavigation() may
    // trigger the script context initializing, so we don't want to initialize a
    // second time here.
    if (is_initializing_main_world_script_context_)
      return;
    if (render_frame()->IsRequestingNavigation()) {
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

void ExtensionFrameHelper::SetTabId(int32_t tab_id) {
  CHECK_EQ(tab_id_, -1);
  CHECK_GE(tab_id, 0);
  tab_id_ = tab_id;
}

void ExtensionFrameHelper::NotifyRenderViewType(mojom::ViewType type) {
  // TODO(devlin): It'd be really nice to be able to
  // DCHECK_EQ(mojom::ViewType::kInvalid, view_type_) here.
  view_type_ = type;
}

void ExtensionFrameHelper::MessageInvoke(const ExtensionId& extension_id,
                                         const std::string& module_name,
                                         const std::string& function_name,
                                         base::Value::List args) {
  extension_dispatcher_->InvokeModuleSystemMethod(
      render_frame(), extension_id, module_name, function_name, args);
}

void ExtensionFrameHelper::ExecuteCode(mojom::ExecuteCodeParamsPtr param,
                                       ExecuteCodeCallback callback) {
  // Sanity checks.
  if (param->injection->is_css()) {
    if (param->injection->get_css()->sources.empty()) {
      local_frame_receiver_.ReportBadMessage(
          "At least one CSS source must be specified.");
      return;
    }

    if (param->injection->get_css()->operation ==
            mojom::CSSInjection::Operation::kRemove &&
        !base::ranges::all_of(param->injection->get_css()->sources,
                              [](const mojom::CSSSourcePtr& source) {
                                return source->key.has_value();
                              })) {
      local_frame_receiver_.ReportBadMessage(
          "An injection key must be specified for CSS removal.");
      return;
    }
  } else {
    DCHECK(param->injection->is_js());  // Enforced by mojo.
    if (param->injection->get_js()->sources.empty()) {
      local_frame_receiver_.ReportBadMessage(
          "At least one JS source must be specified.");
      return;
    }
  }

  extension_dispatcher_->ExecuteCode(std::move(param), std::move(callback),
                                     render_frame());
}

void ExtensionFrameHelper::SetFrameName(const std::string& name) {
  render_frame()->GetWebFrame()->SetName(blink::WebString::FromUTF8(name));
}

void ExtensionFrameHelper::AppWindowClosed(bool send_onclosed) {
  DCHECK(render_frame()->GetWebFrame()->IsOutermostMainFrame());

  if (!send_onclosed)
    return;

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  v8::HandleScope scope(web_frame->GetAgentGroupScheduler()->Isolate());
  v8::Local<v8::Context> v8_context = web_frame->MainWorldScriptContext();
  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(v8_context);
  if (!script_context)
    return;
  script_context->module_system()->CallModuleMethodSafe("app.window",
                                                        "onAppWindowClosed");
}

void ExtensionFrameHelper::SetSpatialNavigationEnabled(bool enabled) {
  render_frame()
      ->GetWebView()
      ->GetSettings()
      ->SetSpatialNavigationEnabled(enabled);
}

void ExtensionFrameHelper::ExecuteDeclarativeScript(
    int32_t tab_id,
    const ExtensionId& extension_id,
    const std::string& script_id,
    const GURL& url) {
  // TODO(crbug.com/40753624): URL-checking isn't the best approach to
  // avoid user data leak. Consider what we can do to mitigate this case.
  // Begin script injection workflow only if the current URL is identical to the
  // one that matched declarative conditions in the browser.
  if (GURL(render_frame()->GetWebFrame()->GetDocument().Url()) == url) {
    extension_dispatcher_->ExecuteDeclarativeScript(
        render_frame(), tab_id, extension_id, script_id, url);
  }
}

void ExtensionFrameHelper::UpdateBrowserWindowId(int32_t window_id) {
  browser_window_id_ = window_id;
}

void ExtensionFrameHelper::DispatchOnConnect(
    const PortId& port_id,
    extensions::mojom::ChannelType channel_type,
    const std::string& channel_name,
    extensions::mojom::TabConnectionInfoPtr tab_info,
    extensions::mojom::ExternalConnectionInfoPtr external_connection_info,
    mojo::PendingAssociatedReceiver<extensions::mojom::MessagePort> port,
    mojo::PendingAssociatedRemote<extensions::mojom::MessagePortHost> port_host,
    DispatchOnConnectCallback callback) {
  extension_dispatcher_->bindings_system()
      ->messaging_service()
      ->DispatchOnConnect(extension_dispatcher_->script_context_set_iterator(),
                          port_id, channel_type, channel_name, *tab_info,
                          *external_connection_info, std::move(port),
                          std::move(port_host), render_frame(),
                          std::move(callback));
}

void ExtensionFrameHelper::NotifyDidCreateScriptContext(int32_t world_id) {
  did_create_script_context_ = true;
}

void ExtensionFrameHelper::OnDestruct() {
  delete this;
}

void ExtensionFrameHelper::DidClearWindowObject() {
  // DidClearWindowObject() is called right at the end of
  // DocumentLoader::CreateParserPostCommit(). This is late enough in the commit
  // process that it won't interfere with any optimizations, since the code
  // below may cause the V8 context to be initialized.
  //
  // Calling this multiple times in a page load is safe because
  // SetAllowsCrossBrowsingInstanceFrameLookup() just sets a bool to true on the
  // SecurityOrigin.
  if (base::FeatureList::IsEnabled(
          extensions_features::kAvoidEarlyExtensionScriptContextCreation)) {
    // Grant cross browsing instance frame lookup if we are an extension. This
    // should match the conditions in FindFrame.
    content::RenderFrame* frame = render_frame();
    if (GetExtensionFromFrame(frame))
      frame->SetAllowsCrossBrowsingInstanceFrameLookup();
  }
}

content::RenderFrame* ExtensionFrameHelper::FindFrameFromFrameTokenString(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_string) {
  CHECK(v8_string->IsString());

  std::string frame_token;
  if (!gin::Converter<std::string>::FromV8(isolate, v8_string, &frame_token)) {
    return nullptr;
  }
  auto token = base::Token::FromString(frame_token);
  if (!token) {
    return nullptr;
  }
  auto unguessable_token =
      base::UnguessableToken::Deserialize(token->high(), token->low());
  if (!unguessable_token) {
    return nullptr;
  }
  auto* web_frame = blink::WebLocalFrame::FromFrameToken(
      blink::LocalFrameToken(unguessable_token.value()));
  return content::RenderFrame::FromWebFrame(web_frame);
}

}  // namespace extensions
