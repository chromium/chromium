/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/local_dom_window.h"

#include <memory>
#include <utility>

#include "cc/input/snap_selection_strategy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/aom/computed_accessible_node.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/dom_window_css.h"
#include "third_party/blink/renderer/core/css/media_query_list.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_media.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/dom/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/core/dom/sink_document.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/events/hash_change_event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/events/pop_state_event.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/bar_prop.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/dom_visual_viewport.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/external.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/history.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/frame/scroll_to_options.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/create_window.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// Timeout for link preloads to be used after window.onload
static constexpr base::TimeDelta kUnusedPreloadTimeout =
    base::TimeDelta::FromSeconds(3);

static void UpdateSuddenTerminationStatus(
    LocalDOMWindow* dom_window,
    bool added_listener,
    SuddenTerminationDisablerType disabler_type) {
  Platform::Current()->SuddenTerminationChanged(!added_listener);
  if (dom_window->GetFrame() && dom_window->GetFrame()->Client())
    dom_window->GetFrame()->Client()->SuddenTerminationDisablerChanged(
        added_listener, disabler_type);
}

using DOMWindowSet = HeapHashCountedSet<WeakMember<LocalDOMWindow>>;

static DOMWindowSet& WindowsWithUnloadEventListeners() {
  DEFINE_STATIC_LOCAL(Persistent<DOMWindowSet>,
                      windows_with_unload_event_listeners,
                      (MakeGarbageCollected<DOMWindowSet>()));
  return *windows_with_unload_event_listeners;
}

static DOMWindowSet& WindowsWithBeforeUnloadEventListeners() {
  DEFINE_STATIC_LOCAL(Persistent<DOMWindowSet>,
                      windows_with_before_unload_event_listeners,
                      (MakeGarbageCollected<DOMWindowSet>()));
  return *windows_with_before_unload_event_listeners;
}

static void TrackUnloadEventListener(LocalDOMWindow* dom_window) {
  DOMWindowSet& set = WindowsWithUnloadEventListeners();
  if (set.insert(dom_window).is_new_entry) {
    // The first unload handler was added.
    UpdateSuddenTerminationStatus(dom_window, true, kUnloadHandler);
  }
}

static void UntrackUnloadEventListener(LocalDOMWindow* dom_window) {
  DOMWindowSet& set = WindowsWithUnloadEventListeners();
  DOMWindowSet::iterator it = set.find(dom_window);
  if (it == set.end())
    return;
  if (set.erase(it)) {
    // The last unload handler was removed.
    UpdateSuddenTerminationStatus(dom_window, false, kUnloadHandler);
  }
}

static void UntrackAllUnloadEventListeners(LocalDOMWindow* dom_window) {
  DOMWindowSet& set = WindowsWithUnloadEventListeners();
  DOMWindowSet::iterator it = set.find(dom_window);
  if (it == set.end())
    return;
  set.RemoveAll(it);
  UpdateSuddenTerminationStatus(dom_window, false, kUnloadHandler);
}

static void TrackBeforeUnloadEventListener(LocalDOMWindow* dom_window) {
  DOMWindowSet& set = WindowsWithBeforeUnloadEventListeners();
  if (set.insert(dom_window).is_new_entry) {
    // The first beforeunload handler was added.
    UpdateSuddenTerminationStatus(dom_window, true, kBeforeUnloadHandler);
  }
}

static void UntrackBeforeUnloadEventListener(LocalDOMWindow* dom_window) {
  DOMWindowSet& set = WindowsWithBeforeUnloadEventListeners();
  DOMWindowSet::iterator it = set.find(dom_window);
  if (it == set.end())
    return;
  if (set.erase(it)) {
    // The last beforeunload handler was removed.
    UpdateSuddenTerminationStatus(dom_window, false, kBeforeUnloadHandler);
  }
}

static void UntrackAllBeforeUnloadEventListeners(LocalDOMWindow* dom_window) {
  DOMWindowSet& set = WindowsWithBeforeUnloadEventListeners();
  DOMWindowSet::iterator it = set.find(dom_window);
  if (it == set.end())
    return;
  set.RemoveAll(it);
  UpdateSuddenTerminationStatus(dom_window, false, kBeforeUnloadHandler);
}

LocalDOMWindow::LocalDOMWindow(LocalFrame& frame)
    : DOMWindow(frame),
      visualViewport_(MakeGarbageCollected<DOMVisualViewport>(this)),
      unused_preloads_timer_(frame.GetTaskRunner(TaskType::kInternalDefault),
                             this,
                             &LocalDOMWindow::WarnUnusedPreloads),
      should_print_when_finished_loading_(false) {}

void LocalDOMWindow::ClearDocument() {
  if (!document_)
    return;

  DCHECK(!document_->IsActive());

  unused_preloads_timer_.Stop();
  document_->ClearDOMWindow();
  document_ = nullptr;
}

void LocalDOMWindow::AcceptLanguagesChanged() {
  if (navigator_)
    navigator_->SetLanguagesDirty();

  DispatchEvent(*Event::Create(event_type_names::kLanguagechange));
}

TrustedTypePolicyFactory* LocalDOMWindow::trustedTypes() const {
  if (!trusted_types_) {
    trusted_types_ =
        MakeGarbageCollected<TrustedTypePolicyFactory>(GetExecutionContext());
  }
  return trusted_types_.Get();
}

Document* LocalDOMWindow::CreateDocument(const String& mime_type,
                                         const DocumentInit& init,
                                         bool force_xhtml) {
  Document* document = nullptr;
  if (force_xhtml) {
    // This is a hack for XSLTProcessor. See
    // XSLTProcessor::createDocumentFromSource().
    document = MakeGarbageCollected<Document>(init);
  } else {
    document = DOMImplementation::createDocument(
        mime_type, init,
        init.GetFrame() ? init.GetFrame()->InViewSourceMode() : false);
    if (document->IsPluginDocument() &&
        document->IsSandboxed(WebSandboxFlags::kPlugins)) {
      // document->Shutdown();
      document = MakeGarbageCollected<SinkDocument>(init);
    }
  }

  return document;
}

LocalDOMWindow* LocalDOMWindow::From(const ScriptState* script_state) {
  v8::HandleScope scope(script_state->GetIsolate());
  return blink::ToLocalDOMWindow(script_state->GetContext());
}

