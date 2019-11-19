// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_ENCRYPTED_MEDIA_TYPES_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_ENCRYPTED_MEDIA_TYPES_H_

namespace blink {

// From https://w3c.github.io/encrypted-media/#idl-def-MediaKeySessionType
enum class WebEncryptedMediaSessionType {
  kUnknown,
  kTemporary,
  kPersistentLicense,
  kPersistentUsageRecord,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_ENCRYPTED_MEDIA_TYPES_H_
