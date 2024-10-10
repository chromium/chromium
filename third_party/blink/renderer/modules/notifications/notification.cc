/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/notifications/notification.h"

#include <memory>
#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_notification_action.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_notification_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/scoped_window_focus_allowed_indicator.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/performance_monitor.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/notifications/notification_data.h"
#include "third_party/blink/renderer/modules/notifications/notification_manager.h"
#include "third_party/blink/renderer/modules/notifications/notification_resources_loader.h"
#include "third_party/blink/renderer/modules/notifications/timestamp_trigger.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

Notification* Notification::Create(ExecutionContext* context,
                                   const String& title,
                                   const NotificationOptions* options,
                                   ExceptionState& exception_state) {
  // The Notification constructor may be disabled through a runtime feature when
  // the platform does not support non-persistent notifications.
  if (!RuntimeEnabledFeatures::NotificationConstructorEnabled()) {
    exception_state.ThrowTypeError(
        "Illegal constructor. Use ServiceWorkerRegistration.showNotification() "
        "instead.");
    return nullptr;
  }

  // The Notification constructor may not be used in Service Worker contexts.
  if (context->IsServiceWorkerGlobalScope()) {
    exception_state.ThrowTypeError("Illegal constructor.");
    return nullptr;
  }

  if (!options->actions().empty()) {
    exception_state.ThrowTypeError(
        "Actions are only supported for persistent notifications shown using "
        "ServiceWorkerRegistration.showNotification().");
    return nullptr;
  }

  if (options->hasShowTrigger()) {
    exception_state.ThrowTypeError(
        "ShowTrigger is only supported for persistent notifications shown "
        "using ServiceWorkerRegistration.showNotification().");
    return nullptr;
  }

  auto* window = DynamicTo<LocalDOMWindow>(context);
  if (context->IsSecureContext()) {
    UseCounter::Count(context, WebFeature::kNotificationSecureOrigin);
    if (window) {
      window->CountUseOnlyInCrossOriginIframe(
          WebFeature::kNotificationAPISecureOriginIframe);
    }
  } else {
    Deprecation::CountDeprecation(context,
                                  WebFeature::kNotificationInsecureOrigin);
    if (window) {
      Deprecation::CountDeprecationCrossOriginIframe(
          window, WebFeature::kNotificationAPIInsecureOriginIframe);
    }
  }

  mojom::blink::NotificationDataPtr data =
      CreateNotificationData(context, title, options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  if (context->IsContextDestroyed()) {
    exception_state.ThrowTypeError("Illegal invocation.");
    return nullptr;
  }

  Notification* notification = MakeGarbageCollected<Notification>(
      context, Type::kNonPersistent, std::move(data));

  // TODO(https://crbug.com/595685): Make |token| a constructor parameter
  // once persistent notifications have been mojofied too.
  if (notification->tag().IsNull() || notification->tag().empty()) {
    auto unguessable_token = base::UnguessableToken::Create();
    notification->SetToken(unguessable_token.ToString().c_str());
  } else {
    notification->SetToken(notification->tag());
  }

  notification->SchedulePrepareShow();

  if (window) {
    if (auto* document_resource_coordinator =
            window->document()->GetResourceCoordinator()) {
      document_resource_coordinator->OnNonPersistentNotificationCreated();
    }
  }

  return notification;
}

Notification* Notification::Create(ExecutionContext* context,
                                   const String& notification_id,
                                   mojom::blink::NotificationDataPtr data,
                                   bool showing) {
  Notification* notification = MakeGarbageCollected<Notification>(
      context, Type::kPersistent, std::move(data));
  notification->SetState(showing ? State::kShowing : State::kClosed);
  notification->SetNotificationId(notification_id);
  return notification;
}

Notification::Notification(ExecutionContext* context,
                           Type type,
                           mojom::blink::NotificationDataPtr data)
    : ActiveScriptWrappable<Notification>({}),
      ExecutionContextLifecycleObserver(context),
      type_(type),
      state_(State::kLoading),
      data_(std::move(data)),
      prepare_show_timer_(context->GetTaskRunner(TaskType::kMiscPlatformAPI),
                          this,
                          &Notification::PrepareShow),
      listener_receiver_(this, context) {
  if (data_->show_trigger_timestamp.has_value()) {
    show_trigger_ = TimestampTrigger::Create(static_cast<DOMTimeStamp>(
        data_->show_trigger_timestamp.value().InMillisecondsFSinceUnixEpoch()));
  }
}

Notification::~Notification() = default;

void Notification::SchedulePrepareShow() {
  DCHECK_EQ(state_, State::kLoading);

  prepare_show_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void Notification::PrepareShow(TimerBase*) {
  DCHECK_EQ(state_, State::kLoading);
  if (!GetExecutionContext()->IsSecureContext()) {
    DispatchErrorEvent();
    return;
  }

  if (NotificationManager::From(GetExecutionContext())->GetPermissionStatus() !=
      mojom::blink::PermissionStatus::GRANTED) {
    DispatchErrorEvent();
    return;
  }

  loader_ = MakeGarbageCollected<NotificationResourcesLoader>(
      WTF::BindOnce(&Notification::DidLoadResources, WrapWeakPersistent(this)));
  loader_->Start(GetExecutionContext(), *data_);
}

void Notification::DidLoadResources(NotificationResourcesLoader* loader) {
  DCHECK_EQ(loader, loader_.Get());

  mojo::PendingRemote<mojom::blink::NonPersistentNotificationListener>
      event_listener;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetExecutionContext()->GetTaskRunner(blink::TaskType::kInternalDefault);
  listener_receiver_.Bind(event_listener.InitWithNewPipeAndPassReceiver(),
                          task_runner);

  NotificationManager::From(GetExecutionContext())
      ->DisplayNonPersistentNotification(token_, data_->Clone(),
                                         loader->GetResources(),
                                         std::move(event_listener));

  loader_.Clear();

  state_ = State::kShowing;
}

void Notification::close() {
  if (state_ != State::kShowing)
    return;

  // Schedule the "close" event to be fired for non-persistent notifications.
  // Persistent notifications won't get such events for programmatic closes.
  if (type_ == Type::kNonPersistent) {
    state_ = State::kClosing;
    NotificationManager::From(GetExecutionContext())
        ->CloseNonPersistentNotification(token_);
    return;
  }

  state_ = State::kClosed;

  NotificationManager::From(GetExecutionContext())
      ->ClosePersistentNotification(notification_id_);
}

void Notification::OnShow() {
  DispatchEvent(*Event::Create(event_type_names::kShow));
}

void Notification::OnClick(OnClickCallback completed_closure) {
  ExecutionContext* context = GetExecutionContext();
  auto* window = DynamicTo<LocalDOMWindow>(context);
  if (window && window->GetFrame()) {
    LocalFrame::NotifyUserActivation(
        window->GetFrame(),
        mojom::blink::UserActivationNotificationType::kInteraction);
  }
  ScopedWindowFocusAllowedIndicator window_focus_allowed(GetExecutionContext());
  DispatchEvent(*Event::Create(event_type_names::kClick));

  std::move(completed_closure).Run();
}

void Notification::OnClose(OnCloseCallback completed_closure) {
  // The notification should be Showing if the user initiated the close, or it
  // should be Closing if the developer initiated the close.
  if (state_ == State::kShowing || state_ == State::kClosing) {
    state_ = State::kClosed;
    DispatchEvent(*Event::Create(event_type_names::kClose));
  }
  std::move(completed_closure).Run();
}

void Notification::DispatchErrorEvent() {
  DispatchEvent(*Event::Create(event_type_names::kError));
}

String Notification::title() const {
  return data_->title;
}

String Notification::dir() const {
  switch (data_->direction) {
    case mojom::blink::NotificationDirection::LEFT_TO_RIGHT:
      return "ltr";
    case mojom::blink::NotificationDirection::RIGHT_TO_LEFT:
      return "rtl";
    case mojom::blink::NotificationDirection::AUTO:
      return "auto";
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

String Notification::lang() const {
  return data_->lang;
}

String Notification::body() const {
  return data_->body;
}

String Notification::tag() const {
  return data_->tag;
}

String Notification::image() const {
  return data_->image.GetString();
}

String Notification::icon() const {
  return data_->icon.GetString();
}

String Notification::badge() const {
  return data_->badge.GetString();
}

VibrationController::VibrationPattern Notification::vibrate() const {
  VibrationController::VibrationPattern pattern;
  if (data_->vibration_pattern.has_value()) {
    pattern.AppendRange(data_->vibration_pattern->begin(),
                        data_->vibration_pattern->end());
  }

  return pattern;
}

DOMTimeStamp Notification::timestamp() const {
  return data_->timestamp;
}

bool Notification::renotify() const {
  return data_->renotify;
}

bool Notification::silent() const {
  return data_->silent;
}

bool Notification::requireInteraction() const {
  return data_->require_interaction;
}

ScriptValue Notification::data(ScriptState* script_state) {
  base::span<const uint8_t> data;
  if (data_->data.has_value()) {
    data = data_->data.value();
  }
  scoped_refptr<SerializedScriptValue> serialized_value =
      SerializedScriptValue::Create(data);

  return ScriptValue(script_state->GetIsolate(),
                     serialized_value->Deserialize(script_state->GetIsolate()));
}

v8::LocalVector<v8::Value> Notification::actions(
    ScriptState* script_state) const {
  v8::LocalVector<v8::Value> result(script_state->GetIsolate());
  if (!data_->actions.has_value())
    return result;

  const Vector<mojom::blink::NotificationActionPtr>& actions =
      data_->actions.value();
  result.resize(actions.size());
  for (wtf_size_t i = 0; i < actions.size(); ++i) {
    NotificationAction* action = NotificationAction::Create();

    switch (actions[i]->type) {
      case mojom::blink::NotificationActionType::BUTTON:
        action->setType("button");
        break;
      case mojom::blink::NotificationActionType::TEXT:
        action->setType("text");
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unknown action type: " << actions[i]->type;
    }

    action->setAction(actions[i]->action);
    action->setTitle(actions[i]->title);
    action->setIcon(actions[i]->icon.GetString());
    action->setPlaceholder(actions[i]->placeholder);

    // Both the Action dictionaries themselves and the sequence they'll be
    // returned in are expected to the frozen. This cannot be done with
    // WebIDL.
    result[i] = FreezeV8Object(
        ToV8Traits<NotificationAction>::ToV8(script_state, action),
        script_state->GetIsolate());
  }

  return result;
}

String Notification::scenario() const {
  switch (data_->scenario) {
    case mojom::blink::NotificationScenario::DEFAULT:
      return "default";
    case mojom::blink::NotificationScenario::INCOMING_CALL:
      return "incoming-call";
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

V8NotificationPermission::Enum Notification::PermissionToV8Enum(
    mojom::blink::PermissionStatus permission) {
  switch (permission) {
    case mojom::blink::PermissionStatus::GRANTED:
      return V8NotificationPermission::Enum::kGranted;
    case mojom::blink::PermissionStatus::DENIED:
      return V8NotificationPermission::Enum::kDenied;
    case mojom::blink::PermissionStatus::ASK:
      return V8NotificationPermission::Enum::kDefault;
  }
  NOTREACHED();
}

V8NotificationPermission Notification::permission(ExecutionContext* context) {
  // Permission is always denied for insecure contexts. Skip the sync IPC call.
  if (!context->IsSecureContext()) {
    return V8NotificationPermission(V8NotificationPermission::Enum::kDenied);
  }

  // If the current global object's browsing context is a prerendering browsing
  // context, then return "default".
  // https://wicg.github.io/nav-speculation/prerendering.html#patch-notifications
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    if (Document* document = window->document(); document->IsPrerendering()) {
      return V8NotificationPermission(V8NotificationPermission::Enum::kDefault);
    }
  }

  mojom::blink::PermissionStatus status =
      NotificationManager::From(context)->GetPermissionStatus();

  // Permission can only be requested from top-level frames and same-origin
  // iframes. This should be reflected in calls getting permission status.
  //
  // TODO(crbug.com/758603): Move this check to the browser process when the
  // NotificationService connection becomes frame-bound.
  if (status == mojom::blink::PermissionStatus::ASK) {
    auto* window = DynamicTo<LocalDOMWindow>(context);
    LocalFrame* frame = window ? window->GetFrame() : nullptr;
    if (!frame || frame->IsCrossOriginToOutermostMainFrame())
      status = mojom::blink::PermissionStatus::DENIED;
  }

  return V8NotificationPermission(PermissionToV8Enum(status));
}

ScriptPromise<V8NotificationPermission> Notification::requestPermission(
    ScriptState* script_state,
    V8NotificationPermissionCallback* deprecated_callback) {
  if (!script_state->ContextIsValid())
    return EmptyPromise();

  ExecutionContext* context = ExecutionContext::From(script_state);

  probe::BreakableLocation(context, "Notification.requestPermission");
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    if (!LocalFrame::HasTransientUserActivation(window->GetFrame())) {
      PerformanceMonitor::ReportGenericViolation(
          context, PerformanceMonitor::kDiscouragedAPIUse,
          "Only request notification permission in response to a user gesture.",
          base::TimeDelta(), nullptr);
    }

    // Sites cannot request notification permission from cross-origin iframes,
    // but they can use notifications if permission had already been granted.
    if (window->GetFrame()->IsCrossOriginToOutermostMainFrame()) {
      Deprecation::CountDeprecation(
          context, WebFeature::kNotificationPermissionRequestedIframe);
    }
  }

  // Sites cannot request notification permission from insecure contexts.
  if (!context->IsSecureContext()) {
    Deprecation::CountDeprecation(
        context, WebFeature::kNotificationPermissionRequestedInsecureOrigin);
  }

  return NotificationManager::From(context)->RequestPermission(
      script_state, deprecated_callback);
}

uint32_t Notification::maxActions() {
  return kNotificationMaxActions;
}

DispatchEventResult Notification::DispatchEventInternal(Event& event) {
  DCHECK(GetExecutionContext()->IsContextThread());
  return EventTarget::DispatchEventInternal(event);
}

const AtomicString& Notification::InterfaceName() const {
  return event_target_names::kNotification;
}

void Notification::ContextDestroyed() {
  state_ = State::kClosed;

  if (prepare_show_timer_.IsActive())
    prepare_show_timer_.Stop();

  if (loader_)
    loader_->Stop();
}

bool Notification::HasPendingActivity() const {
  // Non-persistent notification can receive events until they've been closed.
  // Persistent notifications should be subject to regular garbage collection.
  if (type_ == Type::kNonPersistent)
    return state_ != State::kClosed;

  return false;
}

void Notification::Trace(Visitor* visitor) const {
  visitor->Trace(show_trigger_);
  visitor->Trace(prepare_show_timer_);
  visitor->Trace(loader_);
  visitor->Trace(listener_receiver_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
