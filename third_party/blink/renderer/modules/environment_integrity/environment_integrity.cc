// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/environment_integrity/environment_integrity.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

base::span<const uint8_t> DOMArrayBufferToSpan(DOMArrayBuffer* buffer) {
  return base::make_span(static_cast<const uint8_t*>(buffer->Data()),
                         buffer->ByteLength());
}

}  // namespace

EnvironmentIntegrity::EnvironmentIntegrity(DOMArrayBuffer* attestation_token)
    : attestation_token_(attestation_token) {}

EnvironmentIntegrity::~EnvironmentIntegrity() = default;

String EnvironmentIntegrity::encode(ScriptState* script_state) {
  return WTF::Base64Encode(DOMArrayBufferToSpan(attestation_token_.Get()));
}

void EnvironmentIntegrity::Trace(Visitor* visitor) const {
  visitor->Trace(attestation_token_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
