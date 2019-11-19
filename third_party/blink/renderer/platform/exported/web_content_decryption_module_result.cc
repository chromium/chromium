// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_content_decryption_module_result.h"

#include "third_party/blink/renderer/platform/content_decryption_module_result.h"

namespace blink {

void WebContentDecryptionModuleResult::Complete() {
  impl_->Complete();
  Reset();
}

void WebContentDecryptionModuleResult::CompleteWithContentDecryptionModule(
    WebContentDecryptionModule* cdm) {
  impl_->CompleteWithContentDecryptionModule(cdm);
  Reset();
}

void WebContentDecryptionModuleResult::CompleteWithSession(
    SessionStatus status) {
  impl_->CompleteWithSession(status);
  Reset();
}

void WebContentDecryptionModuleResult::CompleteWithKeyStatus(
    WebEncryptedMediaKeyInformation::KeyStatus key_status) {
  impl_->CompleteWithKeyStatus(key_status);
  Reset();
}

void WebContentDecryptionModuleResult::CompleteWithError(
    WebContentDecryptionModuleException exception,
    uint32_t system_code,
    const WebString& error_message) {
  impl_->CompleteWithError(exception, system_code, error_message);
  Reset();
}

WebContentDecryptionModuleResult::WebContentDecryptionModuleResult(
    ContentDecryptionModuleResult* impl)
    : impl_(impl) {
  DCHECK(impl_.Get());
}

void WebContentDecryptionModuleResult::Reset() {
  impl_.Reset();
}

void WebContentDecryptionModuleResult::Assign(
    const WebContentDecryptionModuleResult& o) {
  impl_ = o.impl_;
}

}  // namespace blink
