// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/digital_credential.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {
constexpr char kDigitalCredentialType[] = "digital";
}  // anonymous namespace

DigitalCredential* DigitalCredential::Create(const String& protocol,
                                             ScriptObject data) {
  return MakeGarbageCollected<DigitalCredential>(protocol, data);
}

DigitalCredential::DigitalCredential(const String& protocol, ScriptObject data)
    : Credential(/* id = */ "", kDigitalCredentialType),
      protocol_(protocol),
      data_(data) {}

bool DigitalCredential::IsDigitalCredential() const {
  return true;
}

void DigitalCredential::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
  Credential::Trace(visitor);
}

// static
bool DigitalCredential::userAgentAllowsProtocol(const String& protocol) {
  // Since Chromium allows all protocols to reach the underlying platform, this
  // method only validates the protocol identifier, but doesn't do any further
  // checks. In other words, any protocol with a valid identifier is allowed.
  if (protocol.empty()) {
    return false;
  }

  bool has_lower_alpha = false;

  for (unsigned i = 0; i < protocol.length(); ++i) {
    // protocol[i] returns a UChar (16-bit) to handle all cases safely.
    UChar c = protocol[i];

    if (IsASCIILower(c)) {
      has_lower_alpha = true;
    } else if (!IsASCIIDigit(c) && c != uchar::kHyphenMinus) {
      return false;
    }
  }

  // Must contain at least one lowercase letter.
  return has_lower_alpha;
}

ScriptObject DigitalCredential::toJSON(ScriptState* script_state) const {
  V8ObjectBuilder builder(script_state);
  builder.AddString("protocol", protocol_);
  builder.AddV8Value("data", data_.V8Object());
  return builder.ToScriptObject();
}

}  // namespace blink
