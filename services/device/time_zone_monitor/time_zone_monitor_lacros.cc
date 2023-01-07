// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor_lacros.h"

#include "base/memory/ptr_util.h"
#include "chromeos/lacros/lacros_service.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace device {

TimeZoneMonitorLacros::TimeZoneMonitorLacros() {
  auto* lacros_service = chromeos::LacrosService::Get();

  // Just DCHECK here, because this is ensured by TimeZoneMonitor::Create() in
  // time_zone_monitor_linux.cc now.
  // TODO(crbug.com/1288168): when the old ash-chrome no longer needs to be
  // supported, clean up the dependencies.
  DCHECK(lacros_service);
  DCHECK(lacros_service->IsAvailable<crosapi::mojom::TimeZoneService>());
  lacros_service->GetRemote<crosapi::mojom::TimeZoneService>()->AddObserver(
      receiver_.BindNewPipeAndPassRemoteWithVersion());
}

TimeZoneMonitorLacros::~TimeZoneMonitorLacros() = default;

void TimeZoneMonitorLacros::OnTimeZoneChanged(
    const std::u16string& time_zone_id) {
  UpdateIcuAndNotifyClients(base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString(time_zone_id.c_str(), time_zone_id.size()))));
}

}  // namespace device
