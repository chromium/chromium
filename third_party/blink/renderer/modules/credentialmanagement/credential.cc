// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {
constexpr char kDigitalCredentialType[] = "digital";
constexpr char kFederatedCredentialType[] = "federated";
constexpr char kIdentityCredentialType[] = "identity";
constexpr char kOtpCredentialType[] = "otp";
}  // namespace

Credential::~Credential() = default;

Credential::Credential(const String& id, const String& type)
    : id_(id), type_(type) {
  DCHECK(!id_.empty() || type == kDigitalCredentialType ||
         type == kFederatedCredentialType || type == kIdentityCredentialType ||
         type == kOtpCredentialType);
  DCHECK(!type_.empty());
}

KURL Credential::ParseStringAsURLOrThrow(const String& url,
                                         ExceptionState& exception_state) {
  if (url.empty())
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
