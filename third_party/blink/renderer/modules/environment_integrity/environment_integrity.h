// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ENVIRONMENT_INTEGRITY_ENVIRONMENT_INTEGRITY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ENVIRONMENT_INTEGRITY_ENVIRONMENT_INTEGRITY_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class EnvironmentIntegrity : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit EnvironmentIntegrity(DOMArrayBuffer* attestation_token);
  ~EnvironmentIntegrity() override;

  EnvironmentIntegrity(const EnvironmentIntegrity&) = delete;
  EnvironmentIntegrity& operator=(const EnvironmentIntegrity&) = delete;

  String encode(ScriptState*);

  DOMArrayBuffer* attestationToken() const { return attestation_token_.Get(); }

  void Trace(Visitor*) const override;

 private:
  const Member<DOMArrayBuffer> attestation_token_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ENVIRONMENT_INTEGRITY_ENVIRONMENT_INTEGRITY_H_
