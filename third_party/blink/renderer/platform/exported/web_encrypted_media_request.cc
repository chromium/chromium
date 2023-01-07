// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_encrypted_media_request.h"

#include "third_party/blink/public/platform/web_media_key_system_configuration.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/platform/encrypted_media_request.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

WebEncryptedMediaRequest::WebEncryptedMediaRequest(
    const WebEncryptedMediaRequest& request) {
  Assign(request);
}

WebEncryptedMediaRequest::WebEncryptedMediaRequest(
    EncryptedMediaRequest* request)
    : private_(request) {}

WebEncryptedMediaRequest::~WebEncryptedMediaRequest() {
  Reset();
}

WebString WebEncryptedMediaRequest::KeySystem() const {
  return private_->KeySystem();
}

const WebVector<WebMediaKeySystemConfiguration>&
WebEncryptedMediaRequest::SupportedConfigurations() const {
  return private_->SupportedConfigurations();
}

WebSecurityOrigin WebEncryptedMediaRequest::GetSecurityOrigin() const {
  return WebSecurityOrigin(private_->GetSecurityOrigin());
}

void WebEncryptedMediaRequest::RequestSucceeded(
    std::unique_ptr<WebContentDecryptionModuleAccess> access) {
  private_->RequestSucceeded(std::move(access));
}

void WebEncryptedMediaRequest::RequestNotSupported(
    const WebString& error_message) {
  private_->RequestNotSupported(error_message);
}

void WebEncryptedMediaRequest::Assign(const WebEncryptedMediaRequest& other) {
  private_ = other.private_;
}

void WebEncryptedMediaRequest::Reset() {
  private_.Reset();
}

}  // namespace blink
