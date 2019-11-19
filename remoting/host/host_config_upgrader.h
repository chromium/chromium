// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_CONFIG_UPGRADER_H_
#define REMOTING_HOST_HOST_CONFIG_UPGRADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/values.h"
#include "net/base/backoff_entry.h"
#include "remoting/base/offline_token_exchanger.h"

namespace remoting {

// Standalone tool to upgrade the host config file with a new OAuth refresh
// token if needed. This creates its own RunLoop, without needing to create
// any ChromotingHostContext.
class HostConfigUpgrader {
 public:
  // Upgrades the config file with a new refresh token if needed. This uses
  // an internal RunLoop, and blocks until the upgrade succeeds (a new config
  // is written or no action is taken), or it fails after some number of
  // retry attempts. Returns 0 on success, non-zero on error (to be used as
  // the process exit code).
  static int UpgradeConfigFile();

 private:
  HostConfigUpgrader();
  ~HostConfigUpgrader();

  int DoUpgrade();
  void RequestExchangeToken();

  // Callback passed to the OfflineTokenExchanger.
  void OnTokenExchanged(OfflineTokenExchanger::Status status,
                        const std::string& refresh_token);

  // Waits for an increasing backoff time before requesting a new token.
  // On reaching a maximum retry count, it quits the RunLoop.
  void MaybeWaitAndRetry();

  void WriteConfig();

  base::SingleThreadTaskExecutor io_task_executor_{base::MessagePumpType::IO};
  base::RunLoop run_loop_;
  int exit_code_ = 0;
  base::FilePath config_path_;
  std::unique_ptr<base::DictionaryValue> config_;
  std::unique_ptr<OfflineTokenExchanger> token_exchanger_;

  // Holds the old refresh token, so the exchange can be retried if needed.
  std::string refresh_token_;

  net::BackoffEntry backoff_entry_;

  DISALLOW_COPY_AND_ASSIGN(HostConfigUpgrader);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_CONFIG_UPGRADER_H_
