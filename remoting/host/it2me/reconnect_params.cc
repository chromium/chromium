// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/reconnect_params.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/uuid.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/signaling/signaling_id_util.h"

namespace remoting {

ReconnectParams::ReconnectParams() = default;
ReconnectParams::ReconnectParams(ReconnectParams&& other) = default;
ReconnectParams& ReconnectParams::operator=(ReconnectParams&& other) = default;
ReconnectParams::~ReconnectParams() = default;

base::Value::Dict ReconnectParams::ToDict(const ReconnectParams& params) {
  DCHECK(params.IsValid());
  return base::Value::Dict()
      .Set(kReconnectSupportId, params.support_id)
      .Set(kReconnectHostSecret, params.host_secret)
      .Set(kReconnectPrivateKey, params.private_key)
      .Set(kReconnectFtlDeviceId, params.ftl_device_id)
      .Set(kReconnectClientFtlAddress, params.client_ftl_address);
}

ReconnectParams ReconnectParams::FromDict(const base::Value::Dict& dict) {
  ReconnectParams params;
  const std::string* support_id = dict.FindString(kReconnectSupportId);
  if (support_id) {
    params.support_id = *support_id;
  }
  const std::string* host_secret = dict.FindString(kReconnectHostSecret);
  if (host_secret) {
    params.host_secret = *host_secret;
  }
  const std::string* private_key = dict.FindString(kReconnectPrivateKey);
  if (private_key) {
    params.private_key = *private_key;
  }
  const std::string* ftl_device_id = dict.FindString(kReconnectFtlDeviceId);
  if (ftl_device_id) {
    params.ftl_device_id = *ftl_device_id;
  }
  const std::string* client_ftl_address =
      dict.FindString(kReconnectClientFtlAddress);
  if (client_ftl_address) {
    params.client_ftl_address = *client_ftl_address;
  }

  DCHECK(params.IsValid());
  return params;
}

bool ReconnectParams::IsValid() const {
  if (support_id.empty()) {
    LOG(ERROR) << "Missing field: support_id";
    return false;
  } else if (support_id.length() != 7) {
    LOG(ERROR) << "Invalid support_id: " << support_id;
    return false;
  }

  if (host_secret.empty()) {
    LOG(ERROR) << "Missing field: host_secret";
    return false;
  } else if (host_secret.length() != 5) {
    LOG(ERROR) << "Invalid host_secret: " << host_secret;
    return false;
  }

  if (private_key.empty()) {
    LOG(ERROR) << "Missing field: private_key";
    return false;
  }

  if (ftl_device_id.empty()) {
    LOG(ERROR) << "Missing field: ftl_device_id";
    return false;
  } else if (!base::Uuid::ParseLowercase(ftl_device_id).is_valid()) {
    LOG(ERROR) << "Invalid ftl_device_id: " << ftl_device_id;
    return false;
  }

  if (client_ftl_address.empty()) {
    LOG(ERROR) << "Missing field: client_ftl_address";
    return false;
  } else if (!IsValidFtlSignalingId(client_ftl_address)) {
    LOG(ERROR) << "Invalid client_ftl_address: " << client_ftl_address;
    return false;
  }

  return true;
}

}  // namespace remoting
