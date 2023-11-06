// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_RECONNECT_PARAMS_H_
#define REMOTING_HOST_IT2ME_RECONNECT_PARAMS_H_

#include <string>

#include "base/values.h"

namespace remoting {

struct ReconnectParams {
  ReconnectParams();
  ReconnectParams(ReconnectParams&& other);
  ReconnectParams& operator=(ReconnectParams&& other);
  ~ReconnectParams();

  // Helpers used to convert to/from a JSON dictionary.
  static base::Value::Dict ToDict(const ReconnectParams& params);
  static ReconnectParams FromDict(const base::Value::Dict& dict);

  // Verifies the structure contains valid data.
  bool IsValid() const;

  // The 7 digit host identifier used for Directory registration and lookups.
  std::string support_id;

  // The 5 digit host 'secret' used to establish a secure P2P connection.
  std::string host_secret;

  // A Base64 encoded string representing the host's private key.
  std::string private_key;

  // A UUID representing an endpoint in the FTL signaling service.
  std::string ftl_device_registration_id;
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_RECONNECT_PARAMS_H_
