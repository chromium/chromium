// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ENCRYPTED_MEDIA_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ENCRYPTED_MEDIA_REQUEST_H_

#include <memory>

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class SecurityOrigin;
class WebContentDecryptionModuleAccess;
struct WebMediaKeySystemConfiguration;
class WebString;
template <typename T>
class WebVector;

class EncryptedMediaRequest : public GarbageCollected<EncryptedMediaRequest> {
 public:
  virtual ~EncryptedMediaRequest() = default;

  virtual WebString KeySystem() const = 0;
  virtual const WebVector<WebMediaKeySystemConfiguration>&
  SupportedConfigurations() const = 0;

  virtual const SecurityOrigin* GetSecurityOrigin() const = 0;

  virtual void RequestSucceeded(
      std::unique_ptr<WebContentDecryptionModuleAccess>) = 0;
  virtual void RequestNotSupported(const WebString& error_message) = 0;

  virtual void Trace(blink::Visitor* visitor) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ENCRYPTED_MEDIA_REQUEST_H_
