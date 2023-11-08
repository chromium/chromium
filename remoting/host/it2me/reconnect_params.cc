// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/reconnect_params.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/uuid.h"
#include "remoting/host/it2me/it2me_constants.h"

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
      .Set(kReconnectFtlDeviceRegistrationId,
           params.ftl_device_registration_id);
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
  const std::string* ftl_device_registration_id =
      dict.FindString(kReconnectFtlDeviceRegistrationId);
  if (ftl_device_registration_id) {
    params.ftl_device_registration_id = *ftl_device_registration_id;
  }

  DCHECK(params.IsValid());
  return params;
}

bool ReconnectParams::IsValid() const {
  if (support_id.empty()) {
    LOG(WARNING) << "Missing field: support_id";
    return false;
  } else if (support_id.length() != 7) {
    LOG(WARNING) << "Invalid support_id: " << support_id;
    return false;
  }

  if (host_secret.empty()) {
    LOG(WARNING) << "Missing field: host_secret";
    return false;
  } else if (host_secret.length() != 5) {
    LOG(WARNING) << "Invalid host_secret: " << host_secret;
    return false;
  }

  if (private_key.empty()) {
    LOG(WARNING) << "Missing field: private_key";
    return false;
  }

  if (ftl_device_registration_id.empty()) {
    LOG(WARNING) << "Missing field: ftl_device_registration_id";
    return false;
  } else if (!base::Uuid::ParseLowercase(ftl_device_registration_id)
                  .is_valid()) {
    LOG(WARNING) << "Invalid ftl_device_registration_id: "
                 << ftl_device_registration_id;
    return false;
  }

  return true;
}

}  // namespace remoting
