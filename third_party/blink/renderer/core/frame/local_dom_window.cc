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
#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "cc/input/snap_selection_strategy.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/storage_access_api/status.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_disposition.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_picture_in_picture_window_options.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_to_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/dom_window_css.h"
#include "third_party/blink/renderer/core/css/media_query_list.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_media.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/frame_request_callback_collection.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_controller.h"
#include "third_party/blink/renderer/core/events/hash_change_event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/events/page_transition_event.h"
#include "third_party/blink/renderer/core/events/pop_state_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/bar_prop.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/document_policy_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/dom_viewport.h"
#include "third_party/blink/renderer/core/frame/dom_visual_viewport.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/external.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/history.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/permissions_policy_violation_report_body.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fence.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/create_window.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/sync_scroll_attempt_heuristic.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/platform/back_forward_cache_buffer_limit_tracker.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"
#include "ui/display/screen_info.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
bool IsRunningMicrotasks(ScriptState* script_state) {
  if (auto* microtask_queue = ToMicrotaskQueue(script_state))
    return microtask_queue->IsRunningMicrotasks();
  return v8::MicrotasksScope::IsRunningMicrotasks(script_state->GetIsolate());
}

void SetCurrentTaskAsCallbackParent(
    CallbackFunctionWithTaskAttributionBase* callback) {
  ScriptState* script_state = callback->CallbackRelevantScriptState();
  auto* tracker =
      scheduler::TaskAttributionTracker::From(script_state->GetIsolate());
  if (tracker && script_state->World().IsMainWorld()) {
    callback->SetParentTask(tracker->RunningTask());
  }
}

int RequestAnimationFrame(Document* document,
                          V8FrameRequestCallback* callback,
                          bool legacy) {
  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidRequestAnimationFrame();
  SetCurrentTaskAsCallbackParent(callback);
  auto* frame_callback = MakeGarbageCollected<V8FrameCallback>(callback);
  frame_callback->SetUseLegacyTimeBase(legacy);
  return document->RequestAnimationFrame(frame_callback);
}

}  // namespace

class LocalDOMWindow::NetworkStateObserver final
    : public GarbageCollected<LocalDOMWindow::NetworkStateObserver>,
      public NetworkStateNotifier::NetworkStateObserver,
      public ExecutionContextLifecycleObserver {
 public:
  explicit NetworkStateObserver(ExecutionContext* context)
      : ExecutionContextLifecycleObserver(context) {}

  void Initialize() {
    online_observer_handle_ = GetNetworkStateNotifier().AddOnLineObserver(
        this, GetExecutionContext()->GetTaskRunner(TaskType::kNetworking));
  }

  void OnLineStateChange(bool on_line) override {
    AtomicString event_name =
        on_line ? event_type_names::kOnline : event_type_names::kOffline;
    auto* window = To<LocalDOMWindow>(GetExecutionContext());
    window->DispatchEvent(*Event::Create(event_name));
  }

  void ContextDestroyed() override { online_observer_handle_ = nullptr; }

  void Trace(Visitor* visitor) const override {
    ExecutionContextLifecycleObserver::Trace(visitor);
  }

 private:
  std::unique_ptr<NetworkStateNotifier::NetworkStateObserverHandle>
      online_observer_handle_;
};

LocalDOMWindow::LocalDOMWindow(LocalFrame& frame, WindowAgent* agent)
    : DOMWindow(frame),
      ExecutionContext(agent->isolate(),
                       agent,
                       /*Same value as IsWindow(). is_window=*/true),
      script_controller_(MakeGarbageCollected<ScriptController>(
          *this,
          *static_cast<LocalWindowProxyManager*>(
              frame.GetWindowProxyManager()))),
      viewport_(MakeGarbageCollected<DOMViewport>(this)),
      visualViewport_(MakeGarbageCollected<DOMVisualViewport>(this)),
      should_print_when_finished_loading_(false),
      input_method_controller_(
          MakeGarbageCollected<InputMethodController>(*this, frame)),
      spell_checker_(MakeGarbageCollected<SpellChecker>(*this)),
      text_suggestion_controller_(
          MakeGarbageCollected<TextSuggestionController>(*this)),
      isolated_world_csp_map_(
          MakeGarbageCollected<
              HeapHashMap<int, Member<ContentSecurityPolicy>>>()),
      token_(frame.GetLocalFrameToken()),
      network_state_observer_(MakeGarbageCollected<NetworkStateObserver>(this)),
      closewatcher_stack_(
          MakeGarbageCollected<CloseWatcher::WatcherStack>(this)),
      navigation_id_(WTF::CreateCanonicalUUIDString()) {}

void LocalDOMWindow::BindContentSecurityPolicy() {
  DCHECK(!GetContentSecurityPolicy()->IsBound());
  GetContentSecurityPolicy()->BindToDelegate(
      GetContentSecurityPolicyDelegate());
}

void LocalDOMWindow::Initialize() {
  GetAgent()->AttachContext(this);
  network_state_observer_->Initialize();
}

void LocalDOMWindow::ClearForReuse() {
  is_dom_window_reused_ = true;
  // update event listener counts before clearing document_
  if (document_ && HasEventListeners()) {
    GetEventTargetData()->event_listener_map.ForAllEventListenerTypes(
        [this](const AtomicString& event_type, uint32_t count) {
          document_->DidRemoveEventListeners(count);
        });
  }
  document_ = nullptr;
}

void LocalDOMWindow::ResetWindowAgent(WindowAgent* agent) {
  GetAgent()->DetachContext(this);
  ResetAgent(agent);
  if (document_) {
    document_->ResetAgent(*agent);
  }

  CHECK(GetFrame());
  GetFrame()->GetFrameScheduler()->SetAgentClusterId(GetAgentClusterID());

  // This is only called on Android WebView, we need to reassign the microtask
  // queue if there already is one for the associated context. There shouldn't
  // be any other worlds with Android WebView so using the MainWorld is fine.
  auto* microtask_queue = agent->event_loop()->microtask_queue();
  if (microtask_queue) {
    v8::HandleScope handle_scope(GetIsolate());
    v8::Local<v8::Context> main_world_context = ToV8ContextMaybeEmpty(
        GetFrame(), DOMWrapperWorld::MainWorld(GetIsolate()));
    if (!main_world_context.IsEmpty())
      main_world_context->SetMicrotaskQueue(microtask_queue);
  }

  GetAgent()->AttachContext(this);
}

void LocalDOMWindow::AcceptLanguagesChanged() {
  if (navigator_)
    navigator_->SetLanguagesDirty();

  DispatchEvent(*Event::Create(event_type_names::kLanguagechange));
}

ScriptValue LocalDOMWindow::event(ScriptState* script_state) {
  // If current event is null, return undefined.
  if (!current_event_) {
    return ScriptValue(script_state->GetIsolate(),
                       v8::Undefined(script_state->GetIsolate()));
  }

  return ScriptValue(script_state->GetIsolate(),
                     ToV8Traits<Event>::ToV8(script_state, CurrentEvent()));
}

Event* LocalDOMWindow::CurrentEvent() const {
  return current_event_.Get();
}

void LocalDOMWindow::SetCurrentEvent(Event* new_event) {
  current_event_ = new_event;
}

TrustedTypePolicyFactory* LocalDOMWindow::GetTrustedTypesForWorld(
    const DOMWrapperWorld& world) const {
  DCHECK(world.IsMainWorld() || world.IsIsolatedWorld());
  DCHECK(IsMainThread());
  auto iter = trusted_types_map_.find(&world);
  if (iter != trusted_types_map_.end())
    return iter->value.Get();
  return trusted_types_map_
      .insert(&world, MakeGarbageCollected<TrustedTypePolicyFactory>(
                          GetExecutionContext()))
      .stored_value->value;
}

TrustedTypePolicyFactory* LocalDOMWindow::trustedTypes(
    ScriptState* script_state) const {
  return GetTrustedTypesForWorld(script_state->World());
}

