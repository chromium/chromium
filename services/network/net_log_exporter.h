// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NET_LOG_EXPORTER_H_
#define SERVICES_NETWORK_NET_LOG_EXPORTER_H_

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "net/log/net_log.h"
#include "services/network/public/mojom/net_log.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace net {
class FileNetLogObserver;
}  // namespace net

namespace network {

class NetworkContext;

// API implementation for exporting ongoing netlogs.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetLogExporter
    : public mojom::NetLogExporter,
      public base::SupportsWeakPtr<NetLogExporter> {
 public:
  // This expects to live on the same thread as NetworkContext, e.g.
  // IO thread or NetworkService main thread.
  explicit NetLogExporter(NetworkContext* network_context);
  ~NetLogExporter() override;

  void Start(base::File destination,
             base::Value extra_constants,
             net::NetLogCaptureMode capture_mode,
             uint64_t max_file_size,
             StartCallback callback) override;
  void Stop(base::Value polled_data, StopCallback callback) override;

  // Sets a callback that will be used to create a scratch directory instead
  // of the normal codepath. For test use only.
  void SetCreateScratchDirHandlerForTesting(
      const base::RepeatingCallback<base::FilePath()>& handler);

 private:
  void CloseFileOffThread(base::File file);

  // Run off-thread by task scheduler, as does disk I/O.
  static base::FilePath CreateScratchDir(
      base::RepeatingCallback<base::FilePath()>
          scratch_dir_create_handler_for_tests);

  static void StartWithScratchDirOrCleanup(
      base::WeakPtr<NetLogExporter> object,
      base::Value extra_constants,
      net::NetLogCaptureMode capture_mode,
      uint64_t max_file_size,
      StartCallback callback,
      const base::FilePath& scratch_dir_path);

  void StartWithScratchDir(base::Value extra_constants,
                           net::NetLogCaptureMode capture_mode,
                           uint64_t max_file_size,
                           StartCallback callback,
                           const base::FilePath& scratch_dir_path);

  // NetworkContext owns |this| via UniqueReceiverSet, so this object can't
  // outlive it.
  NetworkContext* network_context_;
  enum State { STATE_IDLE, STATE_WAITING_DIR, STATE_RUNNING } state_;

  std::unique_ptr<net::FileNetLogObserver> file_net_observer_;
  base::File destination_;

  // Test-only injectable replacement for CreateScratchDir.
  base::RepeatingCallback<base::FilePath()>
      scratch_dir_create_handler_for_tests_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(NetLogExporter);
};

}  // namespace network

#endif  // SERVICES_NETWORK_NET_LOG_EXPORTER_H_
