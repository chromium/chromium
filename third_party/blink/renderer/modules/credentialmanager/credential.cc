// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/credential.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
constexpr char kOtpCredentialType[] = "otp";
}

Credential::~Credential() = default;

Credential::Credential(const String& id, const String& type)
    : id_(id), type_(type) {
  DCHECK(!id_.IsEmpty() || type == kOtpCredentialType);
  DCHECK(!type_.IsEmpty());
}

KURL Credential::ParseStringAsURLOrThrow(const String& url,
                                         ExceptionState& exception_state) {
  if (url.IsEmpty())
    return KURL();
  KURL parsed_url = KURL(NullURL(), url);
  if (!parsed_url.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "'" + url + "' is not a valid URL.");
  }
  return parsed_url;
}

void Credential::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