bool LocalDOMWindow::IsCrossSiteSubframe() const {
  if (!GetFrame())
    return false;
  if (GetFrame()->IsInFencedFrameTree())
    return true;
  // It'd be nice to avoid the url::Origin temporaries, but that would require
  // exposing the net internal helper.
  // TODO: If the helper gets exposed, we could do this without any new
  // allocations using StringUTF8Adaptor.
  auto* top_origin =
      GetFrame()->Tree().Top().GetSecurityContext()->GetSecurityOrigin();
  return !net::registry_controlled_domains::SameDomainOrHost(
      top_origin->ToUrlOrigin(), GetSecurityOrigin()->ToUrlOrigin(),
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool LocalDOMWindow::IsCrossSiteSubframeIncludingScheme() const {
  if (!GetFrame())
    return false;
  if (GetFrame()->IsInFencedFrameTree())
    return true;
  return top()->GetFrame() &&
         !top()
              ->GetFrame()
              ->GetSecurityContext()
              ->GetSecurityOrigin()
              ->IsSameSiteWith(GetSecurityContext().GetSecurityOrigin());
}

LocalDOMWindow* LocalDOMWindow::From(const ScriptState* script_state) {
  return blink::ToLocalDOMWindow(script_state);
}

mojom::blink::V8CacheOptions LocalDOMWindow::GetV8CacheOptions() const {
  if (LocalFrame* frame = GetFrame()) {
    if (const Settings* settings = frame->GetSettings())
      return settings->GetV8CacheOptions();
  }

  return mojom::blink::V8CacheOptions::kDefault;
}

bool LocalDOMWindow::IsContextThread() const {
  return IsMainThread();
}

bool LocalDOMWindow::ShouldInstallV8Extensions() const {
  return GetFrame()->Client()->AllowScriptExtensions();
}

ContentSecurityPolicy* LocalDOMWindow::GetContentSecurityPolicyForWorld(
    const DOMWrapperWorld* world) {
  if (!world || !world->IsIsolatedWorld())
    return GetContentSecurityPolicy();

  int32_t world_id = world->GetWorldId();
  auto it = isolated_world_csp_map_->find(world_id);
  if (it != isolated_world_csp_map_->end())
    return it->value.Get();

  ContentSecurityPolicy* policy =
      IsolatedWorldCSP::Get().CreateIsolatedWorldCSP(*this, world_id);
  if (!policy)
    return GetContentSecurityPolicy();

  isolated_world_csp_map_->insert(world_id, policy);
  return policy;
}

const KURL& LocalDOMWindow::Url() const {
  return document()->Url();
}

const KURL& LocalDOMWindow::BaseURL() const {
  return document()->BaseURL();
}

KURL LocalDOMWindow::CompleteURL(const String& url) const {
  return document()->CompleteURL(url);
}

void LocalDOMWindow::DisableEval(const String& error_message) {
  GetScriptController().DisableEval(error_message);
}

void LocalDOMWindow::SetWasmEvalErrorMessage(const String& error_message) {
  GetScriptController().SetWasmEvalErrorMessage(error_message);
}

String LocalDOMWindow::UserAgent() const {
  if (!GetFrame())
    return String();

  return GetFrame()->Loader().UserAgent();
}

UserAgentMetadata LocalDOMWindow::GetUserAgentMetadata() const {
  return GetFrame()->Loader().UserAgentMetadata().value_or(
      blink::UserAgentMetadata());
}

HttpsState LocalDOMWindow::GetHttpsState() const {
  // TODO(https://crbug.com/880986): Implement Document's HTTPS state in more
  // spec-conformant way.
  return CalculateHttpsState(GetSecurityOrigin());
}

ResourceFetcher* LocalDOMWindow::Fetcher() {
  return document()->Fetcher();
}

bool LocalDOMWindow::CanExecuteScripts(
    ReasonForCallingCanExecuteScripts reason) {
  if (!GetFrame()) {
    return false;
  }

  // Detached frames should not be attempting to execute script.
  DCHECK(!GetFrame()->IsDetached());

  // Normally, scripts are not allowed in sandboxed contexts that disallow them.
  // However, there is an exception for cases when the script should bypass the
  // main world's CSP (such as for privileged isolated worlds). See
  // https://crbug.com/811528.
  if (IsSandboxed(network::mojom::blink::WebSandboxFlags::kScripts) &&
      !ContentSecurityPolicy::ShouldBypassMainWorldDeprecated(this)) {
    // FIXME: This message should be moved off the console once a solution to
    // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    if (reason == kAboutToExecuteScript) {
      AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kSecurity,
          mojom::blink::ConsoleMessageLevel::kError,
          "Blocked script execution in '" + Url().ElidedString() +
              "' because the document's frame is sandboxed and the "
              "'allow-scripts' permission is not set."));
    }
    return false;
  }
  bool script_enabled = GetFrame()->ScriptEnabled();
  if (!script_enabled && reason == kAboutToExecuteScript) {
    WebContentSettingsClient* settings_client =
        GetFrame()->GetContentSettingsClient();
    if (settings_client) {
      settings_client->DidNotAllowScript();
    }
  }
  return script_enabled;
}

String LocalDOMWindow::CheckAndGetJavascriptUrl(
    const DOMWrapperWorld* world,
    const KURL& url,
    Element* element,
    network::mojom::CSPDisposition csp_disposition) {
  const int kJavascriptSchemeLength = sizeof("javascript:") - 1;
  String decoded_url = DecodeURLEscapeSequences(
      url.GetString(), DecodeURLMode::kUTF8OrIsomorphic);
  String script_source = decoded_url.Substring(kJavascriptSchemeLength);

  if (csp_disposition == network::mojom::CSPDisposition::DO_NOT_CHECK)
    return script_source;

  // Check the CSP of the caller (the "source browsing context") if required,
  // as per https://html.spec.whatwg.org/C/#javascript-protocol.
  if (!GetContentSecurityPolicyForWorld(world)->AllowInline(
          ContentSecurityPolicy::InlineType::kNavigation, element, decoded_url,
          String() /* nonce */, Url(), OrdinalNumber::First()))
    return String();

  // TODO(crbug.com/896041): Investigate how trusted type checks can be
  // implemented for isolated worlds.
  if (ContentSecurityPolicy::ShouldBypassMainWorldDeprecated(world))
    return script_source;

  // https://w3c.github.io/trusted-types/dist/spec/#require-trusted-types-for-pre-navigation-check
  // 4.9.1.1. require-trusted-types-for Pre-Navigation check
  script_source =
      TrustedTypesCheckForJavascriptURLinNavigation(script_source, this);

  return script_source;
}

void LocalDOMWindow::ExceptionThrown(ErrorEvent* event) {
  MainThreadDebugger::Instance(GetIsolate())->ExceptionThrown(this, event);
}

// https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
String LocalDOMWindow::OutgoingReferrer() const {
  // Step 3.1: "If environment's global object is a Window object, then"
  // Step 3.1.1: "Let document be the associated Document of environment's
  // global object."

  // Step 3.1.2: "If document's origin is an opaque origin, return no referrer."
  if (GetSecurityOrigin()->IsOpaque())
    return String();

  // Step 3.1.3: "While document is an iframe srcdoc document, let document be
  // document's browsing context's browsing context container's node document."
  Document* referrer_document = document();
  if (LocalFrame* frame = GetFrame()) {
    while (frame->GetDocument()->IsSrcdocDocument()) {
      // Srcdoc documents must be local within the containing frame.
      frame = To<LocalFrame>(frame->Tree().Parent());
      // Srcdoc documents cannot be top-level documents, by definition,
      // because they need to be contained in iframes with the srcdoc.
      DCHECK(frame);
    }
    referrer_document = frame->GetDocument();
  }

  // Step: 3.1.4: "Let referrerSource be document's URL."
  return referrer_document->Url().StrippedForUseAsReferrer();
}

CoreProbeSink* LocalDOMWindow::GetProbeSink() {
  return probe::ToCoreProbeSink(GetFrame());
}

const BrowserInterfaceBrokerProxy& LocalDOMWindow::GetBrowserInterfaceBroker()
    const {
  if (!GetFrame())
    return GetEmptyBrowserInterfaceBroker();

  return GetFrame()->GetBrowserInterfaceBroker();
}

FrameOrWorkerScheduler* LocalDOMWindow::GetScheduler() {
  if (GetFrame())
    return GetFrame()->GetFrameScheduler();
  if (!detached_scheduler_)
    detached_scheduler_ = scheduler::CreateDummyFrameScheduler(GetIsolate());
  return detached_scheduler_.get();
}

scoped_refptr<base::SingleThreadTaskRunner> LocalDOMWindow::GetTaskRunner(
    TaskType type) {
  if (GetFrame())
    return GetFrame()->GetTaskRunner(type);
  TRACE_EVENT_INSTANT("blink",
                      "LocalDOMWindow::GetTaskRunner_ThreadTaskRunner");
  // In most cases, the ExecutionContext will get us to a relevant Frame. In
  // some cases, though, there isn't a good candidate (most commonly when either
  // the passed-in document or the ExecutionContext used to be attached to a
  // Frame but has since been detached) so we will use the default task runner
  // of the AgentGroupScheduler that created this window.
  return To<WindowAgent>(GetAgent())
      ->GetAgentGroupScheduler()
      .DefaultTaskRunner();
}

void LocalDOMWindow::ReportPermissionsPolicyViolation(
    mojom::blink::PermissionsPolicyFeature feature,
    mojom::blink::PolicyDisposition disposition,
    const std::optional<String>& reporting_endpoint,
    const String& message) const {
  if (disposition == mojom::blink::PolicyDisposition::kEnforce) {
    const_cast<LocalDOMWindow*>(this)->CountPermissionsPolicyUsage(
        feature, UseCounterImpl::PermissionsPolicyUsageType::kViolation);
  }

  if (!GetFrame()) {
    return;
  }

  // Construct the permissions policy violation report.
  bool is_isolated_context =
      GetExecutionContext() && GetExecutionContext()->IsIsolatedContext();
  const String& feature_name = GetNameForFeature(feature, is_isolated_context);
  const String& disp_str =
      (disposition == mojom::blink::PolicyDisposition::kReport ? "report"
                                                               : "enforce");

  PermissionsPolicyViolationReportBody* body =
      MakeGarbageCollected<PermissionsPolicyViolationReportBody>(
          feature_name, message, disp_str);

  Report* report = MakeGarbageCollected<Report>(
      ReportType::kPermissionsPolicyViolation, Url().GetString(), body);

  // Send the permissions policy violation report to the specified endpoint,
  // if one exists, as well as any ReportingObservers.
  if (reporting_endpoint) {
    ReportingContext::From(this)->QueueReport(report, {*reporting_endpoint});
  } else {
    ReportingContext::From(this)->QueueReport(report);
  }

  // TODO(iclelland): Report something different in report-only mode
  if (disposition == mojom::blink::PolicyDisposition::kEnforce) {
    GetFrame()->Console().AddMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kViolation,
        mojom::blink::ConsoleMessageLevel::kError, body->message()));
  }
}

