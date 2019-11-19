// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/time_zone_monitor/time_zone_monitor.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace device {

TimeZoneMonitor::TimeZoneMonitor() {
}

TimeZoneMonitor::~TimeZoneMonitor() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TimeZoneMonitor::Bind(
    mojo::PendingReceiver<device::mojom::TimeZoneMonitor> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  receivers_.Add(this, std::move(receiver));
}

void TimeZoneMonitor::NotifyClients(base::StringPiece zone_id_str) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "timezone reset to " << zone_id_str;

  for (auto& client : clients_)
    client->OnTimeZoneChange(zone_id_str.as_string());
}

void TimeZoneMonitor::UpdateIcuAndNotifyClients(
    std::unique_ptr<icu::TimeZone> new_zone) {
  DCHECK(thread_checker_.CalledOnValidThread());

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
  clients_.Add(std::move(client));
}

}  // namespace device
