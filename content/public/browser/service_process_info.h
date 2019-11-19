// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_INFO_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_INFO_H_

#include <stdint.h>

#include <string>

#include "base/process/process_handle.h"
#include "base/util/type_safety/id_type.h"
#include "content/common/content_export.h"

namespace content {

namespace internal {
struct CONTENT_EXPORT ServiceProcessIdTypeMarker {};
}  // namespace internal

// An opaque ID type used to uniquely identify service process instances. This
// is separate from system PID. Values are never reused.
using ServiceProcessId =
    util::IdType<internal::ServiceProcessIdTypeMarker, uint64_t, 0u>;

// Information about a running (or very recently running) service process.
struct CONTENT_EXPORT ServiceProcessInfo {
  ServiceProcessInfo();
  ServiceProcessInfo(const ServiceProcessInfo&);
  ~ServiceProcessInfo();

  // Template helper for testing whether this ServiceProcessInfo corresponds to
  // a service process launched to run the |Interface|.
  template <typename Interface>
  bool IsService() const {
    return service_interface_name == Interface::Name_;
  }

  // A unique identifier for this service process instance. ServiceProcessIds
  // are never reused.
  ServiceProcessId service_process_id;

  // The system-dependent process ID of the service process.
  base::ProcessId pid;

  // The name of the service interface for which the process was launched.
  std::string service_interface_name;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_INFO_H_