void LocalDOMWindow::ReportDocumentPolicyViolation(
    mojom::blink::DocumentPolicyFeature feature,
    mojom::blink::PolicyDisposition disposition,
    const String& message,
    const String& source_file) const {
  if (!GetFrame())
    return;

  // Construct the document policy violation report.
  const String& feature_name =
      GetDocumentPolicyFeatureInfoMap().at(feature).feature_name.c_str();
  bool is_report_only = disposition == mojom::blink::PolicyDisposition::kReport;
  const String& disp_str = is_report_only ? "report" : "enforce";
  const DocumentPolicy* relevant_document_policy =
      is_report_only ? GetSecurityContext().GetReportOnlyDocumentPolicy()
                     : GetSecurityContext().GetDocumentPolicy();

  DocumentPolicyViolationReportBody* body =
      MakeGarbageCollected<DocumentPolicyViolationReportBody>(
          feature_name, message, disp_str, source_file);

  Report* report = MakeGarbageCollected<Report>(
      ReportType::kDocumentPolicyViolation, Url().GetString(), body);

  // Avoids sending duplicate reports, by comparing the generated MatchId.
  // The match ids are not guaranteed to be unique.
  // There are trade offs on storing full objects and storing match ids. Storing
  // full objects takes more memory. Storing match id has the potential of hash
  // collision. Since reporting is not a part critical system or have security
  // concern, dropping a valid report due to hash collision seems a reasonable
  // price to pay for the memory saving.
  unsigned report_id = report->MatchId();
  DCHECK(report_id);

  if (document_policy_violation_reports_sent_.Contains(report_id))
    return;
  document_policy_violation_reports_sent_.insert(report_id);

  // Send the document policy violation report to any ReportingObservers.
  const std::optional<std::string> endpoint =
      relevant_document_policy->GetFeatureEndpoint(feature);

  if (is_report_only) {
    UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.DocumentPolicy.ReportOnly",
                              feature);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.DocumentPolicy.Enforced",
                              feature);
  }

  ReportingContext::From(this)->QueueReport(
      report, endpoint ? Vector<String>{endpoint->c_str()} : Vector<String>{});

  // TODO(iclelland): Report something different in report-only mode
  if (!is_report_only) {
    GetFrame()->Console().AddMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kViolation,
        mojom::blink::ConsoleMessageLevel::kError, body->message()));
  }
}

void LocalDOMWindow::AddConsoleMessageImpl(ConsoleMessage* console_message,
                                           bool discard_duplicates) {
  CHECK(IsContextThread());

  if (!GetFrame())
    return;

  if (document() && console_message->Location()->IsUnknown()) {
    // TODO(dgozman): capture correct location at call places instead.
    unsigned line_number = 0;
    if (!document()->IsInDocumentWrite() &&
        document()->GetScriptableDocumentParser()) {
      ScriptableDocumentParser* parser =
          document()->GetScriptableDocumentParser();
      if (parser->IsParsingAtLineNumber())
        line_number = parser->LineNumber().OneBasedInt();
    }
    Vector<DOMNodeId> nodes(console_message->Nodes());
    std::optional<mojom::blink::ConsoleMessageCategory> category =
        console_message->Category();
    console_message = MakeGarbageCollected<ConsoleMessage>(
        console_message->GetSource(), console_message->GetLevel(),
        console_message->Message(),
        std::make_unique<SourceLocation>(Url().GetString(), String(),
                                         line_number, 0, nullptr));
    console_message->SetNodes(GetFrame(), std::move(nodes));
    if (category)
      console_message->SetCategory(*category);
  }

  GetFrame()->Console().AddMessage(console_message, discard_duplicates);
}

scoped_refptr<base::SingleThreadTaskRunner>
LocalDOMWindow::GetAgentGroupSchedulerCompositorTaskRunner() {
  if (!GetFrame())
    return nullptr;
  auto* frame_scheduler = GetFrame()->GetFrameScheduler();
  return frame_scheduler->GetAgentGroupScheduler()->CompositorTaskRunner();
}

void LocalDOMWindow::AddInspectorIssue(AuditsIssue issue) {
  if (GetFrame()) {
    GetFrame()->GetPage()->GetInspectorIssueStorage().AddInspectorIssue(
        this, std::move(issue));
  }
}

void LocalDOMWindow::CountUse(mojom::WebFeature feature) {
  if (!GetFrame())
    return;
  if (auto* loader = GetFrame()->Loader().GetDocumentLoader())
    loader->CountUse(feature);
}

void LocalDOMWindow::CountWebDXFeature(mojom::blink::WebDXFeature feature) {
  if (!GetFrame()) {
    return;
  }
  if (auto* loader = GetFrame()->Loader().GetDocumentLoader()) {
    loader->CountWebDXFeature(feature);
  }
}

void LocalDOMWindow::CountPermissionsPolicyUsage(
    mojom::blink::PermissionsPolicyFeature feature,
    UseCounterImpl::PermissionsPolicyUsageType type) {
  if (!GetFrame())
    return;
  if (auto* loader = GetFrame()->Loader().GetDocumentLoader()) {
    loader->GetUseCounter().CountPermissionsPolicyUsage(feature, type,
                                                        *GetFrame());
  }
}

void LocalDOMWindow::CountUseOnlyInCrossOriginIframe(
    mojom::blink::WebFeature feature) {
  if (GetFrame() && GetFrame()->IsCrossOriginToOutermostMainFrame())
    CountUse(feature);
}

void LocalDOMWindow::CountUseOnlyInSameOriginIframe(
    mojom::blink::WebFeature feature) {
  if (GetFrame() && !GetFrame()->IsOutermostMainFrame() &&
      !GetFrame()->IsCrossOriginToOutermostMainFrame()) {
    CountUse(feature);
  }
}

void LocalDOMWindow::CountUseOnlyInCrossSiteIframe(
    mojom::blink::WebFeature feature) {
  if (IsCrossSiteSubframeIncludingScheme())
    CountUse(feature);
}

bool LocalDOMWindow::HasInsecureContextInAncestors() const {
  for (Frame* parent = GetFrame()->Tree().Parent(); parent;
       parent = parent->Tree().Parent()) {
    auto* origin = parent->GetSecurityContext()->GetSecurityOrigin();
    if (!origin->IsPotentiallyTrustworthy())
      return true;
  }
  return false;
}

Document* LocalDOMWindow::InstallNewDocument(const DocumentInit& init) {
  // Blink should never attempt to install a new Document to a LocalDOMWindow
  // that's not attached to a LocalFrame.
  DCHECK(GetFrame());
  // Either:
  // - `this` should be a new LocalDOMWindow, that has never had a Document
  //   associated with it or
  // - `this` is being reused, and the previous Document has been disassociated
  //   via `ClearForReuse()`.
  DCHECK(!document_);
  DCHECK_EQ(init.GetWindow(), this);

  document_ = init.CreateDocument();
  document_->Initialize();

  document_->GetViewportData().UpdateViewportDescription();

  auto* frame_scheduler = GetFrame()->GetFrameScheduler();
  frame_scheduler->TraceUrlChange(document_->Url().GetString());
  frame_scheduler->SetCrossOriginToNearestMainFrame(
      GetFrame()->IsCrossOriginToNearestMainFrame());

  GetFrame()->GetPage()->GetChromeClient().InstallSupplements(*GetFrame());

  UpdateEventListenerCountsToDocumentForReuseIfNeeded();

  return document_.Get();
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
        ->PostTask(FROM_HERE, WTF::BindOnce(&LocalDOMWindow::DispatchLoadEvent,
                                            WrapPersistent(this)));
    return;
  }
  DispatchLoadEvent();
}

void LocalDOMWindow::DocumentWasClosed() {
  DispatchWindowLoadEvent();

  // An extension to step 4.5. or a part of step 4.6.3. of
  // https://html.spec.whatwg.org/C/#traverse-the-history .
  //
  // 4.5. ..., invoke the reset algorithm of each of those elements.
  // 4.6.3. Run any session history document visibility change steps ...
  if (document_) {
    document_->GetFormController().RestoreImmediately();
  }

  // 4.6.4. Fire an event named pageshow at the Document object's relevant
  // global object, ...
  EnqueueNonPersistedPageshowEvent();
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
  } else {
    DispatchEvent(*PageTransitionEvent::Create(event_type_names::kPageshow,
                                               false /* persisted */),
                  document_.Get());
  }
}

void LocalDOMWindow::DispatchPersistedPageshowEvent(
    base::TimeTicks navigation_start) {
  // Persisted pageshow events are dispatched for pages that are restored from
  // the back forward cache, and the event's timestamp should reflect the
  // |navigation_start| time of the back navigation.
  DispatchEvent(*PageTransitionEvent::CreatePersistedPageshow(navigation_start),
                document_.Get());
}

void LocalDOMWindow::DispatchPagehideEvent(
    PageTransitionEventPersistence persistence) {
  if (document_->IsPrerendering()) {
    // Do not dispatch the event while prerendering.
    return;
  }
  if (document_->UnloadStarted()) {
    // We've already dispatched pagehide (since it's the first thing we do when
    // starting unload) and shouldn't dispatch it again. We might get here on
    // a document that is already unloading/has unloaded but still part of the
    // FrameTree.
    // TODO(crbug.com/1119291): Investigate whether this is possible or not.
    return;
  }

  DispatchEvent(
      *PageTransitionEvent::Create(event_type_names::kPagehide, persistence),
      document_.Get());
}

void LocalDOMWindow::EnqueueHashchangeEvent(const String& old_url,
                                            const String& new_url) {
  // https://html.spec.whatwg.org/C/#history-traversal
  EnqueueWindowEvent(*HashChangeEvent::Create(old_url, new_url),
                     TaskType::kDOMManipulation);
}

void LocalDOMWindow::DispatchPopstateEvent(
    scoped_refptr<SerializedScriptValue> state_object,
    scheduler::TaskAttributionInfo* parent_task) {
  DCHECK(GetFrame());
  std::optional<scheduler::TaskAttributionTracker::TaskScope>
      task_attribution_scope;
  if (parent_task) {
    auto* tracker = scheduler::TaskAttributionTracker::From(GetIsolate());
    ScriptState* script_state = ToScriptStateForMainWorld(GetFrame());
    if (script_state && tracker) {
      task_attribution_scope = tracker->CreateTaskScope(
          script_state, parent_task,
          scheduler::TaskAttributionTracker::TaskScopeType::kPopState);
    }
  }
  DispatchEvent(*PopStateEvent::Create(std::move(state_object), history()));
}

LocalDOMWindow::~LocalDOMWindow() = default;

void LocalDOMWindow::Dispose() {
  BackForwardCacheBufferLimitTracker::Get()
      .DidRemoveFrameOrWorkerFromBackForwardCache(
          total_bytes_buffered_while_in_back_forward_cache_);
  total_bytes_buffered_while_in_back_forward_cache_ = 0;

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
  return const_cast<LocalDOMWindow*>(this);
}

