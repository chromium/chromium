// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/binding_security_for_platform.h"

namespace blink {

BindingSecurityForPlatform::ShouldAllowAccessToV8ContextFunction
    BindingSecurityForPlatform::should_allow_access_to_v8context_ = nullptr;

// static
bool BindingSecurityForPlatform::ShouldAllowAccessToV8Context(
    v8::Local<v8::Context> accessing_context,
    v8::MaybeLocal<v8::Context> target_context) {
  return (*should_allow_access_to_v8context_)(accessing_context,
                                              target_context);
}

// static
void BindingSecurityForPlatform::SetShouldAllowAccessToV8Context(
    ShouldAllowAccessToV8ContextFunction func) {
  DCHECK(!should_allow_access_to_v8context_);
  DCHECK(func);
  should_allow_access_to_v8context_ = func;
}

}  // namespace blink
