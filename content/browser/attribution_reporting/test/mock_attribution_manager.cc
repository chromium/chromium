// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/test/mock_attribution_manager.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/storable_source.h"

namespace content {

MockAttributionManager::MockAttributionManager() = default;

MockAttributionManager::~MockAttributionManager() = default;

void MockAttributionManager::AddObserver(AttributionObserver* observer) {
  observers_.AddObserver(observer);
  if (on_observer_registered_) {
    std::move(on_observer_registered_).Run();
  }
  observer->OnDebugModeChanged(/*debug_mode=*/false);
}

void MockAttributionManager::RemoveObserver(AttributionObserver* observer) {
  observers_.RemoveObserver(observer);
}

AttributionDataHostManager* MockAttributionManager::GetDataHostManager() {
  DCHECK(data_host_manager_);
  return data_host_manager_.get();
}

void MockAttributionManager::NotifySourcesChanged() {
  for (auto& observer : observers_) {
    observer.OnSourcesChanged();
  }
}

void MockAttributionManager::NotifyReportsChanged() {
  for (auto& observer : observers_) {
    observer.OnReportsChanged();
  }
}

void MockAttributionManager::NotifySourceHandled(
    const StorableSource& source,
    StorableSource::Result result,
    std::optional<uint64_t> cleared_debug_key) {
  base::Time now = base::Time::Now();
  for (auto& observer : observers_) {
    observer.OnSourceHandled(source, now, cleared_debug_key, result);
  }
}

void MockAttributionManager::NotifyReportSent(const AttributionReport& report,
                                              bool is_debug_report,
                                              const SendResult& info) {
  for (auto& observer : observers_) {
    observer.OnReportSent(report, is_debug_report, info);
  }
}

void MockAttributionManager::NotifyTriggerHandled(
    const CreateReportResult& result,
    std::optional<uint64_t> cleared_debug_key) {
  for (auto& observer : observers_) {
    observer.OnTriggerHandled(cleared_debug_key, result);
  }
}

void MockAttributionManager::NotifyDebugReportSent(
    const AttributionDebugReport& report,
    const int status,
    const base::Time time) {
  for (auto& observer : observers_) {
    observer.OnDebugReportSent(report, status, time);
  }
}

void MockAttributionManager::NotifyAggregatableDebugReportSent(
    const AggregatableDebugReport& report,
    base::ValueView report_body,
    attribution_reporting::mojom::ProcessAggregatableDebugReportResult
        process_result,
    const SendAggregatableDebugReportResult& send_result) {
  for (auto& observer : observers_) {
    observer.OnAggregatableDebugReportSent(report, report_body, process_result,
                                           send_result);
  }
}

void MockAttributionManager::NotifyOsRegistration(
    const OsRegistration& registration,
    bool is_debug_key_allowed,
    attribution_reporting::mojom::OsRegistrationResult result) {
  base::Time now = base::Time::Now();
  for (const attribution_reporting::OsRegistrationItem& item :
       registration.registration_items) {
    for (auto& observer : observers_) {
      observer.OnOsRegistration(now, item, registration.top_level_origin,
                                registration.GetType(), is_debug_key_allowed,
                                result);
    }
  }
}

void MockAttributionManager::NotifyDebugModeChanged(bool debug_mode) {
  for (auto& observer : observers_) {
    observer.OnDebugModeChanged(debug_mode);
  }
}

void MockAttributionManager::SetDataHostManager(
    std::unique_ptr<AttributionDataHostManager> manager) {
  DCHECK(manager);
  data_host_manager_ = std::move(manager);
}

void MockAttributionManager::SetOnObserverRegistered(base::OnceClosure done) {
  CHECK(!on_observer_registered_);

  if (!observers_.empty()) {
    std::move(done).Run();
    return;
  }
  on_observer_registered_ = std::move(done);
}

}  // namespace content