Document* LocalDOMWindow::InstallNewDocument(const String& mime_type,
                                             const DocumentInit& init,
                                             bool force_xhtml) {
  DCHECK_EQ(init.GetFrame(), GetFrame());

  ClearDocument();

  document_ = CreateDocument(mime_type, init, force_xhtml);
  document_->Initialize();

  if (!GetFrame())
    return document_;

  GetFrame()->GetScriptController().UpdateDocument();
  document_->GetViewportData().UpdateViewportDescription();
  if (FrameScheduler* frame_scheduler = GetFrame()->GetFrameScheduler()) {
    frame_scheduler->TraceUrlChange(document_->Url().GetString());
    frame_scheduler->SetCrossOrigin(GetFrame()->IsCrossOriginSubframe());
  }

  if (GetFrame()->GetPage() && GetFrame()->View()) {
    GetFrame()->GetPage()->GetChromeClient().InstallSupplements(*GetFrame());
  }

  return document_;
}

void LocalDOMWindow::EnqueueWindowEvent(Event& event, TaskType task_type) {
  EnqueueEvent(event, task_type);
}

void LocalDOMWindow::EnqueueDocumentEvent(Event& event, TaskType task_type) {
  if (document_)
    document_->EnqueueEvent(event, task_type);
}

void LocalDOMWindow::DispatchWindowLoadEvent() {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif
  // Delay 'load' event if we are in EventQueueScope.  This is a short-term
  // workaround to avoid Editing code crashes.  We should always dispatch
  // 'load' event asynchronously.  crbug.com/569511.
  if (ScopedEventQueue::Instance()->ShouldQueueEvents() && document_) {
    document_->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(FROM_HERE, WTF::Bind(&LocalDOMWindow::DispatchLoadEvent,
                                        WrapPersistent(this)));
    return;
  }
  DispatchLoadEvent();
}

void LocalDOMWindow::DocumentWasClosed() {
  DispatchWindowLoadEvent();
  EnqueueNonPersistedPageshowEvent();
  if (pending_state_object_)
    EnqueuePopstateEvent(std::move(pending_state_object_));
}

void LocalDOMWindow::EnqueueNonPersistedPageshowEvent() {
  // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36334 Pageshow event needs
  // to fire asynchronously.  As per spec pageshow must be triggered
  // asynchronously.  However to be compatible with other browsers blink fires
  // pageshow synchronously unless we are in EventQueueScope.
  if (ScopedEventQueue::Instance()->ShouldQueueEvents() && document_) {
    // The task source should be kDOMManipulation, but the spec doesn't say
    // anything about this.
    EnqueueWindowEvent(*PageTransitionEvent::Create(event_type_names::kPageshow,
                                                    false /* persisted */),
                       TaskType::kMiscPlatformAPI);
    return;
  }
  DispatchEvent(*PageTransitionEvent::Create(event_type_names::kPageshow,
                                             false /* persisted */),
                document_.Get());
}

void LocalDOMWindow::DispatchPersistedPageshowEvent(
    base::TimeTicks navigation_start) {
  // Persisted pageshow events are dispatched for pages that are restored from
  // the back forward cache, and the event's timestamp should reflect the
  // |navigation_start| time of the back navigation.
  DispatchEvent(*PageTransitionEvent::CreatePersistedPageshow(navigation_start),
                document_.Get());
}

void LocalDOMWindow::EnqueueHashchangeEvent(const String& old_url,
                                            const String& new_url) {
  // https://html.spec.whatwg.org/C/#history-traversal
  EnqueueWindowEvent(*HashChangeEvent::Create(old_url, new_url),
                     TaskType::kDOMManipulation);
}

void LocalDOMWindow::EnqueuePopstateEvent(
    scoped_refptr<SerializedScriptValue> state_object) {
  // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36202 Popstate event needs
  // to fire asynchronously
  DispatchEvent(*PopStateEvent::Create(std::move(state_object), history()));
}

void LocalDOMWindow::StatePopped(
    scoped_refptr<SerializedScriptValue> state_object) {
  if (!GetFrame())
    return;

  // Per step 11 of section 6.5.9 (history traversal) of the HTML5 spec, we
  // defer firing of popstate until we're in the complete state.
  if (document()->IsLoadCompleted())
    EnqueuePopstateEvent(std::move(state_object));
  else
    pending_state_object_ = std::move(state_object);
}

LocalDOMWindow::~LocalDOMWindow() = default;

void LocalDOMWindow::Dispose() {
  // Oilpan: should the LocalDOMWindow be GCed along with its LocalFrame without
  // the frame having first notified its observers of imminent destruction, the
  // LocalDOMWindow will not have had an opportunity to remove event listeners.
  //
  // Arrange for that removal to happen using a prefinalizer action. Making
  // LocalDOMWindow eager finalizable is problematic as other eagerly finalized
  // objects may well want to access their associated LocalDOMWindow from their
  // destructors.
  if (!GetFrame())
    return;

  RemoveAllEventListeners();
}

ExecutionContext* LocalDOMWindow::GetExecutionContext() const {
  return document_.Get();
}

const LocalDOMWindow* LocalDOMWindow::ToLocalDOMWindow() const {
  return this;
}

LocalDOMWindow* LocalDOMWindow::ToLocalDOMWindow() {
  return this;
}

MediaQueryList* LocalDOMWindow::matchMedia(const String& media) {
  return document() ? document()->GetMediaQueryMatcher().MatchMedia(media)
                    : nullptr;
}

void LocalDOMWindow::FrameDestroyed() {
  RemoveAllEventListeners();
  DisconnectFromFrame();
}

void LocalDOMWindow::RegisterEventListenerObserver(
    EventListenerObserver* event_listener_observer) {
  event_listener_observers_.insert(event_listener_observer);
}

void LocalDOMWindow::Reset() {
  DCHECK(document());
  DCHECK(document()->IsContextDestroyed());
  FrameDestroyed();

  screen_ = nullptr;
  history_ = nullptr;
  locationbar_ = nullptr;
  menubar_ = nullptr;
  personalbar_ = nullptr;
  scrollbars_ = nullptr;
  statusbar_ = nullptr;
  toolbar_ = nullptr;
  navigator_ = nullptr;
  media_ = nullptr;
  custom_elements_ = nullptr;
  application_cache_ = nullptr;
  trusted_types_ = nullptr;
}

