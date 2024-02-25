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

  // A UUID representing an endpoint in the FTL signaling service. This ID is
  // used to generate the registration ID which is used for endpoint targeting.
  std::string ftl_device_id;

  // Used to send a signaling message to notify the client that the host is
  // ready for a reconnection attempt. Required format for this field is:
  // user@domain.com/chromoting_ftl_<device_registration_uuid>
  std::string client_ftl_address;
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_RECONNECT_PARAMS_H_
