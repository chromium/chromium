// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_HOST_INFO_H_
#define REMOTING_TEST_HOST_INFO_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "remoting/test/connection_setup_info.h"

namespace remoting {
namespace test {

enum HostStatus {
  kHostStatusOnline,
  kHostStatusOffline
};

struct HostInfo {
  HostInfo();
  HostInfo(const HostInfo& other);
  ~HostInfo();

  // Returns true if |host_info| is valid and initializes HostInfo.
  bool ParseHostInfo(const base::Value& host_info);

  // Returns true if the chromoting host is ready to accept connections.
  bool IsReadyForConnection() const;

  // Generates connection information to establish a chromoting connection.
  ConnectionSetupInfo GenerateConnectionSetupInfo(
      const std::string& access_token,
      const std::string& user_name,
      const std::string& pin) const;

  std::string host_id;
  std::string host_jid;
  std::string host_name;
  HostStatus status = kHostStatusOffline;
  std::string offline_reason;
  std::string public_key;
  std::vector<std::string> token_url_patterns;
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_HOST_INFO_H_
