// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_

#include <set>

#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/system_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"

namespace content {
namespace protocol {

class SystemInfoHandler : public DevToolsDomainHandler,
                          public SystemInfo::Backend {
 public:
  explicit SystemInfoHandler(bool is_browser_session);

  SystemInfoHandler(const SystemInfoHandler&) = delete;
  SystemInfoHandler& operator=(const SystemInfoHandler&) = delete;

  ~SystemInfoHandler() override;

  // DevToolsDomainHandler implementation.
  void Wire(UberDispatcher* dispatcher) override;

  // Protocol methods.

  // Only available in browser targets.
  void GetInfo(std::unique_ptr<GetInfoCallback> callback) override;
  // Only available in browser targets.
  void GetProcessInfo(
      std::unique_ptr<GetProcessInfoCallback> callback) override;
  Response GetFeatureState(const String& in_featureState,
                           bool* featureEnabled) override;

 private:
  const bool is_browser_session_;

  friend class SystemInfoHandlerGpuObserver;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_
