// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_COMMON_REMOTING_CLIENT_H_
#define REMOTING_CLIENT_COMMON_REMOTING_CLIENT_H_

#include <string_view>

namespace remoting {

// A simple, native chromoting client implementation.
class RemotingClient {
 public:
  RemotingClient();

  RemotingClient(const RemotingClient&) = delete;
  RemotingClient& operator=(const RemotingClient&) = delete;

  ~RemotingClient();

  void StartSession(std::string_view support_id, std::string_view access_token);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_COMMON_REMOTING_CLIENT_H_
