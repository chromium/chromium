// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/alarms/alarms_api.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/browser/api/alarms/alarms_api_constants.h"
#include "extensions/common/api/alarms.h"
#include "extensions/common/error_utils.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace extensions {

namespace alarms = api::alarms;

namespace {

constexpr char kDefaultAlarmName[] = "";
constexpr char kBothRelativeAndAbsoluteTime[] =
    "Cannot set both when and delayInMinutes.";
constexpr char kNoScheduledTime[] =
    "Must set at least one of when, delayInMinutes, or periodInMinutes.";
constexpr char kMaxAlarmsError[] =
    "An extension cannot have more than %d active alarms.";

bool ValidateAlarmCreateInfo(const std::string& alarm_name,
                             const alarms::AlarmCreateInfo& create_info,
                             const Extension* extension,
                             std::string* error,
                             std::vector<std::string>* warnings) {
  if (create_info.delay_in_minutes && create_info.when) {
    *error = kBothRelativeAndAbsoluteTime;
    return false;
  }
  if (!create_info.delay_in_minutes.has_value() &&
      !create_info.when.has_value() &&
      !create_info.period_in_minutes.has_value()) {
    *error = kNoScheduledTime;
    return false;
  }

  // Users can always use an absolute timeout to request an arbitrarily-short or
  // negative delay.  We won't honor the short timeout, but we can't check it
  // and warn the user because it would introduce race conditions (say they
  // compute a long-enough timeout, but then the call into the alarms interface
  // gets delayed past the boundary).  However, it's still worth warning about
  // relative delays that are shorter than we'll honor.
  // For this minimum delay, we compare to the minimum of a packed extension,
  // since we want to appropriately warn developers (we take into account the
  // "real" unpacked state in the phrasing of the warning).
  const base::TimeDelta min_packed_delay =
      alarms_api_constants::GetMinimumDelay(/*is_unpacked=*/false,
                                            extension->manifest_version());
  const bool is_unpacked = Manifest::IsUnpackedLocation(extension->location());
  if (create_info.delay_in_minutes) {
    if (base::Minutes(*create_info.delay_in_minutes) < min_packed_delay) {
      if (is_unpacked) {
        warnings->push_back(base::StringPrintf(
            alarms_api_constants::kWarningMinimumDevDelay, "delay",
            min_packed_delay.InSeconds(), alarm_name.c_str()));
      } else {
        warnings->push_back(base::StringPrintf(
            alarms_api_constants::kWarningMinimumReleaseDelay, "delay",
            min_packed_delay.InSeconds(), alarm_name.c_str()));
      }
    }
  }
  if (create_info.period_in_minutes) {
    if (base::Minutes(*create_info.period_in_minutes) < min_packed_delay) {
      if (is_unpacked) {
        warnings->push_back(base::StringPrintf(
            alarms_api_constants::kWarningMinimumDevDelay, "period",
            min_packed_delay.InSeconds(), alarm_name.c_str()));
      } else {
        warnings->push_back(base::StringPrintf(
            alarms_api_constants::kWarningMinimumReleaseDelay, "period",
            min_packed_delay.InSeconds(), alarm_name.c_str()));
      }
    }
  }

  return true;
}

}  // namespace

AlarmsCreateFunction::AlarmsCreateFunction()
    : clock_(base::DefaultClock::GetInstance()) {}

AlarmsCreateFunction::AlarmsCreateFunction(base::Clock* clock)
    : clock_(clock) {}

AlarmsCreateFunction::~AlarmsCreateFunction() = default;

ExtensionFunction::ResponseAction AlarmsCreateFunction::Run() {
  std::optional<alarms::Create::Params> params =
      alarms::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  AlarmManager* const alarm_manager = AlarmManager::Get(browser_context());
  EXTENSION_FUNCTION_VALIDATE(alarm_manager);

  if (alarm_manager->GetCountForExtension(extension_id()) >=
      AlarmManager::kMaxAlarmsPerExtension) {
    return RespondNow(Error(base::StringPrintf(
        kMaxAlarmsError, AlarmManager::kMaxAlarmsPerExtension)));
  }

  const std::string& alarm_name = params->name.value_or(kDefaultAlarmName);
  std::vector<std::string> warnings;
  std::string error;
  if (!ValidateAlarmCreateInfo(alarm_name, params->alarm_info, extension(),
                               &error, &warnings)) {
    return RespondNow(Error(std::move(error)));
  }
  for (const std::string& warning : warnings) {
    WriteToConsole(blink::mojom::ConsoleMessageLevel::kWarning, warning);
  }

  base::TimeDelta granularity = alarms_api_constants::GetMinimumDelay(
      Manifest::IsUnpackedLocation(extension()->location()),
      extension()->manifest_version());
  Alarm alarm(alarm_name, params->alarm_info, granularity, clock_->Now());
  alarm_manager->AddAlarm(
      extension_id(), std::move(alarm),
      base::BindOnce(&AlarmsCreateFunction::Callback, this));

  // AddAlarm might have already responded.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AlarmsCreateFunction::Callback() {
  Respond(NoArguments());
}

ExtensionFunction::ResponseAction AlarmsGetFunction::Run() {
  std::optional<alarms::Get::Params> params =
      alarms::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string name = params->name.value_or(kDefaultAlarmName);
  AlarmManager::Get(browser_context())
      ->GetAlarm(extension_id(), name,
                 base::BindOnce(&AlarmsGetFunction::Callback, this, name));

  // GetAlarm might have already responded.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AlarmsGetFunction::Callback(const std::string& name,
                                 extensions::Alarm* alarm) {
  if (alarm) {
    Respond(ArgumentList(alarms::Get::Results::Create(*alarm->js_alarm)));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction AlarmsGetAllFunction::Run() {
  AlarmManager::Get(browser_context())
      ->GetAllAlarms(extension_id(),
                     base::BindOnce(&AlarmsGetAllFunction::Callback, this));
  // GetAllAlarms might have already responded.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AlarmsGetAllFunction::Callback(const AlarmList* alarms) {
  base::Value::List alarms_value;
  if (alarms) {
    for (const auto& alarm : *alarms)
      alarms_value.Append(alarm.js_alarm->ToValue());
  }
  Respond(WithArguments(std::move(alarms_value)));
}

ExtensionFunction::ResponseAction AlarmsClearFunction::Run() {
  std::optional<alarms::Clear::Params> params =
      alarms::Clear::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string name = params->name.value_or(kDefaultAlarmName);
  AlarmManager::Get(browser_context())
      ->RemoveAlarm(extension_id(), name,
                    base::BindOnce(&AlarmsClearFunction::Callback, this, name));

  // RemoveAlarm might have already responded.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AlarmsClearFunction::Callback(const std::string& name, bool success) {
  Respond(WithArguments(success));
}

ExtensionFunction::ResponseAction AlarmsClearAllFunction::Run() {
  AlarmManager::Get(browser_context())
      ->RemoveAllAlarms(
          extension_id(),
          base::BindOnce(&AlarmsClearAllFunction::Callback, this));
  // RemoveAllAlarms might have already responded.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AlarmsClearAllFunction::Callback() {
  Respond(WithArguments(true));
}

}  // namespace extensions