const LocalDOMWindow* LocalDOMWindow::ToLocalDOMWindow() const {
  return this;
}

LocalDOMWindow* LocalDOMWindow::ToLocalDOMWindow() {
  return this;
}

MediaQueryList* LocalDOMWindow::matchMedia(const String& media) {
  return document()->GetMediaQueryMatcher().MatchMedia(media);
}

void LocalDOMWindow::FrameDestroyed() {
  TRACE_EVENT0("navigation", "LocalDOMWindow::FrameDestroyed");
  base::ScopedUmaHistogramTimer histogram_timer(
      "Navigation.LocalDOMWindow.FrameDestroyed");
  BackForwardCacheBufferLimitTracker::Get()
      .DidRemoveFrameOrWorkerFromBackForwardCache(
          total_bytes_buffered_while_in_back_forward_cache_);
  total_bytes_buffered_while_in_back_forward_cache_ = 0;

  // Some unit tests manually call FrameDestroyed(). Don't run it a second time.
  if (!GetFrame())
    return;
  // In the Reset() case, this Document::Shutdown() early-exits because it was
  // already called earlier in the commit process.
  // TODO(japhet): Can we merge this function and Reset()? At least, this
  // function should be renamed to Detach(), since in the Reset() case the frame
  // is not being destroyed.
  document()->Shutdown();
  document()->RemoveAllEventListenersRecursively();
  GetAgent()->DetachContext(this);
  NotifyContextDestroyed();
  RemoveAllEventListeners();
  MainThreadDebugger::Instance(GetIsolate())
      ->DidClearContextsForFrame(GetFrame());
  DisconnectFromFrame();
}

void LocalDOMWindow::RegisterEventListenerObserver(
    EventListenerObserver* event_listener_observer) {
  event_listener_observers_.insert(event_listener_observer);
}

void LocalDOMWindow::Reset() {
  DCHECK(document());
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
  trusted_types_map_.clear();
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

Screen* LocalDOMWindow::screen() {
  if (!screen_) {
    LocalFrame* frame = GetFrame();
    int64_t display_id =
        frame ? frame->GetChromeClient().GetScreenInfo(*frame).display_id
              : Screen::kInvalidDisplayId;
    screen_ = MakeGarbageCollected<Screen>(this, display_id);
  }
  return screen_.Get();
}

History* LocalDOMWindow::history() {
  if (!history_)
    history_ = MakeGarbageCollected<History>(this);
  return history_.Get();
}

BarProp* LocalDOMWindow::locationbar() {
  if (!locationbar_) {
    locationbar_ = MakeGarbageCollected<BarProp>(this);
  }
  return locationbar_.Get();
}

BarProp* LocalDOMWindow::menubar() {
  if (!menubar_)
    menubar_ = MakeGarbageCollected<BarProp>(this);
  return menubar_.Get();
}

BarProp* LocalDOMWindow::personalbar() {
  if (!personalbar_) {
    personalbar_ = MakeGarbageCollected<BarProp>(this);
  }
  return personalbar_.Get();
}

BarProp* LocalDOMWindow::scrollbars() {
  if (!scrollbars_) {
    scrollbars_ = MakeGarbageCollected<BarProp>(this);
  }
  return scrollbars_.Get();
}

BarProp* LocalDOMWindow::statusbar() {
  if (!statusbar_)
    statusbar_ = MakeGarbageCollected<BarProp>(this);
  return statusbar_.Get();
}

BarProp* LocalDOMWindow::toolbar() {
  if (!toolbar_)
    toolbar_ = MakeGarbageCollected<BarProp>(this);
  return toolbar_.Get();
}

FrameConsole* LocalDOMWindow::GetFrameConsole() const {
  if (!IsCurrentlyDisplayedInFrame())
    return nullptr;
  return &GetFrame()->Console();
}

Navigator* LocalDOMWindow::navigator() {
  if (!navigator_)
    navigator_ = MakeGarbageCollected<Navigator>(this);
  return navigator_.Get();
}

NavigationApi* LocalDOMWindow::navigation() {
  if (!navigation_)
    navigation_ = MakeGarbageCollected<NavigationApi>(this);
  return navigation_.Get();
}

void LocalDOMWindow::SchedulePostMessage(PostedMessage* posted_message) {
  LocalDOMWindow* source = posted_message->source;

  // Notify the host if the message contained a delegated capability. That state
  // should be tracked by the browser, and messages from remote hosts already
  // signal the browser via RemoteFrameHost's RouteMessageEvent.
  if (posted_message->delegated_capability !=
      mojom::blink::DelegatedCapability::kNone) {
    GetFrame()->GetLocalFrameHostRemote().ReceivedDelegatedCapability(
        posted_message->delegated_capability);
  }

  // Convert the posted message to a MessageEvent so it can be unpacked for
  // local dispatch.
  MessageEvent* event = MessageEvent::Create(
      std::move(posted_message->channels), std::move(posted_message->data),
      posted_message->source_origin->ToString(), String(),
      posted_message->source, posted_message->user_activation,
      posted_message->delegated_capability);

  // Allowing unbounded amounts of messages to build up for a suspended context
  // is problematic; consider imposing a limit or other restriction if this
  // surfaces often as a problem (see crbug.com/587012).
  std::unique_ptr<SourceLocation> location = CaptureSourceLocation(source);
  GetTaskRunner(TaskType::kPostedMessage)
      ->PostTask(
          FROM_HERE,
          WTF::BindOnce(&LocalDOMWindow::DispatchPostMessage,
                        WrapPersistent(this), WrapPersistent(event),
                        std::move(posted_message->target_origin),
                        std::move(location), source->GetAgent()->cluster_id()));
  event->async_task_context()->Schedule(this, "postMessage");
  uint64_t trace_id = base::trace_event::GetNextGlobalTraceId();
  event->SetTraceId(trace_id);
  TRACE_EVENT_INSTANT(
      "devtools.timeline", "SchedulePostMessage", "data",
      [&](perfetto::TracedValue context) {
        inspector_schedule_post_message_event::Data(
            std::move(context), GetExecutionContext(), trace_id);
      },
      perfetto::Flow::Global(trace_id));
}

void LocalDOMWindow::DispatchPostMessage(
    MessageEvent* event,
    scoped_refptr<const SecurityOrigin> intended_target_origin,
    std::unique_ptr<SourceLocation> location,
    const base::UnguessableToken& source_agent_cluster_id) {
  // Do not report postMessage tasks to the ad tracker. This allows non-ad
  // script to perform operations in response to events created by ad frames.
  probe::AsyncTask async_task(this, event->async_task_context(),
                              nullptr /* step */, true /* enabled */,
                              probe::AsyncTask::AdTrackingType::kIgnore);
  if (!IsCurrentlyDisplayedInFrame())
    return;

  event->EntangleMessagePorts(this);

  TRACE_EVENT(
      "devtools.timeline", "HandlePostMessage", "data",
      [&](perfetto::TracedValue context) {
        inspector_handle_post_message_event::Data(
            std::move(context), GetExecutionContext(), *event);
      },
      perfetto::Flow::Global(event->GetTraceId()));

  DispatchMessageEventWithOriginCheck(intended_target_origin.get(), event,
                                      std::move(location),
                                      source_agent_cluster_id);
}

void LocalDOMWindow::DispatchMessageEventWithOriginCheck(
    const SecurityOrigin* intended_target_origin,
    MessageEvent* event,
    std::unique_ptr<SourceLocation> location,
    const base::UnguessableToken& source_agent_cluster_id) {
  TRACE_EVENT0("blink", "LocalDOMWindow::DispatchMessageEventWithOriginCheck");
  if (intended_target_origin) {
    bool valid_target =
        intended_target_origin->IsSameOriginWith(GetSecurityOrigin());

    if (!valid_target) {
      String message = ExceptionMessages::FailedToExecute(
          "postMessage", "DOMWindow",
          "The target origin provided ('" + intended_target_origin->ToString() +
              "') does not match the recipient window's origin ('" +
              GetSecurityOrigin()->ToString() + "').");
      auto* console_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kSecurity,
          mojom::ConsoleMessageLevel::kWarning, message, std::move(location));
      GetFrameConsole()->AddMessage(console_message);
      return;
    }
  }

  KURL sender(event->origin());
  if (!GetContentSecurityPolicy()->AllowConnectToSource(
          sender, sender, RedirectStatus::kNoRedirect,
          ReportingDisposition::kSuppressReporting)) {
    UseCounter::Count(
        this, WebFeature::kPostMessageIncomingWouldBeBlockedByConnectSrc);
  }

  if (event->IsOriginCheckRequiredToAccessData()) {
    scoped_refptr<SecurityOrigin> sender_security_origin =
        SecurityOrigin::Create(sender);
    if (!sender_security_origin->IsSameOriginWith(GetSecurityOrigin())) {
      event = MessageEvent::CreateError(event->origin(), event->source());
    }
  }
  if (event->IsLockedToAgentCluster()) {
    if (!IsSameAgentCluster(source_agent_cluster_id)) {
      UseCounter::Count(
          this,
          WebFeature::kMessageEventSharedArrayBufferDifferentAgentCluster);
      event = MessageEvent::CreateError(event->origin(), event->source());
    } else {
      scoped_refptr<SecurityOrigin> sender_origin =
          SecurityOrigin::Create(sender);
      if (!sender_origin->IsSameOriginWith(GetSecurityOrigin())) {
        UseCounter::Count(
            this, WebFeature::kMessageEventSharedArrayBufferSameAgentCluster);
      } else {
        UseCounter::Count(this,
                          WebFeature::kMessageEventSharedArrayBufferSameOrigin);
      }
    }
  }

  if (!event->CanDeserializeIn(this)) {
    event = MessageEvent::CreateError(event->origin(), event->source());
  }

  if (event->delegatedCapability() ==
      mojom::blink::DelegatedCapability::kPaymentRequest) {
    UseCounter::Count(this, WebFeature::kCapabilityDelegationOfPaymentRequest);
    payment_request_token_.Activate();
  }

  if (event->delegatedCapability() ==
      mojom::blink::DelegatedCapability::kFullscreenRequest) {
    UseCounter::Count(this,
                      WebFeature::kCapabilityDelegationOfFullscreenRequest);
    fullscreen_request_token_.Activate();
  }
  if (RuntimeEnabledFeatures::CapabilityDelegationDisplayCaptureRequestEnabled(
          this) &&
      event->delegatedCapability() ==
          mojom::blink::DelegatedCapability::kDisplayCaptureRequest) {
    // TODO(crbug.com/1412770): Add use counter.
    display_capture_request_token_.Activate();
  }

  if (GetFrame() &&
      GetFrame()->GetPage()->GetPageScheduler()->IsInBackForwardCache()) {
    // Enqueue the event when the page is in back/forward cache, so that it
    // would not cause JavaScript execution. The event will be dispatched upon
    // restore.
    EnqueueEvent(*event, TaskType::kInternalDefault);
  } else {
    DispatchEvent(*event);
  }
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

void LocalDOMWindow::print(ScriptState* script_state) {
  // Don't try to print if there's no frame attached anymore.
  if (!GetFrame())
    return;

  if (script_state && IsRunningMicrotasks(script_state)) {
    UseCounter::Count(this, WebFeature::kDuring_Microtask_Print);
  }

  if (GetFrame()->IsLoading()) {
    should_print_when_finished_loading_ = true;
    return;
  }

  CountUseOnlyInSameOriginIframe(WebFeature::kSameOriginIframeWindowPrint);
  CountUseOnlyInCrossOriginIframe(WebFeature::kCrossOriginWindowPrint);

  should_print_when_finished_loading_ = false;
  GetFrame()->GetPage()->GetChromeClient().Print(GetFrame());
}

void LocalDOMWindow::stop() {
  if (!GetFrame())
    return;
  GetFrame()->Loader().StopAllLoaders(/*abort_client=*/true);
}

void LocalDOMWindow::alert(ScriptState* script_state, const String& message) {
  if (!GetFrame())
    return;

  if (IsSandboxed(network::mojom::blink::WebSandboxFlags::kModals)) {
    UseCounter::Count(this, WebFeature::kDialogInSandboxedContext);
    GetFrameConsole()->AddMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError,
        GetFrame()->IsInFencedFrameTree()
            ? "Ignored call to 'alert()'. The document is in a fenced frame "
              "tree."
            : "Ignored call to 'alert()'. The document is sandboxed, and the "
              "'allow-modals' keyword is not set."));
    return;
  }

  if (IsRunningMicrotasks(script_state)) {
    UseCounter::Count(this, WebFeature::kDuring_Microtask_Alert);
  }

  document()->UpdateStyleAndLayoutTree();

  Page* page = GetFrame()->GetPage();
  if (!page)
    return;

  CountUseOnlyInSameOriginIframe(WebFeature::kSameOriginIframeWindowAlert);
  Deprecation::CountDeprecationCrossOriginIframe(
      this, WebFeature::kCrossOriginWindowAlert);

  page->GetChromeClient().OpenJavaScriptAlert(GetFrame(), message);
}

