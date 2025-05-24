// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TEST_NET_LOG_MANAGER_H_
#define NET_TEST_TEST_NET_LOG_MANAGER_H_

#include <memory>
#include <string_view>

#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"

namespace net {

class FileNetLogObserver;

// Manages NetLogObserver for unit tests. When `--log-net-log` is specified
// without a file path, it dumps NetLog events to LOG. When `--log-net-log`
// is specified with a file path, it dumps NetLog events to the file using
// FileNetLogObserver.
class TestNetLogManager {
 public:
  // TODO(crbug.com/336167322): Move network::switches::kLogNetLog so that we
  // can use the switch here.
  static constexpr const std::string_view kLogNetLogSwitch = "log-net-log";

  explicit TestNetLogManager(
      NetLog* net_log = NetLog::Get(),
      NetLogCaptureMode capture_mode = NetLogCaptureMode::kEverything);

  TestNetLogManager(const TestNetLogManager&) = delete;
  TestNetLogManager& operator=(const TestNetLogManager&) = delete;

  ~TestNetLogManager();

  // Force starts logging if not already started.
  void ForceStart();

 private:
  class LogNetLogObserver;

  void Start(NetLog* net_log, NetLogCaptureMode capture_mode);

  std::unique_ptr<FileNetLogObserver> file_net_log_observer_;
  std::unique_ptr<LogNetLogObserver> log_net_log_observer_;
};

}  // namespace net

#endif  // NET_TEST_TEST_NET_LOG_MANAGER_H_
