// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_ENCRYPTED_MEDIA_REQUEST_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_ENCRYPTED_MEDIA_REQUEST_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_content_decryption_module_access.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_string.h"

#include <memory>

namespace blink {

class EncryptedMediaRequest;
struct WebMediaKeySystemConfiguration;
class WebSecurityOrigin;
template <typename T>
class WebVector;

class WebEncryptedMediaRequest {
 public:
  BLINK_PLATFORM_EXPORT WebEncryptedMediaRequest(
      const WebEncryptedMediaRequest&);
  BLINK_PLATFORM_EXPORT ~WebEncryptedMediaRequest();

  BLINK_PLATFORM_EXPORT WebString KeySystem() const;
  BLINK_PLATFORM_EXPORT const WebVector<WebMediaKeySystemConfiguration>&
  SupportedConfigurations() const;

  BLINK_PLATFORM_EXPORT WebSecurityOrigin GetSecurityOrigin() const;

  BLINK_PLATFORM_EXPORT void RequestSucceeded(
      std::unique_ptr<WebContentDecryptionModuleAccess>);
  BLINK_PLATFORM_EXPORT void RequestNotSupported(
      const WebString& error_message);

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT explicit WebEncryptedMediaRequest(
      EncryptedMediaRequest*);
#endif

 private:
  void Assign(const WebEncryptedMediaRequest&);
  void Reset();

  WebPrivatePtr<EncryptedMediaRequest> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_ENCRYPTED_MEDIA_REQUEST_H_
