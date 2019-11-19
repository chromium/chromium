// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_AUTHENTICATOR_ASSERTION_RESPONSE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_AUTHENTICATOR_ASSERTION_RESPONSE_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/credentialmanager/authenticator_response.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class MODULES_EXPORT AuthenticatorAssertionResponse final
    : public AuthenticatorResponse {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AuthenticatorAssertionResponse(DOMArrayBuffer* client_data_json,
                                          DOMArrayBuffer* authenticator_data,
                                          DOMArrayBuffer* signature,
                                          DOMArrayBuffer* user_handle);
  ~AuthenticatorAssertionResponse() override;

  DOMArrayBuffer* authenticatorData() const {
    return authenticator_data_.Get();
  }

  DOMArrayBuffer* signature() const { return signature_.Get(); }

  DOMArrayBuffer* userHandle() const { return user_handle_.Get(); }

  void Trace(blink::Visitor*) override;

 private:
  const Member<DOMArrayBuffer> authenticator_data_;
  const Member<DOMArrayBuffer> signature_;
  const Member<DOMArrayBuffer> user_handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_AUTHENTICATOR_ASSERTION_RESPONSE_H_
