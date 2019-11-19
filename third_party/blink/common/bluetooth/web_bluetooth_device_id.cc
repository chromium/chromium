// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "crypto/random.h"

namespace blink {

namespace {

enum { kDeviceIdLength = 16 /* 128 bits */ };

}  // namespace

WebBluetoothDeviceId::WebBluetoothDeviceId() {}

WebBluetoothDeviceId::WebBluetoothDeviceId(std::string device_id)
    : device_id_(std::move(device_id)) {
  CHECK(IsValid());
}

WebBluetoothDeviceId::~WebBluetoothDeviceId() {}

const std::string& WebBluetoothDeviceId::str() const {
  CHECK(IsValid());
  return device_id_;
}

// static
WebBluetoothDeviceId WebBluetoothDeviceId::Create() {
  std::string bytes(
      kDeviceIdLength + 1 /* to avoid bytes being reallocated by WriteInto */,
      '\0');

  crypto::RandBytes(base::WriteInto(&bytes /* str */,
                                    kDeviceIdLength + 1 /* length_with_null */),
                    kDeviceIdLength);

  base::Base64Encode(bytes, &bytes);

  return WebBluetoothDeviceId(std::move(bytes));
}

// static
bool WebBluetoothDeviceId::IsValid(const std::string& device_id) {
  std::string decoded;
  if (!base::Base64Decode(device_id, &decoded)) {
    return false;
  }

  if (decoded.size() != kDeviceIdLength) {
    return false;
  }

  // When base64-encoding a 128bit string, only the two MSB are used for
  // the 3rd-to-last character. Because of this, the 3rd-to-last character
  // can only be one of this four characters.
  if (!(device_id[device_id.size() - 3] == 'A' ||
        device_id[device_id.size() - 3] == 'Q' ||
        device_id[device_id.size() - 3] == 'g' ||
        device_id[device_id.size() - 3] == 'w')) {
    return false;
  }

  return true;
}

bool WebBluetoothDeviceId::IsValid() const {
  return WebBluetoothDeviceId::IsValid(device_id_);
}

bool WebBluetoothDeviceId::operator==(
    const WebBluetoothDeviceId& device_id) const {
  return str() == device_id.str();
}

bool WebBluetoothDeviceId::operator!=(
    const WebBluetoothDeviceId& device_id) const {
  return !(*this == device_id);
}

std::ostream& operator<<(std::ostream& out,
                         const WebBluetoothDeviceId& device_id) {
  return out << device_id.str();
}

}  // namespace blink