void LocalDOMWindow::SendOrientationChangeEvent() {
  DCHECK(RuntimeEnabledFeatures::OrientationEventEnabled());
  DCHECK(GetFrame()->IsLocalRoot());

  // Before dispatching the event, build a list of all frames in the page
  // to send the event to, to mitigate side effects from event handlers
  // potentially interfering with others.
  HeapVector<Member<LocalFrame>> frames;
  frames.push_back(GetFrame());
  for (wtf_size_t i = 0; i < frames.size(); i++) {
    for (Frame* child = frames[i]->Tree().FirstChild(); child;
         child = child->Tree().NextSibling()) {
      if (auto* child_local_frame = DynamicTo<LocalFrame>(child))
        frames.push_back(child_local_frame);
    }
  }

  for (LocalFrame* frame : frames) {
    frame->DomWindow()->DispatchEvent(
        *Event::Create(event_type_names::kOrientationchange));
  }
}

int LocalDOMWindow::orientation() const {
  DCHECK(RuntimeEnabledFeatures::OrientationEventEnabled());

  LocalFrame* frame = GetFrame();
  if (!frame)
    return 0;

  ChromeClient& chrome_client = frame->GetChromeClient();
  int orientation = chrome_client.GetScreenInfo(*frame).orientation_angle;
  // For backward compatibility, we want to return a value in the range of
  // [-90; 180] instead of [0; 360[ because window.orientation used to behave
  // like that in WebKit (this is a WebKit proprietary API).
  if (orientation == 270)
    return -90;
  return orientation;
}

Screen* LocalDOMWindow::screen() const {
  if (!screen_)
    screen_ = MakeGarbageCollected<Screen>(GetFrame());
  return screen_.Get();
}

History* LocalDOMWindow::history() const {
  if (!history_)
    history_ = MakeGarbageCollected<History>(GetFrame());
  return history_.Get();
}

BarProp* LocalDOMWindow::locationbar() const {
  if (!locationbar_) {
    locationbar_ =
        MakeGarbageCollected<BarProp>(GetFrame(), BarProp::kLocationbar);
  }
  return locationbar_.Get();
}

BarProp* LocalDOMWindow::menubar() const {
  if (!menubar_)
    menubar_ = MakeGarbageCollected<BarProp>(GetFrame(), BarProp::kMenubar);
  return menubar_.Get();
}

BarProp* LocalDOMWindow::personalbar() const {
  if (!personalbar_) {
    personalbar_ =
        MakeGarbageCollected<BarProp>(GetFrame(), BarProp::kPersonalbar);
  }
  return personalbar_.Get();
}

BarProp* LocalDOMWindow::scrollbars() const {
  if (!scrollbars_) {
    scrollbars_ =
        MakeGarbageCollected<BarProp>(GetFrame(), BarProp::kScrollbars);
  }
  return scrollbars_.Get();
}

BarProp* LocalDOMWindow::statusbar() const {
  if (!statusbar_)
    statusbar_ = MakeGarbageCollected<BarProp>(GetFrame(), BarProp::kStatusbar);
  return statusbar_.Get();
}

BarProp* LocalDOMWindow::toolbar() const {
  if (!toolbar_)
    toolbar_ = MakeGarbageCollected<BarProp>(GetFrame(), BarProp::kToolbar);
  return toolbar_.Get();
}

FrameConsole* LocalDOMWindow::GetFrameConsole() const {
  if (!IsCurrentlyDisplayedInFrame())
    return nullptr;
  return &GetFrame()->Console();
}

ApplicationCache* LocalDOMWindow::applicationCache() const {
  if (!IsCurrentlyDisplayedInFrame())
    return nullptr;
  if (!isSecureContext()) {
    Deprecation::CountDeprecation(
        document(), WebFeature::kApplicationCacheAPIInsecureOrigin);
  }
  if (!application_cache_)
    application_cache_ = MakeGarbageCollected<ApplicationCache>(GetFrame());
  return application_cache_.Get();
}

Navigator* LocalDOMWindow::navigator() const {
  if (!navigator_)
    navigator_ = MakeGarbageCollected<Navigator>(GetFrame());
  return navigator_.Get();
}

void LocalDOMWindow::SchedulePostMessage(
    MessageEvent* event,
    scoped_refptr<const SecurityOrigin> target,
    Document* source) {
  // Allowing unbounded amounts of messages to build up for a suspended context
  // is problematic; consider imposing a limit or other restriction if this
  // surfaces often as a problem (see crbug.com/587012).
  std::unique_ptr<SourceLocation> location = SourceLocation::Capture(source);
  document_->GetTaskRunner(TaskType::kPostedMessage)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&LocalDOMWindow::DispatchPostMessage,
                           WrapPersistent(this), WrapPersistent(event),
                           std::move(target), std::move(location)));
  probe::AsyncTaskScheduled(document(), "postMessage", event->async_task_id());
}

void LocalDOMWindow::DispatchPostMessage(
    MessageEvent* event,
    scoped_refptr<const SecurityOrigin> intended_target_origin,
    std::unique_ptr<SourceLocation> location) {
  probe::AsyncTask async_task(document(), event->async_task_id());
  if (!IsCurrentlyDisplayedInFrame())
    return;

  event->EntangleMessagePorts(document());

  DispatchMessageEventWithOriginCheck(intended_target_origin.get(), event,
                                      std::move(location));
}

void LocalDOMWindow::DispatchMessageEventWithOriginCheck(
    const SecurityOrigin* intended_target_origin,
    MessageEvent* event,
    std::unique_ptr<SourceLocation> location) {
  if (intended_target_origin) {
    // Check target origin now since the target document may have changed since
    // the timer was scheduled.
    const SecurityOrigin* security_origin = document()->GetSecurityOrigin();
    bool valid_target =
        intended_target_origin->IsSameSchemeHostPort(security_origin);

    if (!valid_target) {
      String message = ExceptionMessages::FailedToExecute(
          "postMessage", "DOMWindow",
          "The target origin provided ('" + intended_target_origin->ToString() +
              "') does not match the recipient window's origin ('" +
              document()->GetSecurityOrigin()->ToString() + "').");
      ConsoleMessage* console_message = ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kSecurity,
          mojom::ConsoleMessageLevel::kError, message, std::move(location));
      GetFrameConsole()->AddMessage(console_message);
      return;
    }
  }

  KURL sender(event->origin());
  if (!document()->GetContentSecurityPolicy()->AllowConnectToSource(
          sender, RedirectStatus::kNoRedirect,
          SecurityViolationReportingPolicy::kSuppressReporting)) {
    UseCounter::Count(
        document(), WebFeature::kPostMessageIncomingWouldBeBlockedByConnectSrc);
  }

  if (event->IsOriginCheckRequiredToAccessData()) {
    scoped_refptr<SecurityOrigin> sender_security_origin =
        SecurityOrigin::Create(sender);

    const SecurityOrigin* target_security_origin =
        document()->GetSecurityOrigin();

    if (!sender_security_origin->IsSameSchemeHostPort(target_security_origin)) {
      event = MessageEvent::CreateError(event->origin(), event->source());
    }
  }
  DispatchEvent(*event);
}