bool LocalDOMWindow::confirm(ScriptState* script_state, const String& message) {
  if (!GetFrame())
    return false;

  if (IsSandboxed(network::mojom::blink::WebSandboxFlags::kModals)) {
    UseCounter::Count(this, WebFeature::kDialogInSandboxedContext);
    GetFrameConsole()->AddMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError,
        GetFrame()->IsInFencedFrameTree()
            ? "Ignored call to 'confirm()'. The document is in a fenced frame "
              "tree."
            : "Ignored call to 'confirm()'. The document is sandboxed, and the "
              "'allow-modals' keyword is not set."));
    return false;
  }

  if (IsRunningMicrotasks(script_state)) {
    UseCounter::Count(this, WebFeature::kDuring_Microtask_Confirm);
  }

  document()->UpdateStyleAndLayoutTree();

  Page* page = GetFrame()->GetPage();
  if (!page)
    return false;

  CountUseOnlyInSameOriginIframe(WebFeature::kSameOriginIframeWindowConfirm);
  Deprecation::CountDeprecationCrossOriginIframe(
      this, WebFeature::kCrossOriginWindowConfirm);

  return page->GetChromeClient().OpenJavaScriptConfirm(GetFrame(), message);
}

String LocalDOMWindow::prompt(ScriptState* script_state,
                              const String& message,
                              const String& default_value) {
  if (!GetFrame())
    return String();

  if (IsSandboxed(network::mojom::blink::WebSandboxFlags::kModals)) {
    UseCounter::Count(this, WebFeature::kDialogInSandboxedContext);
    GetFrameConsole()->AddMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError,
        GetFrame()->IsInFencedFrameTree()
            ? "Ignored call to 'prompt()'. The document is in a fenced frame "
              "tree."
            : "Ignored call to 'prompt()'. The document is sandboxed, and the "
              "'allow-modals' keyword is not set."));
    return String();
  }

  if (IsRunningMicrotasks(script_state)) {
    UseCounter::Count(this, WebFeature::kDuring_Microtask_Prompt);
  }

  document()->UpdateStyleAndLayoutTree();

  Page* page = GetFrame()->GetPage();
  if (!page)
    return String();

  String return_value;
  if (page->GetChromeClient().OpenJavaScriptPrompt(GetFrame(), message,
                                                   default_value, return_value))
    return return_value;

  CountUseOnlyInSameOriginIframe(WebFeature::kSameOriginIframeWindowPrompt);
  Deprecation::CountDeprecationCrossOriginIframe(
      this, WebFeature::kCrossOriginWindowAlert);

  return String();
}

bool LocalDOMWindow::find(const String& string,
                          bool case_sensitive,
                          bool backwards,
                          bool wrap,
                          bool whole_word,
                          bool /*searchInFrames*/,
                          bool /*showDialog*/) const {
  auto forced_activatable_locks = document()
                                      ->GetDisplayLockDocumentState()
                                      .GetScopedForceActivatableLocks();

  if (!IsCurrentlyDisplayedInFrame())
    return false;

  // Up-to-date, clean tree is required for finding text in page, since it
  // relies on TextIterator to look over the text.
  document()->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  // FIXME (13016): Support searchInFrames and showDialog
  FindOptions options = FindOptions()
                            .SetBackwards(backwards)
                            .SetCaseInsensitive(!case_sensitive)
                            .SetWrappingAround(wrap)
                            .SetWholeWord(whole_word);
  return Editor::FindString(*GetFrame(), string, options);
}

bool LocalDOMWindow::offscreenBuffering() const {
  return true;
}

int LocalDOMWindow::outerHeight() const {
  if (!GetFrame())
    return 0;

  LocalFrame* frame = GetFrame();

  // FencedFrames should return innerHeight to prevent passing
  // arbitrary data through the window height.
  if (frame->IsInFencedFrameTree()) {
    return innerHeight();
  }

  Page* page = frame->GetPage();
  if (!page)
    return 0;

  ChromeClient& chrome_client = page->GetChromeClient();
  if (page->GetSettings().GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(
        lroundf(chrome_client.RootWindowRect(*frame).height() *
                chrome_client.GetScreenInfo(*frame).device_scale_factor));
  }
  return chrome_client.RootWindowRect(*frame).height();
}

int LocalDOMWindow::outerWidth() const {
  if (!GetFrame())
    return 0;

  LocalFrame* frame = GetFrame();

  // FencedFrames should return innerWidth to prevent passing
  // arbitrary data through the window width.
  if (frame->IsInFencedFrameTree()) {
    return innerWidth();
  }

  Page* page = frame->GetPage();
  if (!page)
    return 0;

  ChromeClient& chrome_client = page->GetChromeClient();
  if (page->GetSettings().GetReportScreenSizeInPhysicalPixelsQuirk()) {
    return static_cast<int>(
        lroundf(chrome_client.RootWindowRect(*frame).width() *
                chrome_client.GetScreenInfo(*frame).device_scale_factor));
  }
  return chrome_client.RootWindowRect(*frame).width();
}

gfx::Size LocalDOMWindow::GetViewportSize() const {
  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return gfx::Size();

  Page* page = GetFrame()->GetPage();
  if (!page)
    return gfx::Size();

  // The main frame's viewport size depends on the page scale. If viewport is
  // enabled, the initial page scale depends on the content width and is set
  // after a layout, perform one now so queries during page load will use the
  // up to date viewport.
  if (page->GetSettings().GetViewportEnabled() && GetFrame()->IsMainFrame()) {
    document()->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  }

  // FIXME: This is potentially too much work. We really only need to know the
  // dimensions of the parent frame's layoutObject.
  if (Frame* parent = GetFrame()->Tree().Parent()) {
    if (auto* parent_local_frame = DynamicTo<LocalFrame>(parent)) {
      parent_local_frame->GetDocument()->UpdateStyleAndLayout(
          DocumentUpdateReason::kJavaScript);
    }
  }

  return document()->View()->Size();
}

int LocalDOMWindow::innerHeight() const {
  if (!GetFrame())
    return 0;

  return AdjustForAbsoluteZoom::AdjustInt(GetViewportSize().height(),
                                          GetFrame()->LayoutZoomFactor());
}

int LocalDOMWindow::innerWidth() const {
  if (!GetFrame())
    return 0;

  return AdjustForAbsoluteZoom::AdjustInt(GetViewportSize().width(),
                                          GetFrame()->LayoutZoomFactor());
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
        lroundf(chrome_client.RootWindowRect(*frame).x() *
                chrome_client.GetScreenInfo(*frame).device_scale_factor));
  }
  return chrome_client.RootWindowRect(*frame).x();
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
        lroundf(chrome_client.RootWindowRect(*frame).y() *
                chrome_client.GetScreenInfo(*frame).device_scale_factor));
  }
  return chrome_client.RootWindowRect(*frame).y();
}

