// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTO_REMOTE_SUPPORT_SERVICE_H_
#define REMOTING_PROTO_REMOTE_SUPPORT_SERVICE_H_

#include <string>

namespace remoting::internal {

struct TachyonAccountInfoStruct {
  std::string account_id;
  std::string registration_id;
};

struct OperatingSystemInfoStruct {
  std::string name;
  std::string version;
};

struct RemoteSupportHostStruct {
  RemoteSupportHostStruct();
  ~RemoteSupportHostStruct();

  RemoteSupportHostStruct(const RemoteSupportHostStruct&);
  RemoteSupportHostStruct(RemoteSupportHostStruct&&);
  RemoteSupportHostStruct& operator=(const RemoteSupportHostStruct&);
  RemoteSupportHostStruct& operator=(RemoteSupportHostStruct&&);

  std::string public_key;
  std::string version;
  std::string authorized_helper_email;
  TachyonAccountInfoStruct tachyon_account_info;
  OperatingSystemInfoStruct operating_system_info;
};

}  // namespace remoting::internal

#endif  // REMOTING_PROTO_REMOTE_SUPPORT_SERVICE_H_
