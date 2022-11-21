// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/notifications/notification_data.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/common/notifications/notification_constants.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_notification_action.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_notification_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/notifications/notification.h"
#include "third_party/blink/renderer/modules/notifications/notification_metrics.h"
#include "third_party/blink/renderer/modules/notifications/timestamp_trigger.h"
#include "third_party/blink/renderer/modules/vibration/vibration_controller.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {
namespace {

mojom::blink::NotificationDirection ToDirectionEnumValue(
    const String& direction) {
  if (direction == "ltr")
    return mojom::blink::NotificationDirection::LEFT_TO_RIGHT;
  if (direction == "rtl")
    return mojom::blink::NotificationDirection::RIGHT_TO_LEFT;
  if (direction == "auto")
    return mojom::blink::NotificationDirection::AUTO;
  NOTREACHED() << "Unknown direction: " << direction;
  return mojom::blink::NotificationDirection::AUTO;
}

mojom::blink::NotificationScenario ToScenarioEnumValue(const String& scenario) {
  if (scenario == "default")
    return mojom::blink::NotificationScenario::DEFAULT;
  if (scenario == "incoming-call")
    return mojom::blink::NotificationScenario::INCOMING_CALL;
  NOTREACHED() << "Unknown scenario: " << scenario;
  return mojom::blink::NotificationScenario::DEFAULT;
}

KURL CompleteURL(ExecutionContext* context, const String& string_url) {
  KURL url = context->CompleteURL(string_url);
  if (url.IsValid())
    return url;
  return KURL();
}

}  // namespace

mojom::blink::NotificationDataPtr CreateNotificationData(
    ExecutionContext* context,
    const String& title,
    const NotificationOptions* options,
    ExceptionState& exception_state) {
  // If silent is true, the notification must not have a vibration pattern.
  if (options->hasVibrate() && options->silent()) {
    RecordPersistentNotificationDisplayResult(
        PersistentNotificationDisplayResult::kSilentWithVibrate);
    exception_state.ThrowTypeError(
        "Silent notifications must not specify vibration patterns.");
    return nullptr;
  }

  // If renotify is true, the notification must have a tag.
  if (options->renotify() && options->tag().empty()) {
    RecordPersistentNotificationDisplayResult(
        PersistentNotificationDisplayResult::kRenotifyWithoutTag);
    exception_state.ThrowTypeError(
        "Notifications which set the renotify flag must specify a non-empty "
        "tag.");
    return nullptr;
  }

  auto notification_data = mojom::blink::NotificationData::New();

  notification_data->title = title;
  notification_data->direction = ToDirectionEnumValue(options->dir());
  notification_data->lang = options->lang();
  notification_data->body = options->body();
  notification_data->tag = options->tag();

  if (options->hasImage() && !options->image().empty())
    notification_data->image = CompleteURL(context, options->image());

  if (options->hasIcon() && !options->icon().empty())
    notification_data->icon = CompleteURL(context, options->icon());

  if (options->hasBadge() && !options->badge().empty())
    notification_data->badge = CompleteURL(context, options->badge());

  VibrationController::VibrationPattern vibration_pattern;
  if (options->hasVibrate()) {
    vibration_pattern =
        VibrationController::SanitizeVibrationPattern(options->vibrate());
  }
  notification_data->vibration_pattern = Vector<int32_t>();
  notification_data->vibration_pattern->Append(vibration_pattern.data(),
                                               vibration_pattern.size());

  notification_data->timestamp = options->hasTimestamp()
                                     ? static_cast<double>(options->timestamp())
                                     : base::Time::Now().ToDoubleT() * 1000.0;
  notification_data->renotify = options->renotify();
  notification_data->silent = options->silent();
  notification_data->require_interaction = options->requireInteraction();

  // TODO(crbug.com/1070871, crbug.com/1070964): |data| member has a null value
  // as a default value, and we don't need |hasData()| check actually.
  if (options->hasData() && !options->data().IsNull()) {
    const ScriptValue& data = options->data();
    v8::Isolate* isolate = data.GetIsolate();
    DCHECK(isolate->InContext());
    SerializedScriptValue::SerializeOptions serialize_options;
    serialize_options.for_storage = SerializedScriptValue::kForStorage;
    scoped_refptr<SerializedScriptValue> serialized_script_value =
        SerializedScriptValue::Serialize(isolate, data.V8Value(),
                                         serialize_options, exception_state);
    if (exception_state.HadException()) {
      RecordPersistentNotificationDisplayResult(
          PersistentNotificationDisplayResult::kFailedToSerializeData);
      return nullptr;
    }

    notification_data->data = Vector<uint8_t>();
    notification_data->data->Append(
        serialized_script_value->Data(),
        base::checked_cast<wtf_size_t>(
            serialized_script_value->DataLengthInBytes()));
  }

  Vector<mojom::blink::NotificationActionPtr> actions;

  const size_t max_actions = Notification::maxActions();
  for (const NotificationAction* action : options->actions()) {
    if (actions.size() >= max_actions)
      break;

    auto notification_action = mojom::blink::NotificationAction::New();
    notification_action->action = action->action();
    notification_action->title = action->title();

    if (action->type() == "button") {
      notification_action->type = mojom::blink::NotificationActionType::BUTTON;
    } else if (action->type() == "text") {
      notification_action->type = mojom::blink::NotificationActionType::TEXT;
    } else {
      NOTREACHED() << "Unknown action type: "
                   << IDLEnumAsString(action->type());
    }

    if (!action->placeholder().IsNull() &&
        notification_action->type ==
            mojom::blink::NotificationActionType::BUTTON) {
      RecordPersistentNotificationDisplayResult(
          PersistentNotificationDisplayResult::kButtonActionWithPlaceholder);
      exception_state.ThrowTypeError(
          "Notifications of type \"button\" cannot specify a placeholder.");
      return nullptr;
    }

    notification_action->placeholder = action->placeholder();

    if (action->hasIcon() && !action->icon().empty())
      notification_action->icon = CompleteURL(context, action->icon());

    actions.push_back(std::move(notification_action));
  }

  notification_data->actions = std::move(actions);

  if (options->hasShowTrigger()) {
    UseCounter::Count(context, WebFeature::kNotificationShowTrigger);

    auto* timestamp_trigger = options->showTrigger();
    auto timestamp = base::Time::FromJsTime(timestamp_trigger->timestamp());

    if (timestamp - base::Time::Now() > kMaxNotificationShowTriggerDelay) {
      RecordPersistentNotificationDisplayResult(
          PersistentNotificationDisplayResult::kShowTriggerDelayTooFarAhead);
      exception_state.ThrowTypeError(
          "Notification trigger timestamp too far ahead in the future.");
      return nullptr;
    }

    notification_data->show_trigger_timestamp = timestamp;
  }

  notification_data->scenario = ToScenarioEnumValue(options->scenario());

  return notification_data;
}

}  // namespace blink