double LocalDOMWindow::scrollX() const {
  if (!GetFrame() || !GetFrame()->GetPage())
    return 0;

  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return 0;

  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidAccessScrollOffset();

  document()->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  // TODO(bokan): This is wrong when the document.rootScroller is non-default.
  // crbug.com/505516.
  double viewport_x = view->LayoutViewport()->GetWebExposedScrollOffset().x();
  return AdjustForAbsoluteZoom::AdjustScroll(viewport_x,
                                             GetFrame()->LayoutZoomFactor());
}

double LocalDOMWindow::scrollY() const {
  if (!GetFrame() || !GetFrame()->GetPage())
    return 0;

  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return 0;

  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidAccessScrollOffset();

  document()->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  // TODO(bokan): This is wrong when the document.rootScroller is non-default.
  // crbug.com/505516.
  double viewport_y = view->LayoutViewport()->GetWebExposedScrollOffset().y();
  return AdjustForAbsoluteZoom::AdjustScroll(viewport_y,
                                             GetFrame()->LayoutZoomFactor());
}

DOMViewport* LocalDOMWindow::viewport() {
  return viewport_.Get();
}

DOMVisualViewport* LocalDOMWindow::visualViewport() {
  return visualViewport_.Get();
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
  DCHECK(RuntimeEnabledFeatures::WindowDefaultStatusEnabled());
  default_status_ = string;
}

String LocalDOMWindow::origin() const {
  return GetSecurityOrigin()->ToString();
}

Document* LocalDOMWindow::document() const {
  return document_.Get();
}

StyleMedia* LocalDOMWindow::styleMedia() {
  if (!media_)
    media_ = MakeGarbageCollected<StyleMedia>(this);
  return media_.Get();
}

CSSStyleDeclaration* LocalDOMWindow::getComputedStyle(
    Element* elt,
    const String& pseudo_elt) const {
  DCHECK(elt);
  return MakeGarbageCollected<CSSComputedStyleDeclaration>(elt, false,
                                                           pseudo_elt);
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

  LocalFrameView* view = GetFrame()->View();
  if (!view)
    return;

  Page* page = GetFrame()->GetPage();
  if (!page)
    return;

  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidSetScrollOffset();

  document()->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

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
  gfx::PointF current_position = viewport->ScrollPosition();
  gfx::Vector2dF scaled_delta(x * GetFrame()->LayoutZoomFactor(),
                              y * GetFrame()->LayoutZoomFactor());
  gfx::PointF new_scaled_position = current_position + scaled_delta;

  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndAndDirection(
          current_position, scaled_delta,
          RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled());
  new_scaled_position =
      viewport->GetSnapPositionAndSetTarget(*strategy).value_or(
          new_scaled_position);

  mojom::blink::ScrollBehavior scroll_behavior =
      ScrollableArea::V8EnumToScrollBehavior(
          scroll_to_options->behavior().AsEnum());
  viewport->SetScrollOffset(
      viewport->ScrollPositionToOffset(new_scaled_position),
      mojom::blink::ScrollType::kProgrammatic, scroll_behavior);
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

  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidSetScrollOffset();

  // It is only necessary to have an up-to-date layout if the position may be
  // clamped, which is never the case for (0, 0).
  if (!scroll_to_options->hasLeft() || !scroll_to_options->hasTop() ||
      scroll_to_options->left() || scroll_to_options->top()) {
    document()->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  }

  float scaled_x = 0.0f;
  float scaled_y = 0.0f;

  PaintLayerScrollableArea* viewport = view->LayoutViewport();
  ScrollOffset current_offset = viewport->GetScrollOffset();
  scaled_x = current_offset.x();
  scaled_y = current_offset.y();

  if (scroll_to_options->hasLeft()) {
    scaled_x = ScrollableArea::NormalizeNonFiniteScroll(
                   base::saturated_cast<float>(scroll_to_options->left())) *
               GetFrame()->LayoutZoomFactor();
  }

  if (scroll_to_options->hasTop()) {
    scaled_y = ScrollableArea::NormalizeNonFiniteScroll(
                   base::saturated_cast<float>(scroll_to_options->top())) *
               GetFrame()->LayoutZoomFactor();
  }

  gfx::PointF new_scaled_position = viewport->ScrollOffsetToPosition(
      SnapScrollOffsetToPhysicalPixels(ScrollOffset(scaled_x, scaled_y)));

  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(
          new_scaled_position, scroll_to_options->hasLeft(),
          scroll_to_options->hasTop());
  new_scaled_position =
      viewport->GetSnapPositionAndSetTarget(*strategy).value_or(
          new_scaled_position);
  mojom::blink::ScrollBehavior scroll_behavior =
      ScrollableArea::V8EnumToScrollBehavior(
          scroll_to_options->behavior().AsEnum());
  viewport->SetScrollOffset(
      viewport->ScrollPositionToOffset(new_scaled_position),
      mojom::blink::ScrollType::kProgrammatic, scroll_behavior);
}

void LocalDOMWindow::moveBy(int x, int y) const {
  if (!GetFrame() || !GetFrame()->IsOutermostMainFrame() ||
      document()->IsPrerendering()) {
    return;
  }

  if (IsPictureInPictureWindow())
    return;

  LocalFrame* frame = GetFrame();
  Page* page = frame->GetPage();
  if (!page)
    return;

  gfx::Rect window_rect = page->GetChromeClient().RootWindowRect(*frame);
  window_rect.Offset(x, y);
  // Security check (the spec talks about UniversalBrowserWrite to disable this
  // check...)
  page->GetChromeClient().SetWindowRect(window_rect, *frame);
}

void LocalDOMWindow::moveTo(int x, int y) const {
  if (!GetFrame() || !GetFrame()->IsOutermostMainFrame() ||
      document()->IsPrerendering()) {
    return;
  }

  if (IsPictureInPictureWindow())
    return;

  LocalFrame* frame = GetFrame();
  Page* page = frame->GetPage();
  if (!page)
    return;

  gfx::Rect window_rect = page->GetChromeClient().RootWindowRect(*frame);
  window_rect.set_origin(gfx::Point(x, y));
  // Security check (the spec talks about UniversalBrowserWrite to disable this
  // check...)
  page->GetChromeClient().SetWindowRect(window_rect, *frame);
}

void LocalDOMWindow::resizeBy(int x,
                              int y,
                              ExceptionState& exception_state) const {
  if (!GetFrame() || !GetFrame()->IsOutermostMainFrame() ||
      document()->IsPrerendering()) {
    return;
  }

  if (IsPictureInPictureWindow()) {
    if (!LocalFrame::ConsumeTransientUserActivation(GetFrame())) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "resizeBy() requires user activation in document picture-in-picture");
      return;
    }
  }

  LocalFrame* frame = GetFrame();
  Page* page = frame->GetPage();
  if (!page)
    return;

  gfx::Rect fr = page->GetChromeClient().RootWindowRect(*frame);
  gfx::Size dest(fr.width() + x, fr.height() + y);
  gfx::Rect update(fr.origin(), dest);
  page->GetChromeClient().SetWindowRect(update, *frame);
}

void LocalDOMWindow::resizeTo(int width,
                              int height,
                              ExceptionState& exception_state) const {
  if (!GetFrame() || !GetFrame()->IsOutermostMainFrame() ||
      document()->IsPrerendering()) {
    return;
  }

  if (IsPictureInPictureWindow()) {
    if (!LocalFrame::ConsumeTransientUserActivation(GetFrame())) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "resizeTo() requires user activation in document picture-in-picture");
      return;
    }
  }

  LocalFrame* frame = GetFrame();
  Page* page = frame->GetPage();
  if (!page)
    return;

  gfx::Rect fr = page->GetChromeClient().RootWindowRect(*frame);
  gfx::Size dest = gfx::Size(width, height);
  gfx::Rect update(fr.origin(), dest);
  page->GetChromeClient().SetWindowRect(update, *frame);
}

int LocalDOMWindow::requestAnimationFrame(V8FrameRequestCallback* callback) {
  return RequestAnimationFrame(document(), callback, /*legacy=*/false);
}

int LocalDOMWindow::webkitRequestAnimationFrame(
    V8FrameRequestCallback* callback) {
  return RequestAnimationFrame(document(), callback, /*legacy=*/true);
}

void LocalDOMWindow::cancelAnimationFrame(int id) {
  document()->CancelAnimationFrame(id);
}

void LocalDOMWindow::queueMicrotask(V8VoidFunction* callback) {
  GetAgent()->event_loop()->EnqueueMicrotask(
      WTF::BindOnce(&V8VoidFunction::InvokeAndReportException,
                    WrapPersistent(callback), nullptr));
}

bool LocalDOMWindow::originAgentCluster() const {
  return GetAgent()->IsOriginKeyed();
}

CustomElementRegistry* LocalDOMWindow::customElements(
    ScriptState* script_state) const {
  if (!script_state->World().IsMainWorld())
    return nullptr;
  return customElements();
}

CustomElementRegistry* LocalDOMWindow::customElements() const {
  if (!custom_elements_ && document_) {
    custom_elements_ = MakeGarbageCollected<CustomElementRegistry>(this);
    custom_elements_->AssociatedWith(*document_);
  }
  return custom_elements_.Get();
}

CustomElementRegistry* LocalDOMWindow::MaybeCustomElements() const {
  return custom_elements_.Get();
}

External* LocalDOMWindow::external() {
  if (!external_)
    external_ = MakeGarbageCollected<External>();
  return external_.Get();
}

// NOLINTNEXTLINE(bugprone-virtual-near-miss)
bool LocalDOMWindow::isSecureContext() const {
  return IsSecureContext();
}

void LocalDOMWindow::ClearIsolatedWorldCSPForTesting(int32_t world_id) {
  isolated_world_csp_map_->erase(world_id);
}

bool IsSuddenTerminationDisablerEvent(const AtomicString& event_type) {
  return event_type == event_type_names::kUnload ||
         event_type == event_type_names::kBeforeunload ||
         event_type == event_type_names::kPagehide ||
         event_type == event_type_names::kVisibilitychange;
}

