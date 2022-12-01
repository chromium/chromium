// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/alarms/alarms_api.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/browser/api/alarms/alarms_api_constants.h"
#include "extensions/common/api/alarms.h"
#include "extensions/common/error_utils.h"

namespace extensions {

namespace alarms = api::alarms;

namespace {

const char kDefaultAlarmName[] = "";
const char kBothRelativeAndAbsoluteTime[] =
    "Cannot set both when and delayInMinutes.";
const char kNoScheduledTime[] =
    "Must set at least one of when, delayInMinutes, or periodInMinutes.";

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
  if (create_info.delay_in_minutes) {
    if (*create_info.delay_in_minutes <
        alarms_api_constants::kReleaseDelayMinimum) {
      if (Manifest::IsUnpackedLocation(extension->location())) {
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            alarms_api_constants::kWarningMinimumDevDelay, alarm_name));
      } else {
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            alarms_api_constants::kWarningMinimumReleaseDelay, alarm_name));
      }
    }
  }
  if (create_info.period_in_minutes) {
    if (*create_info.period_in_minutes <
        alarms_api_constants::kReleaseDelayMinimum) {
      if (Manifest::IsUnpackedLocation(extension->location())) {
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            alarms_api_constants::kWarningMinimumDevPeriod, alarm_name));
      } else {
        warnings->push_back(ErrorUtils::FormatErrorMessage(
            alarms_api_constants::kWarningMinimumReleasePeriod, alarm_name));
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

AlarmsCreateFunction::~AlarmsCreateFunction() {}

ExtensionFunction::ResponseAction AlarmsCreateFunction::Run() {
  std::unique_ptr<alarms::Create::Params> params(
      alarms::Create::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const std::string& alarm_name = params->name.value_or(kDefaultAlarmName);
  std::vector<std::string> warnings;
  std::string error;
  if (!ValidateAlarmCreateInfo(alarm_name, params->alarm_info, extension(),
                               &error, &warnings)) {
    return RespondNow(Error(std::move(error)));
  }
  for (std::vector<std::string>::const_iterator it = warnings.begin();
       it != warnings.end(); ++it)
    WriteToConsole(blink::mojom::ConsoleMessageLevel::kWarning, *it);

  const int kSecondsPerMinute = 60;
  base::TimeDelta granularity =
      base::Seconds((Manifest::IsUnpackedLocation(extension()->location())
                         ? alarms_api_constants::kDevDelayMinimum
                         : alarms_api_constants::kReleaseDelayMinimum)) *
      kSecondsPerMinute;

  Alarm alarm(alarm_name, params->alarm_info, granularity, clock_->Now());
  AlarmManager::Get(browser_context())
      ->AddAlarm(extension_id(), std::move(alarm),
                 base::BindOnce(&AlarmsCreateFunction::Callback, this));

  // AddAlarm might have already responded.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AlarmsCreateFunction::Callback() {
  Respond(NoArguments());
}

ExtensionFunction::ResponseAction AlarmsGetFunction::Run() {
  std::unique_ptr<alarms::Get::Params> params(
      alarms::Get::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

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
  Respond(OneArgument(base::Value(std::move(alarms_value))));
}

ExtensionFunction::ResponseAction AlarmsClearFunction::Run() {
  std::unique_ptr<alarms::Clear::Params> params(
      alarms::Clear::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string name = params->name.value_or(kDefaultAlarmName);
  AlarmManager::Get(browser_context())
      ->RemoveAlarm(extension_id(), name,
                    base::BindOnce(&AlarmsClearFunction::Callback, this, name));

  // RemoveAlarm might have already responded.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AlarmsClearFunction::Callback(const std::string& name, bool success) {
  Respond(OneArgument(base::Value(success)));
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
  Respond(OneArgument(base::Value(true)));
}

}  // namespace extensions
