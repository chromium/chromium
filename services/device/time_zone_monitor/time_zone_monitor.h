// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_TIME_ZONE_MONITOR_TIME_ZONE_MONITOR_H_
#define SERVICES_DEVICE_TIME_ZONE_MONITOR_TIME_ZONE_MONITOR_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/time_zone_monitor.mojom.h"
#include "third_party/icu/source/common/unicode/uversion.h"

template <class T>
class scoped_refptr;

namespace base {
class SequencedTaskRunner;
}

U_NAMESPACE_BEGIN
class TimeZone;
U_NAMESPACE_END

namespace device {

// TimeZoneMonitor watches the system time zone, and notifies renderers
// when it changes. Some renderer code caches the system time zone, so
// this notification is necessary to inform such code that cached
// timezone data may have become invalid. Due to sandboxing, it is not
// possible for renderer processes to monitor for system time zone
// changes themselves, so this must happen in the browser process.
//
// Sandboxing also may prevent renderer processes from reading the time
// zone when it does change, so platforms may have to deal with this in
// platform-specific ways:
//  - Mac uses a sandbox hole defined in content/renderer/renderer.sb.
//  - Linux-based platforms use ProxyLocaltimeCallToBrowser in
//    content/zygote/zygote_main_linux.cc and HandleLocaltime in
//    content/browser/sandbox_ipc_linux.cc to override
//    localtime in renderer processes with custom code that calls
//    localtime in the browser process via Chrome IPC.

class TimeZoneMonitor : public device::mojom::TimeZoneMonitor {
 public:
  // Returns a new TimeZoneMonitor object (likely a subclass) specific to the
  // platform. Inject |file_task_runner| to enable running blocking file
  // operations on it when necessary.
  static std::unique_ptr<TimeZoneMonitor> Create(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);

  ~TimeZoneMonitor() override;

  void Bind(mojo::PendingReceiver<device::mojom::TimeZoneMonitor> receiver);

 protected:
  TimeZoneMonitor();

  // Notifies clients that the system time zone may have changed and is now
  // zone_id_str.
  void NotifyClients(base::StringPiece zone_id_str);

  // Sets ICU's default TimeZone for the process and calls NotifyClients().
  void UpdateIcuAndNotifyClients(std::unique_ptr<icu::TimeZone> new_zone);

  // Detects the host time zone using the logic in ICU.
  static std::unique_ptr<icu::TimeZone> DetectHostTimeZoneFromIcu();

  // Converts |zone|'s ID string to UTF-8 and returns it.
  static std::string GetTimeZoneId(const icu::TimeZone& zone);

 private:
  base::ThreadChecker thread_checker_;

  // device::mojom::device::mojom::TimeZoneMonitor:
  void AddClient(mojo::PendingRemote<device::mojom::TimeZoneMonitorClient>
                     client) override;

  mojo::ReceiverSet<device::mojom::TimeZoneMonitor> receivers_;
  mojo::RemoteSet<device::mojom::TimeZoneMonitorClient> clients_;
  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitor);
};

}  // namespace device

#endif  // SERVICES_DEVICE_TIME_ZONE_MONITOR_TIME_ZONE_MONITOR_H_
