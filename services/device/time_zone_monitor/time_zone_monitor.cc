// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace device {

TimeZoneMonitor::TimeZoneMonitor()
    : timezone_(icu::TimeZone::createDefault()) {}

TimeZoneMonitor::~TimeZoneMonitor() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TimeZoneMonitor::Bind(
    mojo::PendingReceiver<device::mojom::TimeZoneMonitor> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  receivers_.Add(this, std::move(receiver));
}

void TimeZoneMonitor::NotifyClients(std::string_view zone_id_str) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("device", "TimeZoneMonitor::NotifyClients");
  VLOG(1) << "timezone reset to " << zone_id_str;

  for (auto& client : clients_)
    client->OnTimeZoneChange(std::string(zone_id_str));
}

void TimeZoneMonitor::UpdateIcuAndNotifyClients(
    std::unique_ptr<icu::TimeZone> new_zone) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("device", "TimeZoneMonitor::UpdateIcuAndNotifyClients");

  // Do not notify clients if the timezone didn't change.
  if (*timezone_ == *new_zone) {
    return;
  }
  // Keep track of the last timezone sent to clients.
  timezone_ = base::WrapUnique(new_zone->clone());

  std::string zone_id_str = GetTimeZoneId(*new_zone);

  icu::TimeZone::adoptDefault(new_zone.release());

  NotifyClients(zone_id_str);
}

// static
std::unique_ptr<icu::TimeZone> TimeZoneMonitor::DetectHostTimeZoneFromIcu() {
  return base::WrapUnique(icu::TimeZone::detectHostTimeZone());
}

// static
std::string TimeZoneMonitor::GetTimeZoneId(const icu::TimeZone& zone) {
  icu::UnicodeString zone_id;
  std::string zone_id_str;
  zone.getID(zone_id).toUTF8String(zone_id_str);
  return zone_id_str;
}

void TimeZoneMonitor::AddClient(
    mojo::PendingRemote<device::mojom::TimeZoneMonitorClient> client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto id = clients_.Add(std::move(client));
  clients_.Get(id)->OnTimeZoneChange(GetTimeZoneId(*timezone_));
}

}  // namespace device