DOMSelection* LocalDOMWindow::getSelection() {
  if (!IsCurrentlyDisplayedInFrame())
    return nullptr;

  return document()->GetSelection();
}

Element* LocalDOMWindow::frameElement() const {
  if (!GetFrame())
    return nullptr;

  return DynamicTo<HTMLFrameOwnerElement>(GetFrame()->Owner());
}

void LocalDOMWindow::blur() {}

void LocalDOMWindow::print(ScriptState* script_state) {
  if (!GetFrame())
    return;

  Page* page = GetFrame()->GetPage();
  if (!page)
    return;

  if (script_state &&
      v8::MicrotasksScope::IsRunningMicrotasks(script_state->GetIsolate())) {
    UseCounter::Count(document(), WebFeature::kDuring_Microtask_Print);
  }

  if (GetFrame()->IsLoading()) {
    should_print_when_finished_loading_ = true;
    return;
  }

  document()->CountUseOnlyInCrossOriginIframe(
      WebFeature::kCrossOriginWindowPrint);

  should_print_when_finished_loading_ = false;
  page->GetChromeClient().Print(GetFrame());
}

void LocalDOMWindow::stop() {
  if (!GetFrame())
    return;
  GetFrame()->Loader().StopAllLoaders();
}

void LocalDOMWindow::alert(ScriptState* script_state, const String& message) {
  if (!GetFrame())
    return;

  if (document()->IsSandboxed(WebSandboxFlags::kModals)) {
    UseCounter::Count(document(), WebFeature::kDialogInSandboxedContext);
    GetFrameConsole()->AddMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Ignored call to 'alert()'. The document is sandboxed, and the "
        "'allow-modals' keyword is not set."));
    return;
  }

  if (v8::MicrotasksScope::IsRunningMicrotasks(script_state->GetIsolate())) {
    UseCounter::Count(document(), WebFeature::kDuring_Microtask_Alert);
  }

  document()->UpdateStyleAndLayoutTree();

  Page* page = GetFrame()->GetPage();
  if (!page)
    return;

  document()->CountUseOnlyInCrossOriginIframe(
      WebFeature::kCrossOriginWindowAlert);

  page->GetChromeClient().OpenJavaScriptAlert(GetFrame(), message);
}

bool LocalDOMWindow::confirm(ScriptState* script_state, const String& message) {
  if (!GetFrame())
    return false;

  if (document()->IsSandboxed(WebSandboxFlags::kModals)) {
    UseCounter::Count(document(), WebFeature::kDialogInSandboxedContext);
    GetFrameConsole()->AddMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Ignored call to 'confirm()'. The document is sandboxed, and the "
        "'allow-modals' keyword is not set."));
    return false;
  }

  if (v8::MicrotasksScope::IsRunningMicrotasks(script_state->GetIsolate())) {
    UseCounter::Count(document(), WebFeature::kDuring_Microtask_Confirm);
  }

  document()->UpdateStyleAndLayoutTree();

  Page* page = GetFrame()->GetPage();
  if (!page)
    return false;

  document()->CountUseOnlyInCrossOriginIframe(
      WebFeature::kCrossOriginWindowConfirm);

  return page->GetChromeClient().OpenJavaScriptConfirm(GetFrame(), message);
}

String LocalDOMWindow::prompt(ScriptState* script_state,
                              const String& message,
                              const String& default_value) {
  if (!GetFrame())
    return String();

  if (document()->IsSandboxed(WebSandboxFlags::kModals)) {
    UseCounter::Count(document(), WebFeature::kDialogInSandboxedContext);
    GetFrameConsole()->AddMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Ignored call to 'prompt()'. The document is sandboxed, and the "
        "'allow-modals' keyword is not set."));
    return String();
  }

  if (v8::MicrotasksScope::IsRunningMicrotasks(script_state->GetIsolate())) {
    UseCounter::Count(document(), WebFeature::kDuring_Microtask_Prompt);
  }

  document()->UpdateStyleAndLayoutTree();

  Page* page = GetFrame()->GetPage();
  if (!page)
    return String();

  String return_value;
  if (page->GetChromeClient().OpenJavaScriptPrompt(GetFrame(), message,
                                                   default_value, return_value))
    return return_value;

  document()->CountUseOnlyInCrossOriginIframe(
      WebFeature::kCrossOriginWindowPrompt);

  return String();
}

bool LocalDOMWindow::find(const String& string,
                          bool case_sensitive,
                          bool backwards,
                          bool wrap,
                          bool whole_word,
                          bool /*searchInFrames*/,
                          bool /*showDialog*/) const {
  if (!IsCurrentlyDisplayedInFrame())
    return false;

  // Up-to-date, clean tree is required for finding text in page, since it
  // relies on TextIterator to look over the text.
  document()->UpdateStyleAndLayout();

  // FIXME (13016): Support searchInFrames and showDialog
  FindOptions options =
      (backwards ? kBackwards : 0) | (case_sensitive ? 0 : kCaseInsensitive) |
      (wrap ? kWrapAround : 0) | (whole_word ? kWholeWord : 0);
  return Editor::FindString(*GetFrame(), string, options);
}

bool LocalDOMWindow::offscreenBuffering() const {
  return true;
}

int LocalDOMWindow::outerHeight() const {
  if (!GetFrame())
    return 0;

  LocalFrame* frame = GetFrame();
  Page* page = frame->GetPage();
  if (!page)
    return 0;

  ChromeClient& chrome_client = page->GetChromeClient();
  if (page->GetSettings().GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(
        lroundf(chrome_client.RootWindowRect(*frame).Height() *
                chrome_client.GetScreenInfo(*frame).device_scale_factor));
  }
  return chrome_client.RootWindowRect(*frame).Height();
}

int LocalDOMWindow::outerWidth() const {
  if (!GetFrame())
    return 0;

  LocalFrame* frame = GetFrame();
  Page* page = frame->GetPage();
  if (!page)
    return 0;

  ChromeClient& chrome_client = page->GetChromeClient();
  if (page->GetSettings().GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(
        lroundf(chrome_client.RootWindowRect(*frame).Width() *
                chrome_client.GetScreenInfo(*frame).device_scale_factor));
  }
  return chrome_client.RootWindowRect(*frame).Width();
}

