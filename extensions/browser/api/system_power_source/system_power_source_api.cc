// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_power_source/system_power_source_api.h"

#include <cmath>

#include "base/no_destructor.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/system_power_source.h"

namespace extensions {

namespace {

std::unique_ptr<double> RoundDownToTwoSignificantDigits(double d) {
  if (!std::isnormal(d) || d <= 0.0)
    return nullptr;

  int scale = std::floor(std::log10(d));
  return std::make_unique<double>(std::floor(d / std::pow(10, scale - 1)) *
                                  std::pow(10, scale - 1));
}

api::system_power_source::PowerSourceType PowerSourceTypeFromProtoValue(
    power_manager::PowerSupplyProperties_PowerSource_Type type) {
  switch (type) {
    case power_manager::PowerSupplyProperties_PowerSource_Type_OTHER:
      return api::system_power_source::POWER_SOURCE_TYPE_UNKNOWN;

    case power_manager::PowerSupplyProperties_PowerSource_Type_MAINS:
      return api::system_power_source::POWER_SOURCE_TYPE_MAINS;

    case power_manager::PowerSupplyProperties_PowerSource_Type_USB_C:
    case power_manager::PowerSupplyProperties_PowerSource_Type_USB_BC_1_2:
      return api::system_power_source::POWER_SOURCE_TYPE_USB;
  }
  NOTREACHED();
  return api::system_power_source::POWER_SOURCE_TYPE_UNKNOWN;
}

std::vector<api::system_power_source::PowerSourceInfo>
PowerSourceInfoVectorFromProtoValue(
    const power_manager::PowerSupplyProperties& proto) {
  std::vector<api::system_power_source::PowerSourceInfo> power_sources;
  base::Optional<std::string> external_power_source_id;

  if (proto.has_external_power_source_id())
    external_power_source_id = proto.external_power_source_id();

  power_sources.reserve(proto.available_external_power_source_size());
  for (int i = 0; i < proto.available_external_power_source_size(); ++i) {
    const power_manager::PowerSupplyProperties_PowerSource& power_source_in =
        proto.available_external_power_source(i);
    api::system_power_source::PowerSourceInfo power_source_out;

    power_source_out.type =
        power_source_in.has_type()
            ? PowerSourceTypeFromProtoValue(power_source_in.type())
            : api::system_power_source::POWER_SOURCE_TYPE_UNKNOWN;

    if (power_source_in.has_max_power()) {
      // Round to two significant digits for privacy reasons, to reduce the risk
      // of finger-printing.
      power_source_out.max_power =
          RoundDownToTwoSignificantDigits(power_source_in.max_power());
    }

    power_source_out.active =
        external_power_source_id.has_value() &&
        external_power_source_id.value() == power_source_in.id();

    power_sources.push_back(std::move(power_source_out));
  }

  return power_sources;
}

}  // namespace

// static
BrowserContextKeyedAPIFactory<SystemPowerSourceAPI>*
SystemPowerSourceAPI::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<SystemPowerSourceAPI>>
      instance;
  return instance.get();
}

SystemPowerSourceAPI::SystemPowerSourceAPI(content::BrowserContext* context)
    : browser_context_(context), power_manager_observer_(this) {
  power_manager_observer_.Add(chromeos::PowerManagerClient::Get());
}

SystemPowerSourceAPI::~SystemPowerSourceAPI() = default;

void SystemPowerSourceAPI::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;

  std::unique_ptr<base::ListValue> args =
      api::system_power_source::OnPowerChanged::Create(
          PowerSourceInfoVectorFromProtoValue(proto));

  auto event = std::make_unique<Event>(
      events::SYSTEM_POWER_SOURCE_ONPOWERCHANGED,
      api::system_power_source::OnPowerChanged::kEventName, std::move(args));
  event_router->BroadcastEvent(std::move(event));
}

SystemPowerSourceGetPowerSourceInfoFunction::
    SystemPowerSourceGetPowerSourceInfoFunction() = default;

SystemPowerSourceGetPowerSourceInfoFunction::
    ~SystemPowerSourceGetPowerSourceInfoFunction() = default;

ExtensionFunction::ResponseAction
SystemPowerSourceGetPowerSourceInfoFunction::Run() {
  const base::Optional<power_manager::PowerSupplyProperties>&
      power_supply_properties =
          chromeos::PowerManagerClient::Get()->GetLastStatus();

  if (!power_supply_properties.has_value())
    return RespondNow(NoArguments());

  return RespondNow(ArgumentList(
      api::system_power_source::GetPowerSourceInfo::Results::Create(
          PowerSourceInfoVectorFromProtoValue(*power_supply_properties))));
}

SystemPowerSourceRequestStatusUpdateFunction::
    SystemPowerSourceRequestStatusUpdateFunction() = default;

SystemPowerSourceRequestStatusUpdateFunction::
    ~SystemPowerSourceRequestStatusUpdateFunction() = default;

ExtensionFunction::ResponseAction
SystemPowerSourceRequestStatusUpdateFunction::Run() {
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
  return RespondNow(NoArguments());
}

}  // namespace extensions
