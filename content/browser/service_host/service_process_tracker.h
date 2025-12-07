// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_HOST_SERVICE_PROCESS_TRACKER_H_
#define CONTENT_BROWSER_SERVICE_HOST_SERVICE_PROCESS_TRACKER_H_

#include <map>
#include <optional>
#include <string>

#include "base/observer_list.h"
#include "base/process/process.h"
#include "content/browser/service_host/utility_process_host.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "url/gurl.h"

namespace content {

// This class keeps track of all the UtilityProcessHosts and coordinates
// the notification system to the ServiceHost observers.
class ServiceProcessTracker {
 public:
  ServiceProcessTracker();

  ServiceProcessTracker(const ServiceProcessTracker&) = delete;
  ServiceProcessTracker& operator=(const ServiceProcessTracker&) = delete;

  ~ServiceProcessTracker();

  ServiceProcessInfo AddProcess(base::Process process,
                                const std::optional<GURL>& site,
                                const std::string& service_interface_name);

  void NotifyTerminated(ServiceProcessId id);

  void NotifyCrashed(ServiceProcessId id,
                     UtilityProcessHost::Client::CrashType type);

  void AddObserver(ServiceProcessHost::Observer* observer);

  void RemoveObserver(ServiceProcessHost::Observer* observer);

  std::vector<ServiceProcessInfo> GetProcesses();

 private:
  ServiceProcessId GenerateNextId();

  ServiceProcessId::Generator service_process_id_generator_;

  std::map<ServiceProcessId, ServiceProcessInfo> processes_;

  // Observers are owned and used exclusively on the UI thread.
  base::ObserverList<ServiceProcessHost::Observer> observers_;
};

ServiceProcessTracker& GetServiceProcessTracker();

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_HOST_SERVICE_PROCESS_TRACKER_H_
