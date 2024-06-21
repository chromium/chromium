// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_DEVICE_BOUND_SESSION_CREATE_PARAMS_H_
#define NET_DEVICE_BOUND_SESSIONS_DEVICE_BOUND_SESSION_CREATE_PARAMS_H_

#include <string>
#include <vector>

#include "net/base/net_export.h"

namespace net {

// Struct to contain the parameters from the session instruction JSON.
// https://github.com/WICG/dbsc/blob/main/README.md#session-registration-instructions-json
struct NET_EXPORT DeviceBoundSessionCreateParams final {
  // Scope section of session instructions.
  struct Scope {
    // Specification section of the session scope instructions.
    struct Specification {
      enum class Type { kExclude, kInclude };
      bool operator==(const Specification&) const = default;
      Type type;
      std::string domain;
      std::string path;
    };

    // Defaults to false if not in the params
    bool include_site = false;
    std::vector<Specification> specifications;

    Scope();
    Scope(Scope&& other);
    Scope& operator=(Scope&& other);
    ~Scope();
  };

  // Credential section of the session instruction.
  struct Credential {
    bool operator==(const Credential&) const = default;
    std::string name;
    std::string attributes;
  };

  DeviceBoundSessionCreateParams(std::string id,
                                 std::string refresh_url,
                                 Scope scope,
                                 std::vector<Credential> creds);
  DeviceBoundSessionCreateParams(DeviceBoundSessionCreateParams&& other);
  DeviceBoundSessionCreateParams& operator=(
      DeviceBoundSessionCreateParams&& other);

  ~DeviceBoundSessionCreateParams();

  std::string session_id;
  std::string refresh_url;
  Scope scope;
  std::vector<Credential> credentials;
};

}  // namespace net

#endif  // NET_DEVICE_BOUND_SESSIONS_DEVICE_BOUND_SESSION_CREATE_PARAMS_H_
