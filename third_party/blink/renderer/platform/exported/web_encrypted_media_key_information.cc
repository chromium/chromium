// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_encrypted_media_key_information.h"

namespace blink {

WebEncryptedMediaKeyInformation::WebEncryptedMediaKeyInformation() = default;

WebEncryptedMediaKeyInformation::~WebEncryptedMediaKeyInformation() = default;

WebData WebEncryptedMediaKeyInformation::Id() const {
  return id_;
}

void WebEncryptedMediaKeyInformation::SetId(const WebData& id) {
  id_.Assign(id);
}

WebEncryptedMediaKeyInformation::KeyStatus
WebEncryptedMediaKeyInformation::Status() const {
  return status_;
}

void WebEncryptedMediaKeyInformation::SetStatus(KeyStatus status) {
  status_ = status;
}

uint32_t WebEncryptedMediaKeyInformation::SystemCode() const {
  return system_code_;
}

void WebEncryptedMediaKeyInformation::SetSystemCode(uint32_t system_code) {
  system_code_ = system_code;
}

}  // namespace blink