IntSize LocalDOMWindow::GetViewportSize() const {
  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return IntSize();

  Page* page = GetFrame()->GetPage();
  if (!page)
    return IntSize();

  // The main frame's viewport size depends on the page scale. If viewport is
  // enabled, the initial page scale depends on the content width and is set
  // after a layout, perform one now so queries during page load will use the
  // up to date viewport.
  if (page->GetSettings().GetViewportEnabled() && GetFrame()->IsMainFrame())
    document()->UpdateStyleAndLayout();

  // FIXME: This is potentially too much work. We really only need to know the
  // dimensions of the parent frame's layoutObject.
  if (Frame* parent = GetFrame()->Tree().Parent()) {
    if (auto* parent_local_frame = DynamicTo<LocalFrame>(parent))
      parent_local_frame->GetDocument()->UpdateStyleAndLayout();
  }

  return document()->View()->Size();
}

int LocalDOMWindow::innerHeight() const {
  if (!GetFrame())
    return 0;

  return AdjustForAbsoluteZoom::AdjustInt(GetViewportSize().Height(),
                                          GetFrame()->PageZoomFactor());
}

int LocalDOMWindow::innerWidth() const {
  if (!GetFrame())
    return 0;

  return AdjustForAbsoluteZoom::AdjustInt(GetViewportSize().Width(),
                                          GetFrame()->PageZoomFactor());
}

int LocalDOMWindow::screenX() const {
  LocalFrame* frame = GetFrame();
  if (!frame)
    return 0;

  Page* page = frame->GetPage();
  if (!page)
    return 0;

  ChromeClient& chrome_client = page->GetChromeClient();
  if (page->GetSettings().GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(
        lroundf(chrome_client.RootWindowRect(*frame).X() *
                chrome_client.GetScreenInfo(*frame).device_scale_factor));
  }
  return chrome_client.RootWindowRect(*frame).X();
}

int LocalDOMWindow::screenY() const {
  LocalFrame* frame = GetFrame();
  if (!frame)
    return 0;

  Page* page = frame->GetPage();
  if (!page)
    return 0;

  ChromeClient& chrome_client = page->GetChromeClient();
  if (page->GetSettings().GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(
        lroundf(chrome_client.RootWindowRect(*frame).Y() *
                chrome_client.GetScreenInfo(*frame).device_scale_factor));
  }
  return chrome_client.RootWindowRect(*frame).Y();
}

double LocalDOMWindow::scrollX() const {
  if (!GetFrame() || !GetFrame()->GetPage())
    return 0;

  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return 0;

  document()->UpdateStyleAndLayout();

  // TODO(bokan): This is wrong when the document.rootScroller is non-default.
  // crbug.com/505516.
  double viewport_x = view->LayoutViewport()->GetScrollOffset().Width();
  return AdjustForAbsoluteZoom::AdjustScroll(viewport_x,
                                             GetFrame()->PageZoomFactor());
}

double LocalDOMWindow::scrollY() const {
  if (!GetFrame() || !GetFrame()->GetPage())
    return 0;

  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return 0;

  document()->UpdateStyleAndLayout();

  // TODO(bokan): This is wrong when the document.rootScroller is non-default.
  // crbug.com/505516.
  double viewport_y = view->LayoutViewport()->GetScrollOffset().Height();
  return AdjustForAbsoluteZoom::AdjustScroll(viewport_y,
                                             GetFrame()->PageZoomFactor());
}

DOMVisualViewport* LocalDOMWindow::visualViewport() {
  return visualViewport_;
}

const AtomicString& LocalDOMWindow::name() const {
  if (!IsCurrentlyDisplayedInFrame())
    return g_null_atom;

  return GetFrame()->Tree().GetName();
}

void LocalDOMWindow::setName(const AtomicString& name) {
  if (!IsCurrentlyDisplayedInFrame())
    return;

  GetFrame()->Tree().SetName(name, FrameTree::kReplicate);
}

void LocalDOMWindow::setStatus(const String& string) {
  status_ = string;
}

void LocalDOMWindow::setDefaultStatus(const String& string) {
  default_status_ = string;
}

String LocalDOMWindow::origin() const {
  return GetExecutionContext()->GetSecurityOrigin()->ToString();
}

Document* LocalDOMWindow::document() const {
  return document_.Get();
}

StyleMedia* LocalDOMWindow::styleMedia() const {
  if (!media_)
    media_ = MakeGarbageCollected<StyleMedia>(GetFrame());
  return media_.Get();
}

CSSStyleDeclaration* LocalDOMWindow::getComputedStyle(
    Element* elt,
    const String& pseudo_elt) const {
  DCHECK(elt);
  return MakeGarbageCollected<CSSComputedStyleDeclaration>(elt, false,
                                                           pseudo_elt);
}

ScriptPromise LocalDOMWindow::getComputedAccessibleNode(
    ScriptState* script_state,
    Element* element) {
  DCHECK(element);
  auto* resolver = MakeGarbageCollected<ComputedAccessibleNodePromiseResolver>(
      script_state, *element);
  ScriptPromise promise = resolver->Promise();
  resolver->ComputeAccessibleNode();
  return promise;
}

double LocalDOMWindow::devicePixelRatio() const {
  if (!GetFrame())
    return 0.0;

  return GetFrame()->DevicePixelRatio();
}

void LocalDOMWindow::scrollBy(double x, double y) const {
  ScrollToOptions* options = ScrollToOptions::Create();
  options->setLeft(x);
  options->setTop(y);
  scrollBy(options);
}

void LocalDOMWindow::scrollBy(const ScrollToOptions* scroll_to_options) const {
  if (!IsCurrentlyDisplayedInFrame())
    return;

  document()->UpdateStyleAndLayout();

  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return;

  Page* page = GetFrame()->GetPage();
  if (!page)
    return;

  float x = 0.0f;
  float y = 0.0f;
  if (scroll_to_options->hasLeft()) {
    x = ScrollableArea::NormalizeNonFiniteScroll(
        base::saturated_cast<float>(scroll_to_options->left()));
  }
  if (scroll_to_options->hasTop()) {
    y = ScrollableArea::NormalizeNonFiniteScroll(
        base::saturated_cast<float>(scroll_to_options->top()));
  }

  PaintLayerScrollableArea* viewport = view->LayoutViewport();
  FloatPoint current_position = viewport->ScrollPosition();
  FloatPoint scaled_delta(x * GetFrame()->PageZoomFactor(),
                          y * GetFrame()->PageZoomFactor());
  FloatPoint new_scaled_position = current_position + scaled_delta;

  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndAndDirection(
          gfx::ScrollOffset(current_position), gfx::ScrollOffset(scaled_delta));
  new_scaled_position =
      viewport->GetSnapPositionAndSetTarget(*strategy).value_or(
          new_scaled_position);

  ScrollBehavior scroll_behavior = kScrollBehaviorAuto;
  ScrollableArea::ScrollBehaviorFromString(scroll_to_options->behavior(),
                                           scroll_behavior);
  viewport->SetScrollOffset(
      viewport->ScrollPositionToOffset(new_scaled_position),
      kProgrammaticScroll, scroll_behavior);
}

