// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_HOST_UTILITY_PROCESS_CLIENT_H_
#define CONTENT_BROWSER_SERVICE_HOST_UTILITY_PROCESS_CLIENT_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/process/process.h"
#include "content/browser/service_host/utility_process_host.h"
#include "content/public/browser/service_process_info.h"
#include "url/gurl.h"

namespace content {

// Helper to bridge UtilityProcessHost IO thread events to the
// ServiceProcessTracker. Every UtilityProcessHost created for a service process
// has a unique instance of this class associated with it.
class UtilityProcessClient : public UtilityProcessHost::Client {
 public:
  UtilityProcessClient(
      const std::string& service_interface_name,
      const std::optional<GURL>& site,
      base::OnceCallback<void(const base::Process&)> process_callback);

  UtilityProcessClient(const UtilityProcessClient&) = delete;
  UtilityProcessClient& operator=(const UtilityProcessClient&) = delete;

  ~UtilityProcessClient() override;

  // UtilityProcessHost::Client:
  void OnProcessLaunched(const base::Process& process) override;

  void OnProcessTerminatedNormally() override;

  void OnProcessCrashed(CrashType type) override;

 private:
  const std::string service_interface_name_;

  // Optional site GURL for per-site utility processes.
  const std::optional<GURL> site_;

  base::OnceCallback<void(const base::Process&)> process_callback_;
  std::optional<ServiceProcessInfo> process_info_;
};
}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_HOST_UTILITY_PROCESS_CLIENT_H_
