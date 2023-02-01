// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor_lacros.h"

#include "base/memory/ptr_util.h"
#include "chromeos/lacros/lacros_service.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace device {

TimeZoneMonitorLacros::TimeZoneMonitorLacros() {
  // On unit_tests/browser_tests, LacrosService may not be available.
  if (auto* lacros_service = chromeos::LacrosService::Get();
      lacros_service &&
      lacros_service->IsAvailable<crosapi::mojom::TimeZoneService>()) {
    lacros_service->GetRemote<crosapi::mojom::TimeZoneService>()->AddObserver(
        receiver_.BindNewPipeAndPassRemoteWithVersion());
  }
}

TimeZoneMonitorLacros::~TimeZoneMonitorLacros() = default;

void TimeZoneMonitorLacros::OnTimeZoneChanged(
    const std::u16string& time_zone_id) {
  UpdateIcuAndNotifyClients(base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString(time_zone_id.c_str(), time_zone_id.size()))));
}

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::make_unique<TimeZoneMonitorLacros>();
}

}  // namespace device