void LocalDOMWindow::scrollTo(double x, double y) const {
  ScrollToOptions* options = ScrollToOptions::Create();
  options->setLeft(x);
  options->setTop(y);
  scrollTo(options);
}

void LocalDOMWindow::scrollTo(const ScrollToOptions* scroll_to_options) const {
  if (!IsCurrentlyDisplayedInFrame())
    return;

  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return;

  Page* page = GetFrame()->GetPage();
  if (!page)
    return;

  // It is only necessary to have an up-to-date layout if the position may be
  // clamped, which is never the case for (0, 0).
  if (!scroll_to_options->hasLeft() || !scroll_to_options->hasTop() ||
      scroll_to_options->left() || scroll_to_options->top()) {
    document()->UpdateStyleAndLayout();
  }

  float scaled_x = 0.0f;
  float scaled_y = 0.0f;

  PaintLayerScrollableArea* viewport = view->LayoutViewport();
  ScrollOffset current_offset = viewport->GetScrollOffset();
  scaled_x = current_offset.Width();
  scaled_y = current_offset.Height();

  if (scroll_to_options->hasLeft()) {
    scaled_x = ScrollableArea::NormalizeNonFiniteScroll(
                   base::saturated_cast<float>(scroll_to_options->left())) *
               GetFrame()->PageZoomFactor();
  }

  if (scroll_to_options->hasTop()) {
    scaled_y = ScrollableArea::NormalizeNonFiniteScroll(
                   base::saturated_cast<float>(scroll_to_options->top())) *
               GetFrame()->PageZoomFactor();
  }

  FloatPoint new_scaled_position =
      viewport->ScrollOffsetToPosition(ScrollOffset(scaled_x, scaled_y));

  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(
          gfx::ScrollOffset(new_scaled_position), scroll_to_options->hasLeft(),
          scroll_to_options->hasTop());
  new_scaled_position =
      viewport->GetSnapPositionAndSetTarget(*strategy).value_or(
          new_scaled_position);
  ScrollBehavior scroll_behavior = kScrollBehaviorAuto;
  ScrollableArea::ScrollBehaviorFromString(scroll_to_options->behavior(),
                                           scroll_behavior);
  viewport->SetScrollOffset(
      viewport->ScrollPositionToOffset(new_scaled_position),
      kProgrammaticScroll, scroll_behavior);
}

void LocalDOMWindow::moveBy(int x, int y) const {
  if (!GetFrame() || !GetFrame()->IsMainFrame())
    return;

  LocalFrame* frame = GetFrame();
  Page* page = frame->GetPage();
  if (!page)
    return;

  IntRect window_rect = page->GetChromeClient().RootWindowRect(*frame);
  window_rect.SaturatedMove(x, y);
  // Security check (the spec talks about UniversalBrowserWrite to disable this
  // check...)
  page->GetChromeClient().SetWindowRectWithAdjustment(window_rect, *frame);
}

void LocalDOMWindow::moveTo(int x, int y) const {
  if (!GetFrame() || !GetFrame()->IsMainFrame())
    return;

  LocalFrame* frame = GetFrame();
  Page* page = frame->GetPage();
  if (!page)
    return;

  IntRect window_rect = page->GetChromeClient().RootWindowRect(*frame);
  window_rect.SetLocation(IntPoint(x, y));
  // Security check (the spec talks about UniversalBrowserWrite to disable this
  // check...)
  page->GetChromeClient().SetWindowRectWithAdjustment(window_rect, *frame);
}

void LocalDOMWindow::resizeBy(int x, int y) const {
  if (!GetFrame() || !GetFrame()->IsMainFrame())
    return;

  LocalFrame* frame = GetFrame();
  Page* page = frame->GetPage();
  if (!page)
    return;

  IntRect fr = page->GetChromeClient().RootWindowRect(*frame);
  IntSize dest = fr.Size() + IntSize(x, y);
  IntRect update(fr.Location(), dest);
  page->GetChromeClient().SetWindowRectWithAdjustment(update, *frame);
}

void LocalDOMWindow::resizeTo(int width, int height) const {
  if (!GetFrame() || !GetFrame()->IsMainFrame())
    return;

  LocalFrame* frame = GetFrame();
  Page* page = frame->GetPage();
  if (!page)
    return;

  IntRect fr = page->GetChromeClient().RootWindowRect(*frame);
  IntSize dest = IntSize(width, height);
  IntRect update(fr.Location(), dest);
  page->GetChromeClient().SetWindowRectWithAdjustment(update, *frame);
}

int LocalDOMWindow::requestAnimationFrame(V8FrameRequestCallback* callback) {
  if (Document* doc = document()) {
    auto* frame_callback =
        MakeGarbageCollected<FrameRequestCallbackCollection::V8FrameCallback>(
            callback);
    frame_callback->SetUseLegacyTimeBase(false);
    return doc->RequestAnimationFrame(frame_callback);
  }
  return 0;
}

int LocalDOMWindow::webkitRequestAnimationFrame(
    V8FrameRequestCallback* callback) {
  if (Document* doc = document()) {
    auto* frame_callback =
        MakeGarbageCollected<FrameRequestCallbackCollection::V8FrameCallback>(
            callback);
    frame_callback->SetUseLegacyTimeBase(true);
    return doc->RequestAnimationFrame(frame_callback);
  }
  return 0;
}

void LocalDOMWindow::cancelAnimationFrame(int id) {
  if (Document* document = this->document())
    document->CancelAnimationFrame(id);
}

