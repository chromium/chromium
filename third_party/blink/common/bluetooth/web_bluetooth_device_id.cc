// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"

#include <ostream>
#include <utility>

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "crypto/random.h"

namespace blink {

WebBluetoothDeviceId::WebBluetoothDeviceId() {}

WebBluetoothDeviceId::WebBluetoothDeviceId(
    const std::string& encoded_device_id) {
  std::string decoded;

  CHECK(base::Base64Decode(encoded_device_id, &decoded));
  CHECK(decoded.size() == sizeof(WebBluetoothDeviceIdKey));
  std::copy_n(decoded.begin(), device_id_.size(), device_id_.begin());
  is_initialized_ = true;
}

WebBluetoothDeviceId::~WebBluetoothDeviceId() {}

WebBluetoothDeviceId::WebBluetoothDeviceId(
    const WebBluetoothDeviceIdKey& device_id)
    : device_id_(device_id), is_initialized_(true) {}

std::string WebBluetoothDeviceId::DeviceIdInBase64() const {
  CHECK(IsValid());
  return base::Base64Encode(device_id_);
}

std::string WebBluetoothDeviceId::str() const {
  return WebBluetoothDeviceId::DeviceIdInBase64();
}

const WebBluetoothDeviceIdKey& WebBluetoothDeviceId::DeviceId() const {
  CHECK(IsValid());
  return device_id_;
}

// static
WebBluetoothDeviceId WebBluetoothDeviceId::Create() {
  WebBluetoothDeviceIdKey bytes;

  crypto::RandBytes(bytes);

  return WebBluetoothDeviceId(std::move(bytes));
}

// static
bool WebBluetoothDeviceId::IsValid(const std::string& encoded_device_id) {
  std::string decoded;
  if (!base::Base64Decode(encoded_device_id, &decoded)) {
    return false;
  }

  if (decoded.size() != sizeof(WebBluetoothDeviceIdKey)) {
    return false;
  }

  return true;
}

bool WebBluetoothDeviceId::IsValid() const {
  return is_initialized_;
}

bool WebBluetoothDeviceId::operator==(
    const WebBluetoothDeviceId& device_id) const {
  return this->DeviceId() == device_id.DeviceId();
}

bool WebBluetoothDeviceId::operator!=(
    const WebBluetoothDeviceId& device_id) const {
  return !(*this == device_id);
}

bool WebBluetoothDeviceId::operator<(
    const WebBluetoothDeviceId& device_id) const {
  return this->str() < device_id.str();
}

std::ostream& operator<<(std::ostream& out,
                         const WebBluetoothDeviceId& device_id) {
  return out << device_id.str();
}

}  // namespace blink
