// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CONTENT_DECRYPTION_MODULE_RESULT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CONTENT_DECRYPTION_MODULE_RESULT_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_content_decryption_module_exception.h"
#include "third_party/blink/public/platform/web_encrypted_media_key_information.h"
#include "third_party/blink/public/platform/web_private_ptr.h"

namespace blink {

class ContentDecryptionModuleResult;
class WebContentDecryptionModule;
class WebString;

class WebContentDecryptionModuleResult {
 public:
  enum SessionStatus {
    // New session has been initialized.
    kNewSession,

    // CDM could not find the requested session.
    kSessionNotFound,

    // CDM already has a non-closed session that matches the provided
    // parameters.
    kSessionAlreadyExists,
  };

  WebContentDecryptionModuleResult(const WebContentDecryptionModuleResult& o) {
    Assign(o);
  }

  ~WebContentDecryptionModuleResult() { Reset(); }

  WebContentDecryptionModuleResult& operator=(
      const WebContentDecryptionModuleResult& o) {
    Assign(o);
    return *this;
  }

  // Called when the CDM completes an operation and has no additional data to
  // pass back.
  BLINK_PLATFORM_EXPORT void Complete();

  // Called when a CDM is created.
  BLINK_PLATFORM_EXPORT void CompleteWithContentDecryptionModule(
      WebContentDecryptionModule*);

  // Called when the CDM completes a session operation.
  BLINK_PLATFORM_EXPORT void CompleteWithSession(SessionStatus);

  // Called when the CDM completes getting key status for policy.
  BLINK_PLATFORM_EXPORT void CompleteWithKeyStatus(
      WebEncryptedMediaKeyInformation::KeyStatus);

  // Called when the operation fails.
  BLINK_PLATFORM_EXPORT void CompleteWithError(
      WebContentDecryptionModuleException,
      uint32_t system_code,
      const WebString& message);

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT explicit WebContentDecryptionModuleResult(
      ContentDecryptionModuleResult*);
#endif

 private:
  BLINK_PLATFORM_EXPORT void Reset();
  BLINK_PLATFORM_EXPORT void Assign(const WebContentDecryptionModuleResult&);

  WebPrivatePtr<ContentDecryptionModuleResult,
                kWebPrivatePtrDestructionCrossThread>
      impl_;
};

}  // namespace blink

#endif  // WebContentDecryptionModuleSession_h
