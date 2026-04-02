// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_host/utility_process_client.h"

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/process/process.h"
#include "content/browser/service_host/service_process_tracker.h"
#include "content/browser/service_host/utility_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_info.h"
#include "url/gurl.h"

namespace content {
UtilityProcessClient::UtilityProcessClient(
    const std::string& service_interface_name,
    const std::optional<GURL>& site,
    base::OnceCallback<void(const base::Process&)> process_callback,
    base::WeakPtr<ServiceProcessHost::Observer> observer)
    : service_interface_name_(service_interface_name),
      site_(std::move(site)),
      process_callback_(std::move(process_callback)),
      observer_(std::move(observer)) {}

UtilityProcessClient::~UtilityProcessClient() = default;

void UtilityProcessClient::OnProcessLaunched(const base::Process& process) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  auto info = GetServiceProcessTracker().AddProcess(
      process.Duplicate(), site_, service_interface_name_, observer_);
  service_process_id_ = info.service_process_id();
  if (process_callback_) {
    std::move(process_callback_).Run(process);
  }
}

void UtilityProcessClient::OnProcessTerminatedNormally() {
  GetServiceProcessTracker().NotifyTerminated(*service_process_id_);
}

void UtilityProcessClient::OnProcessCrashed(CrashType type) {
  // TODO(crbug.com/40654042): It is unclear how we can observe
  // |OnProcessCrashed()| without observing |OnProcessLaunched()| first, but
  // it can happen on Android. Ignore the notification in this case.
  if (!service_process_id_) {
    return;
  }

  GetServiceProcessTracker().NotifyCrashed(*service_process_id_, type);
}
}  // namespace content