int LocalDOMWindow::requestPostAnimationFrame(
    V8FrameRequestCallback* callback) {
  if (Document* doc = document()) {
    FrameRequestCallbackCollection::V8FrameCallback* frame_callback =
        MakeGarbageCollected<FrameRequestCallbackCollection::V8FrameCallback>(
            callback);
    return doc->RequestPostAnimationFrame(frame_callback);
  }
  return 0;
}

void LocalDOMWindow::cancelPostAnimationFrame(int id) {
  if (Document* doc = this->document())
    doc->CancelPostAnimationFrame(id);
}

void LocalDOMWindow::queueMicrotask(V8VoidFunction* callback) {
  Microtask::EnqueueMicrotask(
      WTF::Bind(&V8VoidFunction::InvokeAndReportException,
                WrapPersistent(callback), nullptr));
}

int LocalDOMWindow::requestIdleCallback(V8IdleRequestCallback* callback,
                                        const IdleRequestOptions* options) {
  if (Document* document = this->document()) {
    return document->RequestIdleCallback(
        ScriptedIdleTaskController::V8IdleTask::Create(callback), options);
  }
  return 0;
}

void LocalDOMWindow::cancelIdleCallback(int id) {
  if (Document* document = this->document())
    document->CancelIdleCallback(id);
}

CustomElementRegistry* LocalDOMWindow::customElements(
    ScriptState* script_state) const {
  if (!script_state->World().IsMainWorld())
    return nullptr;
  return customElements();
}

CustomElementRegistry* LocalDOMWindow::customElements() const {
  if (!custom_elements_ && document_)
    custom_elements_ = MakeGarbageCollected<CustomElementRegistry>(this);
  return custom_elements_;
}

CustomElementRegistry* LocalDOMWindow::MaybeCustomElements() const {
  return custom_elements_;
}

void LocalDOMWindow::SetModulator(Modulator* modulator) {
  DCHECK(!modulator_);
  modulator_ = modulator;
}

External* LocalDOMWindow::external() {
  if (!external_)
    external_ = MakeGarbageCollected<External>();
  return external_;
}

bool LocalDOMWindow::isSecureContext() const {
  if (!GetFrame())
    return false;

  return document()->IsSecureContext();
}

void LocalDOMWindow::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  DOMWindow::AddedEventListener(event_type, registered_listener);
  if (auto* frame = GetFrame()) {
    frame->GetEventHandlerRegistry().DidAddEventHandler(
        *this, event_type, registered_listener.Options());
  }

  if (Document* document = this->document())
    document->AddListenerTypeIfNeeded(event_type, *this);

  for (auto& it : event_listener_observers_) {
    it->DidAddEventListener(this, event_type);
  }

  if (event_type == event_type_names::kUnload) {
    UseCounter::Count(document(), WebFeature::kDocumentUnloadRegistered);
    TrackUnloadEventListener(this);
  } else if (event_type == event_type_names::kBeforeunload) {
    UseCounter::Count(document(), WebFeature::kDocumentBeforeUnloadRegistered);
    TrackBeforeUnloadEventListener(this);
    if (GetFrame() && !GetFrame()->IsMainFrame()) {
      UseCounter::Count(document(),
                        WebFeature::kSubFrameBeforeUnloadRegistered);
    }
  } else if (event_type == event_type_names::kPagehide) {
    UseCounter::Count(document(), WebFeature::kDocumentPageHideRegistered);
  } else if (event_type == event_type_names::kPageshow) {
    UseCounter::Count(document(), WebFeature::kDocumentPageShowRegistered);
  }
}

void LocalDOMWindow::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  DOMWindow::RemovedEventListener(event_type, registered_listener);
  if (auto* frame = GetFrame()) {
    frame->GetEventHandlerRegistry().DidRemoveEventHandler(
        *this, event_type, registered_listener.Options());
  }

  for (auto& it : event_listener_observers_) {
    it->DidRemoveEventListener(this, event_type);
  }

  if (event_type == event_type_names::kUnload) {
    UntrackUnloadEventListener(this);
  } else if (event_type == event_type_names::kBeforeunload) {
    UntrackBeforeUnloadEventListener(this);
  }
}

void LocalDOMWindow::WarnUnusedPreloads(TimerBase* base) {
  if (!document() || !document()->Fetcher())
    return;
  Vector<KURL> urls = document()->Fetcher()->GetUrlsOfUnusedPreloads();
  for (const KURL& url : urls) {
    String message =
        "The resource " + url.GetString() + " was preloaded using link " +
        "preload but not used within a few seconds from the window's load " +
        "event. Please make sure it has an appropriate `as` value and it is " +
        "preloaded intentionally.";
    GetFrameConsole()->AddMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kWarning, message));
  }
}

void LocalDOMWindow::DispatchLoadEvent() {
  Event& load_event = *Event::Create(event_type_names::kLoad);
  DocumentLoader* document_loader =
      GetFrame() ? GetFrame()->Loader().GetDocumentLoader() : nullptr;
  if (document_loader &&
      document_loader->GetTiming().LoadEventStart().is_null()) {
    DocumentLoadTiming& timing = document_loader->GetTiming();
    timing.MarkLoadEventStart();
    DispatchEvent(load_event, document());
    timing.MarkLoadEventEnd();
    // If fetcher->countPreloads() is not empty here, it's full of link
    // preloads, as speculatove preloads were cleared at DCL.
    if (GetFrame() &&
        document_loader == GetFrame()->Loader().GetDocumentLoader() &&
        document()->Fetcher()->CountPreloads()) {
      unused_preloads_timer_.StartOneShot(kUnusedPreloadTimeout, FROM_HERE);
    }
  } else {
    DispatchEvent(load_event, document());
  }

  if (GetFrame()) {
    WindowPerformance* performance = DOMWindowPerformance::performance(*this);
    DCHECK(performance);
    performance->NotifyNavigationTimingToObservers();
  }

  // For load events, send a separate load event to the enclosing frame only.
  // This is a DOM extension and is independent of bubbling/capturing rules of
  // the DOM.
  FrameOwner* owner = GetFrame() ? GetFrame()->Owner() : nullptr;
  if (owner)
    owner->DispatchLoad();

  TRACE_EVENT_INSTANT1("devtools.timeline", "MarkLoad",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_mark_load_event::Data(GetFrame()));
  probe::LoadEventFired(GetFrame());
}

DispatchEventResult LocalDOMWindow::DispatchEvent(Event& event,
                                                  EventTarget* target) {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif

  event.SetTrusted(true);
  event.SetTarget(target ? target : this);
  event.SetCurrentTarget(this);
  event.SetEventPhase(Event::kAtTarget);

  TRACE_EVENT1("devtools.timeline", "EventDispatch", "data",
               inspector_event_dispatch_event::Data(event));
  return FireEventListeners(event);
}

