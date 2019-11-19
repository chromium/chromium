// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CONNECT_TO_HOST_INFO_H_
#define REMOTING_CLIENT_CONNECT_TO_HOST_INFO_H_

#include <string>

#include "remoting/base/chromoting_event.h"

namespace remoting {

struct ConnectToHostInfo {
  ConnectToHostInfo();
  ConnectToHostInfo(const ConnectToHostInfo& other);
  ConnectToHostInfo(ConnectToHostInfo&& other);
  ~ConnectToHostInfo();

  ConnectToHostInfo& operator=(const ConnectToHostInfo& other);

  std::string username;
  std::string auth_token;
  std::string host_jid;
  std::string host_ftl_id;
  std::string host_id;
  std::string host_pubkey;
  std::string pairing_id;
  std::string pairing_secret;
  std::string capabilities;
  std::string flags;
  std::string host_version;
  std::string host_os;
  std::string host_os_version;
  ChromotingEvent::SessionEntryPoint session_entry_point =
      ChromotingEvent::SessionEntryPoint::CONNECT_BUTTON;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CONNECT_TO_HOST_INFO_H_
