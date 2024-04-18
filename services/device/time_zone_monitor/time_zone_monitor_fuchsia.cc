// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include <memory>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/intl_profile_watcher.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace device {
namespace {

class TimeZoneMonitorFuchsia : public TimeZoneMonitor {
 public:
  TimeZoneMonitorFuchsia()
      : watcher_(base::BindRepeating(&TimeZoneMonitorFuchsia::OnProfileChanged,
                                     base::Unretained(this))) {}

  TimeZoneMonitorFuchsia(const TimeZoneMonitorFuchsia&) = delete;
  TimeZoneMonitorFuchsia& operator=(const TimeZoneMonitorFuchsia&) = delete;
  ~TimeZoneMonitorFuchsia() override = default;

 private:
  void OnProfileChanged(const ::fuchsia::intl::Profile& profile) {
    std::string new_zone_id = watcher_.GetPrimaryTimeZoneIdFromProfile(profile);
    std::unique_ptr<icu::TimeZone> new_zone(
        base::WrapUnique(icu::TimeZone::createTimeZone(
            icu::UnicodeString::fromUTF8(new_zone_id))));

    // Changes to profile properties other than the time zone may have caused
    // the notification, but we only want to update the ICU default zone and
    // notify renderers if the time zone changed. The timezone must have
    // previously been populated. See InitializeICU().
    std::unique_ptr<icu::TimeZone> current_zone(icu::TimeZone::createDefault());
    if (*current_zone == *new_zone) {
      DVLOG(1) << "timezone already updated";
      return;
    }

    UpdateIcuAndNotifyClients(std::move(new_zone));
  }

  base::FuchsiaIntlProfileWatcher watcher_;
};

}  // namespace

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return std::make_unique<TimeZoneMonitorFuchsia>();
}

}  // namespace device