void LocalDOMWindow::RemoveAllEventListeners() {
  EventTarget::RemoveAllEventListeners();

  for (auto& it : event_listener_observers_) {
    it->DidRemoveAllEventListeners(this);
  }

  if (GetFrame())
    GetFrame()->GetEventHandlerRegistry().DidRemoveAllEventHandlers(*this);

  UntrackAllUnloadEventListeners(this);
  UntrackAllBeforeUnloadEventListeners(this);
}

void LocalDOMWindow::FinishedLoading(FrameLoader::NavigationFinishState state) {
  bool was_should_print_when_finished_loading =
      should_print_when_finished_loading_;
  should_print_when_finished_loading_ = false;

  if (was_should_print_when_finished_loading &&
      state == FrameLoader::NavigationFinishState::kSuccess) {
    print(nullptr);
  }
}

void LocalDOMWindow::PrintErrorMessage(const String& message) const {
  if (!IsCurrentlyDisplayedInFrame())
    return;

  if (message.IsEmpty())
    return;

  GetFrameConsole()->AddMessage(
      ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                             mojom::ConsoleMessageLevel::kError, message));
}

DOMWindow* LocalDOMWindow::open(v8::Isolate* isolate,
                                const String& url_string,
                                const AtomicString& target,
                                const String& features,
                                ExceptionState& exception_state) {
  LocalDOMWindow* incumbent_window = IncumbentDOMWindow(isolate);
  LocalDOMWindow* entered_window = EnteredDOMWindow(isolate);

  // If the bindings implementation is 100% correct, the current realm and the
  // entered realm should be same origin-domain. However, to be on the safe
  // side and add some defense in depth, we'll check against the entry realm
  // as well here.
  if (!BindingSecurity::ShouldAllowAccessTo(entered_window, this,
                                            exception_state)) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kWindowOpenRealmMismatch);
    return nullptr;
  }

  if (!IsCurrentlyDisplayedInFrame())
    return nullptr;
  if (!incumbent_window->GetFrame())
    return nullptr;
  Document* active_document = incumbent_window->document();
  if (!active_document)
    return nullptr;
  LocalFrame* entered_window_frame = entered_window->GetFrame();
  if (!entered_window_frame)
    return nullptr;

  UseCounter::Count(*active_document, WebFeature::kDOMWindowOpen);
  if (!features.IsEmpty())
    UseCounter::Count(*active_document, WebFeature::kDOMWindowOpenFeatures);

  KURL completed_url =
      url_string.IsEmpty()
          ? KURL(g_empty_string)
          : entered_window_frame->GetDocument()->CompleteURL(url_string);
  if (!completed_url.IsEmpty() && !completed_url.IsValid()) {
    UseCounter::Count(active_document, WebFeature::kWindowOpenWithInvalidURL);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Unable to open a window with invalid URL '" +
            completed_url.GetString() + "'.\n");
    return nullptr;
  }

  WebWindowFeatures window_features = GetWindowFeaturesFromString(features);

  FrameLoadRequest frame_request(active_document,
                                 ResourceRequest(completed_url));
  frame_request.SetFeaturesForWindowOpen(window_features);

  // Normally, FrameLoader would take care of setting the referrer for a
  // navigation that is triggered from javascript. However, creating a window
  // goes through sufficient processing that it eventually enters FrameLoader as
  // an embedder-initiated navigation.  FrameLoader assumes no responsibility
  // for generating an embedder-initiated navigation's referrer, so we need to
  // ensure the proper referrer is set now.
  // TODO(domfarolino): Stop setting ResourceRequest's HTTP Referrer and store
  // this is a separate member. See https://crbug.com/850813.
  const SecurityOrigin* origin =
      active_document ? active_document->GetSecurityOrigin()
                      : SecurityOrigin::Create(completed_url).get();
  frame_request.GetResourceRequest().SetHttpReferrer(
      SecurityPolicy::GenerateReferrer(
          active_document->GetReferrerPolicy(), origin, completed_url,
          window_features.noreferrer ? Referrer::NoReferrer()
                                     : active_document->OutgoingReferrer()));

  frame_request.GetResourceRequest().SetHasUserGesture(
      LocalFrame::HasTransientUserActivation(GetFrame()));
  GetFrame()->MaybeLogAdClickNavigation();

  FrameTree::FindResult result =
      GetFrame()->Tree().FindOrCreateFrameForNavigation(
          frame_request, target.IsEmpty() ? "_blank" : target);
  if (!result.frame)
    return nullptr;

  if (window_features.x_set || window_features.y_set) {
    // This runs after FindOrCreateFrameForNavigation() so blocked popups are
    // not counted.
    UseCounter::Count(*active_document,
                      WebFeature::kDOMWindowOpenPositioningFeatures);
  }

  if (!completed_url.IsEmpty() || result.new_window)
    result.frame->Navigate(frame_request, WebFrameLoadType::kStandard);

  // TODO(japhet): window-open-noopener.html?_top and several tests in
  // html/browsers/windows/browsing-context-names/ appear to require that
  // the special case target names (_top, _parent, _self) ignore opener
  // policy (by always returning a non-null window, and by never overriding
  // the opener). The spec doesn't mention this.
  if (EqualIgnoringASCIICase(target, "_top") ||
      EqualIgnoringASCIICase(target, "_parent") ||
      EqualIgnoringASCIICase(target, "_self")) {
    return result.frame->DomWindow();
  }

  if (window_features.noopener)
    return nullptr;
  if (!result.new_window)
    result.frame->Client()->SetOpener(GetFrame());
  return result.frame->DomWindow();
}

void LocalDOMWindow::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  visitor->Trace(screen_);
  visitor->Trace(history_);
  visitor->Trace(locationbar_);
  visitor->Trace(menubar_);
  visitor->Trace(personalbar_);
  visitor->Trace(scrollbars_);
  visitor->Trace(statusbar_);
  visitor->Trace(toolbar_);
  visitor->Trace(navigator_);
  visitor->Trace(media_);
  visitor->Trace(custom_elements_);
  visitor->Trace(modulator_);
  visitor->Trace(external_);
  visitor->Trace(application_cache_);
  visitor->Trace(visualViewport_);
  visitor->Trace(event_listener_observers_);
  visitor->Trace(trusted_types_);
  DOMWindow::Trace(visitor);
  Supplementable<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
