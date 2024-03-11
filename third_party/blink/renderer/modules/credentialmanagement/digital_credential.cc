// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/digital_credential.h"

namespace blink {

namespace {
constexpr char kDigitalCredentialType[] = "digital";
}  // anonymous namespace

DigitalCredential* DigitalCredential::Create(const String& protocol,
                                             const String& data) {
  return MakeGarbageCollected<DigitalCredential>(protocol, data);
}

DigitalCredential::DigitalCredential(const String& protocol, const String& data)
    : Credential(/* id = */ "", kDigitalCredentialType),
      protocol_(protocol),
      data_(data) {}

bool DigitalCredential::IsDigitalCredential() const {
  return true;
}

}  // namespace blink
