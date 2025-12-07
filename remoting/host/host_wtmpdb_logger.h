// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_WTMPDB_LOGGER_H_
#define REMOTING_HOST_HOST_WTMPDB_LOGGER_H_

#include <cstdint>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/host/host_status_observer.h"
#include "sql/database.h"

namespace remoting {

class HostStatusMonitor;

// HostWtmpdbLogger records client connect/disconnect events to wtmpdb.
class HostWtmpdbLogger : public HostStatusObserver {
 public:
  explicit HostWtmpdbLogger(scoped_refptr<HostStatusMonitor> monitor);

  HostWtmpdbLogger(const HostWtmpdbLogger&) = delete;
  HostWtmpdbLogger& operator=(const HostWtmpdbLogger&) = delete;

  ~HostWtmpdbLogger() override;

  // HostStatusObserver interface.
  void OnClientConnected(const std::string& signaling_id) override;
  void OnClientDisconnected(const std::string& signaling_id) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // A struct containing pty and wtmpdb ids to identify sessions.
  struct ConnectionInfo {
    std::int32_t pty_id;
    std::int64_t wtmpdb_id;
  };

  // A map from client signaling ID to connection info.
  base::flat_map<std::string, ConnectionInfo> session_;

  scoped_refptr<HostStatusMonitor> monitor_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_WTMPDB_LOGGER_H_
