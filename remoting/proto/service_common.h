// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTO_SERVICE_COMMON_H_
#define REMOTING_PROTO_SERVICE_COMMON_H_

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

}  // namespace remoting::internal

#endif  // REMOTING_PROTO_SERVICE_COMMON_H_
