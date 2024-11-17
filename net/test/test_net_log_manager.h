// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TEST_NET_LOG_MANAGER_H_
#define NET_TEST_TEST_NET_LOG_MANAGER_H_

#include <memory>
#include <string_view>

#include "net/log/net_log_capture_mode.h"

namespace net {

class NetLog;
class FileNetLogObserver;

// Manages NetLogObserver for unit tests. When `--log-net-log` is specified
// without a file path, it dumps NetLog events to VLOG. When `--log-net-log`
// is specified with a file path, it dumps NetLog events to the file using
// FileNetLogObserver.
class TestNetLogManager {
 public:
  // TODO(crbug.com/336167322): Move network::switches::kLogNetLog so that we
  // can use the switch here.
  static constexpr const std::string_view kLogNetLogSwitch = "log-net-log";

  TestNetLogManager(NetLog* net_log, NetLogCaptureMode capture_mode);

  TestNetLogManager(const TestNetLogManager&) = delete;
  TestNetLogManager& operator=(const TestNetLogManager&) = delete;

  ~TestNetLogManager();

 private:
  class VlogNetLogObserver;

  std::unique_ptr<FileNetLogObserver> file_net_log_observer_;
  std::unique_ptr<VlogNetLogObserver> vlog_net_log_observer_;
};

}  // namespace net

#endif  // NET_TEST_TEST_NET_LOG_MANAGER_H_