void LocalDOMWindow::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  DOMWindow::AddedEventListener(event_type, registered_listener);
  if (auto* frame = GetFrame()) {
    frame->GetEventHandlerRegistry().DidAddEventHandler(
        *this, event_type, registered_listener.Options());
  }

  document()->AddListenerTypeIfNeeded(event_type, *this);
  document()->DidAddEventListeners(/*count*/ 1);

  for (auto& it : event_listener_observers_) {
    it->DidAddEventListener(this, event_type);
  }

  if (event_type == event_type_names::kUnload) {
    CountDeprecation(WebFeature::kDocumentUnloadRegistered);
  } else if (event_type == event_type_names::kBeforeunload) {
    UseCounter::Count(this, WebFeature::kDocumentBeforeUnloadRegistered);
    if (GetFrame() && !GetFrame()->IsMainFrame())
      UseCounter::Count(this, WebFeature::kSubFrameBeforeUnloadRegistered);
  } else if (event_type == event_type_names::kPagehide) {
    UseCounter::Count(this, WebFeature::kDocumentPageHideRegistered);
  } else if (event_type == event_type_names::kPageshow) {
    UseCounter::Count(this, WebFeature::kDocumentPageShowRegistered);
  }

  if (GetFrame() && IsSuddenTerminationDisablerEvent(event_type))
    GetFrame()->AddedSuddenTerminationDisablerListener(*this, event_type);
}

void LocalDOMWindow::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  DOMWindow::RemovedEventListener(event_type, registered_listener);
  document()->DidRemoveEventListeners(/*count*/ 1);
  if (auto* frame = GetFrame()) {
    frame->GetEventHandlerRegistry().DidRemoveEventHandler(
        *this, event_type, registered_listener.Options());
  }

  for (auto& it : event_listener_observers_) {
    it->DidRemoveEventListener(this, event_type);
  }

  // Update sudden termination disabler state if we removed a listener for
  // unload/beforeunload/pagehide/visibilitychange.
  if (GetFrame() && IsSuddenTerminationDisablerEvent(event_type))
    GetFrame()->RemovedSuddenTerminationDisablerListener(*this, event_type);
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
  } else {
    DispatchEvent(load_event, document());
  }

  if (LocalFrame* frame = GetFrame()) {
    WindowPerformance* performance = DOMWindowPerformance::performance(*this);
    DCHECK(performance);
    performance->NotifyNavigationTimingToObservers();

    // For load events, send a separate load event to the enclosing frame only.
    // This is a DOM extension and is independent of bubbling/capturing rules of
    // the DOM.
    if (FrameOwner* owner = frame->Owner())
      owner->DispatchLoad();

    if (frame->IsAttached()) {
      DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
          "MarkLoad", inspector_mark_load_event::Data, frame);
      probe::LoadEventFired(frame);
      frame->GetFrameScheduler()->OnDispatchLoadEvent();
    }
  }
}

DispatchEventResult LocalDOMWindow::DispatchEvent(Event& event,
                                                  EventTarget* target) {
#if DCHECK_IS_ON()
  DCHECK(!EventDispatchForbiddenScope::IsEventDispatchForbidden());
#endif

  event.SetTrusted(true);
  event.SetTarget(target ? target : this);
  event.SetCurrentTarget(this);
  event.SetEventPhase(Event::PhaseType::kAtTarget);

  DEVTOOLS_TIMELINE_TRACE_EVENT("EventDispatch",
                                inspector_event_dispatch_event::Data, event,
                                GetIsolate());
  return FireEventListeners(event);
}

void LocalDOMWindow::RemoveAllEventListeners() {
  int previous_unload_handlers_count =
      NumberOfEventListeners(event_type_names::kUnload);
  int previous_before_unload_handlers_count =
      NumberOfEventListeners(event_type_names::kBeforeunload);
  int previous_page_hide_handlers_count =
      NumberOfEventListeners(event_type_names::kPagehide);
  int previous_visibility_change_handlers_count =
      NumberOfEventListeners(event_type_names::kVisibilitychange);
  if (document_ && HasEventListeners()) {
    GetEventTargetData()->event_listener_map.ForAllEventListenerTypes(
        [this](const AtomicString& event_type, uint32_t count) {
          document_->DidRemoveEventListeners(count);
        });
  }
  EventTarget::RemoveAllEventListeners();

  for (auto& it : event_listener_observers_) {
    it->DidRemoveAllEventListeners(this);
  }

  if (GetFrame()) {
    GetFrame()->GetEventHandlerRegistry().DidRemoveAllEventHandlers(*this);
  }

  // Update sudden termination disabler state if we previously have listeners
  // for unload/beforeunload/pagehide/visibilitychange.
  if (GetFrame() && previous_unload_handlers_count) {
    GetFrame()->RemovedSuddenTerminationDisablerListener(
        *this, event_type_names::kUnload);
  }
  if (GetFrame() && previous_before_unload_handlers_count) {
    GetFrame()->RemovedSuddenTerminationDisablerListener(
        *this, event_type_names::kBeforeunload);
  }
  if (GetFrame() && previous_page_hide_handlers_count) {
    GetFrame()->RemovedSuddenTerminationDisablerListener(
        *this, event_type_names::kPagehide);
  }
  if (GetFrame() && previous_visibility_change_handlers_count) {
    GetFrame()->RemovedSuddenTerminationDisablerListener(
        *this, event_type_names::kVisibilitychange);
  }
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

  if (message.empty())
    return;

  GetFrameConsole()->AddMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kError, message));
}

DOMWindow* LocalDOMWindow::open(v8::Isolate* isolate,
                                const String& url_string,
                                const AtomicString& target,
                                const String& features,
                                ExceptionState& exception_state) {
  // Get the window script is currently executing within the context of.
  // This is usually, but not necessarily the same as 'this'.
  LocalDOMWindow* entered_window = EnteredDOMWindow(isolate);

  if (!IsCurrentlyDisplayedInFrame() || !entered_window->GetFrame()) {
    return nullptr;
  }

  // If the bindings implementation is 100% correct, the current realm and the
  // entered realm should be same origin-domain. However, to be on the safe
  // side and add some defense in depth, we'll check against the entry realm
  // as well here.
  if (!BindingSecurity::ShouldAllowAccessTo(entered_window, this)) {
    // Trigger DCHECK() failure, while gracefully failing on release builds.
    NOTREACHED_IN_MIGRATION();
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kWindowOpenRealmMismatch);
    return nullptr;
  }

  UseCounter::Count(*entered_window, WebFeature::kDOMWindowOpen);
  entered_window->CountUseOnlyInCrossOriginIframe(
      WebFeature::kDOMWindowOpenCrossOriginIframe);
  if (!features.empty())
    UseCounter::Count(*entered_window, WebFeature::kDOMWindowOpenFeatures);

  KURL completed_url = url_string.empty()
                           ? KURL(g_empty_string)
                           : entered_window->CompleteURL(url_string);
  if (!completed_url.IsEmpty() && !completed_url.IsValid()) {
    UseCounter::Count(entered_window, WebFeature::kWindowOpenWithInvalidURL);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Unable to open a window with invalid URL '" +
            completed_url.GetString() + "'.\n");
    return nullptr;
  }

  WebWindowFeatures window_features =
      GetWindowFeaturesFromString(features, entered_window);

  if (window_features.is_partitioned_popin) {
    UseCounter::Count(*entered_window,
                      WebFeature::kPartitionedPopin_OpenAttempt);
    if (!IsFeatureEnabled(
            mojom::blink::PermissionsPolicyFeature::kPartitionedPopins,
            ReportOptions::kReportOnFailure)) {
      exception_state.ThrowSecurityError(
          "Permissions-Policy: `popin` access denied.",
          "Permissions-Policy: `popin` access denied.");
      return nullptr;
    }
    if (entered_window->GetFrame()->GetPage()->IsPartitionedPopin()) {
      exception_state.ThrowSecurityError(
          "Partitioned popins cannot open their own popin.",
          "Partitioned popins cannot open their own popin.");
      return nullptr;
    }
    if (entered_window->Url().Protocol() != WTF::g_https_atom) {
      exception_state.ThrowSecurityError(
          "Partitioned popins must be opened from https URLs.",
          "Partitioned popins must be opened from https URLs.");
      return nullptr;
    }
    // We prevent redirections via PartitionedPopinsNavigationThrottle.
    if (completed_url.Protocol() != WTF::g_https_atom) {
      exception_state.ThrowSecurityError(
          "Partitioned popins can only open https URLs.",
          "Partitioned popins can only open https URLs.");
      return nullptr;
    }
  }

  // In fenced frames, we should always use `noopener`.
  if (GetFrame()->IsInFencedFrameTree()) {
    window_features.noopener = true;
  }

  FrameLoadRequest frame_request(entered_window,
                                 ResourceRequest(completed_url));
  frame_request.SetFeaturesForWindowOpen(window_features);

  // Normally, FrameLoader would take care of setting the referrer for a
  // navigation that is triggered from javascript. However, creating a window
  // goes through sufficient processing that it eventually enters FrameLoader as
  // an embedder-initiated navigation.  FrameLoader assumes no responsibility
  // for generating an embedder-initiated navigation's referrer, so we need to
  // ensure the proper referrer is set now.
  Referrer referrer = SecurityPolicy::GenerateReferrer(
      window_features.noreferrer ? network::mojom::ReferrerPolicy::kNever
                                 : entered_window->GetReferrerPolicy(),
      completed_url, entered_window->OutgoingReferrer());
  frame_request.GetResourceRequest().SetReferrerString(referrer.referrer);
  frame_request.GetResourceRequest().SetReferrerPolicy(
      referrer.referrer_policy);

  bool has_user_gesture = LocalFrame::HasTransientUserActivation(GetFrame());
  frame_request.GetResourceRequest().SetHasUserGesture(has_user_gesture);

  if (window_features.attribution_srcs.has_value()) {
    // An impression must be attached prior to the
    // `FindOrCreateFrameForNavigation()` call, as that call may result in
    // performing a navigation if the call results in creating a new window with
    // noopener set.
    frame_request.SetImpression(entered_window->GetFrame()
                                    ->GetAttributionSrcLoader()
                                    ->RegisterNavigation(
                                        /*navigation_url=*/completed_url,
                                        *window_features.attribution_srcs,
                                        has_user_gesture,
                                        referrer.referrer_policy));
  }

  FrameTree::FindResult result =
      GetFrame()->Tree().FindOrCreateFrameForNavigation(
          frame_request, target.empty() ? AtomicString("_blank") : target);
  if (!result.frame)
    return nullptr;

  if (window_features.x_set || window_features.y_set) {
    // This runs after FindOrCreateFrameForNavigation() so blocked popups are
    // not counted.
    UseCounter::Count(*entered_window,
                      WebFeature::kDOMWindowOpenPositioningFeatures);

    // Coarsely measure whether coordinates may be requesting another screen.
    ChromeClient& chrome_client = GetFrame()->GetChromeClient();
    const gfx::Rect screen = chrome_client.GetScreenInfo(*GetFrame()).rect;
    const gfx::Rect window(window_features.x, window_features.y,
                           window_features.width, window_features.height);
    if (!screen.Contains(window)) {
      UseCounter::Count(
          *entered_window,
          WebFeature::kDOMWindowOpenPositioningFeaturesCrossScreen);
    }
  }

