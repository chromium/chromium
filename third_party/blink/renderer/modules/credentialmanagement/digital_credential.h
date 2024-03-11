// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_DIGITAL_CREDENTIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_DIGITAL_CREDENTIAL_H_

#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class MODULES_EXPORT DigitalCredential final : public Credential {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static DigitalCredential* Create(const String& protocol, const String& data);

  explicit DigitalCredential(const String& protocol, const String& data);

  // Credential:
  bool IsDigitalCredential() const override;

  // DigitalCredential.idl
  const String& protocol() const { return protocol_; }
  const String& data() const { return data_; }

 private:
  const String protocol_;
  const String data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_DIGITAL_CREDENTIAL_H_
