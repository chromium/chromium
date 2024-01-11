// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_INFO_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_INFO_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/process/process.h"
#include "base/types/id_type.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

namespace internal {
struct CONTENT_EXPORT ServiceProcessIdTypeMarker {};
}  // namespace internal

// An opaque ID type used to uniquely identify service process instances. This
// is separate from system PID. Values are never reused.
using ServiceProcessId =
    base::IdType<internal::ServiceProcessIdTypeMarker, uint64_t, 0u>;

// Information about a running (or very recently running) service process.
//
// This class is move-only but can be copied by calling the Duplicate() method.
// This is explicitly defined to prevent accidental copying, as the Duplicate()
// operation will call Duplicate() on the underlying base::Process.
class CONTENT_EXPORT ServiceProcessInfo {
 public:
  ServiceProcessInfo(const std::string& name,
                     const std::optional<GURL>& site,
                     const ServiceProcessId& id,
                     base::Process process);
  ServiceProcessInfo(const ServiceProcessInfo&) = delete;
  ServiceProcessInfo(ServiceProcessInfo&&);
  ServiceProcessInfo& operator=(const ServiceProcessInfo&) = delete;
  ServiceProcessInfo& operator=(ServiceProcessInfo&&);

  ~ServiceProcessInfo();

  // Template helper for testing whether this ServiceProcessInfo corresponds to
  // a service process launched to run the |Interface|.
  template <typename Interface>
  bool IsService() const {
    return service_interface_name_ == Interface::Name_;
  }

  // Duplicates the ServiceProcessInfo since this struct is non-copyable. This
  // Duplicates the underlying `process_`.
  ServiceProcessInfo Duplicate() const;

  const ServiceProcessId service_process_id() const {
    return service_process_id_;
  }
  const std::string service_interface_name() const {
    return service_interface_name_;
  }
  const std::optional<GURL>& site() const { return site_; }
  const base::Process& GetProcess() const { return process_; }

 private:
  // The name of the service interface for which the process was launched.
  std::string service_interface_name_;

  // Optional site associated with the process for per-site service processes.
  std::optional<GURL> site_;

  // A unique identifier for this service process instance. ServiceProcessIds
  // are never reused.
  ServiceProcessId service_process_id_;

  // The service process.
  base::Process process_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_INFO_H_