#if BUILDFLAG(IS_ANDROID)
  // Popup windows are handled just like new tabs on mobile today, but we might
  // want to change that. https://crbug.com/1364321
  if (window_features.is_popup) {
    UseCounter::Count(*entered_window, WebFeature::kWindowOpenPopupOnMobile);
  }
#endif

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
    result.frame->SetOpener(GetFrame());
  return result.frame->DomWindow();
}

DOMWindow* LocalDOMWindow::openPictureInPictureWindow(
    v8::Isolate* isolate,
    const WebPictureInPictureWindowOptions& options) {
  LocalDOMWindow* entered_window = EnteredDOMWindow(isolate);
  DCHECK(isSecureContext());

  if (!IsCurrentlyDisplayedInFrame() || !entered_window->GetFrame()) {
    return nullptr;
  }

  // If the bindings implementation is 100% correct, the current realm and the
  // entered realm should be same origin-domain. However, to be on the safe
  // side and add some defense in depth, we'll check against the entry realm
  // as well here.
  if (!BindingSecurity::ShouldAllowAccessTo(entered_window, this)) {
    // Trigger DCHECK() failure, while gracefully failing on release builds.
    NOTREACHED_IN_MIGRATION();
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kWindowOpenRealmMismatch);
    return nullptr;
  }

  FrameLoadRequest frame_request(entered_window,
                                 ResourceRequest(KURL(g_empty_string)));
  frame_request.SetPictureInPictureWindowOptions(options);

  // We always create a new window here.
  FrameTree::FindResult result =
      GetFrame()->Tree().FindOrCreateFrameForNavigation(frame_request,
                                                        AtomicString("_blank"));
  if (!result.frame)
    return nullptr;

  // A new window should always be created.
  DCHECK(result.new_window);

  result.frame->Navigate(frame_request, WebFrameLoadType::kStandard);
  LocalDOMWindow* pip_dom_window =
      To<LocalDOMWindow>(result.frame->DomWindow());
  pip_dom_window->SetIsPictureInPictureWindow();

  // Ensure that we're using the same compatibility mode as the opener document.
  pip_dom_window->document()->SetCompatibilityMode(
      entered_window->document()->GetCompatibilityMode());

  // Also copy any autoplay flags, since these are set on navigation commit.
  // The pip window should match whatever the opener has.
  auto* opener_page = entered_window->document()->GetPage();
  auto* pip_page = pip_dom_window->document()->GetPage();
  CHECK(opener_page);
  CHECK(pip_page);
  pip_page->ClearAutoplayFlags();
  pip_page->AddAutoplayFlags(opener_page->AutoplayFlags());

  return pip_dom_window;
}

void LocalDOMWindow::Trace(Visitor* visitor) const {
  visitor->Trace(script_controller_);
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
  visitor->Trace(external_);
  visitor->Trace(navigation_);
  visitor->Trace(viewport_);
  visitor->Trace(visualViewport_);
  visitor->Trace(event_listener_observers_);
  visitor->Trace(current_event_);
  visitor->Trace(trusted_types_map_);
  visitor->Trace(input_method_controller_);
  visitor->Trace(spell_checker_);
  visitor->Trace(text_suggestion_controller_);
  visitor->Trace(isolated_world_csp_map_);
  visitor->Trace(network_state_observer_);
  visitor->Trace(fence_);
  visitor->Trace(closewatcher_stack_);
  DOMWindow::Trace(visitor);
  ExecutionContext::Trace(visitor);
  Supplementable<LocalDOMWindow>::Trace(visitor);
}

bool LocalDOMWindow::CrossOriginIsolatedCapability() const {
  return Agent::IsCrossOriginIsolated() &&
         IsFeatureEnabled(
             mojom::blink::PermissionsPolicyFeature::kCrossOriginIsolated) &&
         GetPolicyContainer()->GetPolicies().allow_cross_origin_isolation;
}

bool LocalDOMWindow::IsIsolatedContext() const {
  return Agent::IsIsolatedContext();
}

ukm::UkmRecorder* LocalDOMWindow::UkmRecorder() {
  DCHECK(document_);
  return document_->UkmRecorder();
}

ukm::SourceId LocalDOMWindow::UkmSourceID() const {
  DCHECK(document_);
  return document_->UkmSourceID();
}

void LocalDOMWindow::SetStorageKey(const BlinkStorageKey& storage_key) {
  storage_key_ = storage_key;
}

bool LocalDOMWindow::IsPaymentRequestTokenActive() const {
  return payment_request_token_.IsActive();
}

bool LocalDOMWindow::ConsumePaymentRequestToken() {
  return payment_request_token_.ConsumeIfActive();
}

bool LocalDOMWindow::IsFullscreenRequestTokenActive() const {
  return fullscreen_request_token_.IsActive();
}

bool LocalDOMWindow::ConsumeFullscreenRequestToken() {
  return fullscreen_request_token_.ConsumeIfActive();
}

bool LocalDOMWindow::IsDisplayCaptureRequestTokenActive() const {
  return display_capture_request_token_.IsActive();
}

bool LocalDOMWindow::ConsumeDisplayCaptureRequestToken() {
  return display_capture_request_token_.ConsumeIfActive();
}

void LocalDOMWindow::SetIsInBackForwardCache(bool is_in_back_forward_cache) {
  ExecutionContext::SetIsInBackForwardCache(is_in_back_forward_cache);
  if (!is_in_back_forward_cache) {
    BackForwardCacheBufferLimitTracker::Get()
        .DidRemoveFrameOrWorkerFromBackForwardCache(
            total_bytes_buffered_while_in_back_forward_cache_);
    total_bytes_buffered_while_in_back_forward_cache_ = 0;
  }
}

void LocalDOMWindow::DidBufferLoadWhileInBackForwardCache(
    bool update_process_wide_count,
    size_t num_bytes) {
  total_bytes_buffered_while_in_back_forward_cache_ += num_bytes;
  if (update_process_wide_count) {
    BackForwardCacheBufferLimitTracker::Get().DidBufferBytes(num_bytes);
  }
}

bool LocalDOMWindow::credentialless() const {
  return GetExecutionContext()
      ->GetPolicyContainer()
      ->GetPolicies()
      .is_credentialless;
}

bool LocalDOMWindow::IsInFencedFrame() const {
  return GetFrame() && GetFrame()->IsInFencedFrameTree();
}

Fence* LocalDOMWindow::fence() {
  // Return nullptr if we aren't in a fenced subtree.
  if (!GetFrame()) {
    return nullptr;
  }
  if (!GetFrame()->IsInFencedFrameTree()) {
    // We temporarily allow window.fence in iframes with fenced frame reporting
    // metadata (navigated by urn:uuids).
    // If we are in an iframe that doesn't qualify, return nullptr.
    if (!blink::features::IsAllowURNsInIframeEnabled() ||
        !GetFrame()->GetDocument()->Loader()->FencedFrameProperties() ||
        !GetFrame()
             ->GetDocument()
             ->Loader()
             ->FencedFrameProperties()
             ->has_fenced_frame_reporting()) {
      return nullptr;
    }
  }

  if (!fence_) {
    fence_ = MakeGarbageCollected<Fence>(*this);
  }

  return fence_.Get();
}

bool LocalDOMWindow::IsPictureInPictureWindow() const {
  return is_picture_in_picture_window_;
}

void LocalDOMWindow::SetIsPictureInPictureWindow() {
  is_picture_in_picture_window_ = true;
}

net::StorageAccessApiStatus LocalDOMWindow::GetStorageAccessApiStatus() const {
  return storage_access_api_status_;
}

void LocalDOMWindow::SetStorageAccessApiStatus(
    net::StorageAccessApiStatus status) {
  CHECK_GE(status, storage_access_api_status_);
  storage_access_api_status_ = status;
}

void LocalDOMWindow::GenerateNewNavigationId() {
  navigation_id_ = WTF::CreateCanonicalUUIDString();
}

void LocalDOMWindow::SetHasBeenRevealed(bool revealed) {
  if (has_been_revealed_ == revealed)
    return;
  has_been_revealed_ = revealed;
  CHECK(document_);
  ViewTransitionSupplement::From(*document_)->DidChangeRevealState();
}

void LocalDOMWindow::UpdateEventListenerCountsToDocumentForReuseIfNeeded() {
  if (!is_dom_window_reused_) {
    return;
  }
  if (document_ && HasEventListeners()) {
    GetEventTargetData()->event_listener_map.ForAllEventListenerTypes(
        [this](const AtomicString& event_type, uint32_t count) {
          document_->AddListenerTypeIfNeeded(event_type, *this);
          document_->DidAddEventListeners(count);
        });
  }
  is_dom_window_reused_ = false;
}
}  // namespace blink
