// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/binding_security_for_platform.h"

namespace blink {

BindingSecurityForPlatform::ShouldAllowAccessToV8ContextFunction
    BindingSecurityForPlatform::should_allow_access_to_v8context_ = nullptr;
BindingSecurityForPlatform::ShouldAllowWrapperCreationOrThrowExceptionFunction
    BindingSecurityForPlatform::
        should_allow_wrapper_creation_or_throw_exception_ = nullptr;
BindingSecurityForPlatform::RethrowWrapperCreationExceptionFunction
    BindingSecurityForPlatform::rethrow_wrapper_creation_exception_ = nullptr;

// static
bool BindingSecurityForPlatform::ShouldAllowAccessToV8Context(
    v8::Local<v8::Context> accessing_context,
    v8::MaybeLocal<v8::Context> target_context) {
  return (*should_allow_access_to_v8context_)(accessing_context,
                                              target_context);
}

// static
bool BindingSecurityForPlatform::ShouldAllowWrapperCreationOrThrowException(
    v8::Local<v8::Context> accessing_context,
    v8::MaybeLocal<v8::Context> creation_context,
    const WrapperTypeInfo* wrapper_type_info) {
  return (*should_allow_wrapper_creation_or_throw_exception_)(
      accessing_context, creation_context, wrapper_type_info);
}

// static
void BindingSecurityForPlatform::RethrowWrapperCreationException(
    v8::Local<v8::Context> accessing_context,
    v8::MaybeLocal<v8::Context> creation_context,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Value> cross_context_exception) {
  (*rethrow_wrapper_creation_exception_)(accessing_context, creation_context,
                                         wrapper_type_info,
                                         cross_context_exception);
}

// static
void BindingSecurityForPlatform::SetShouldAllowAccessToV8Context(
    ShouldAllowAccessToV8ContextFunction func) {
  DCHECK(!should_allow_access_to_v8context_);
  DCHECK(func);
  should_allow_access_to_v8context_ = func;
}

// static
void BindingSecurityForPlatform::SetShouldAllowWrapperCreationOrThrowException(
    ShouldAllowWrapperCreationOrThrowExceptionFunction func) {
  DCHECK(!should_allow_wrapper_creation_or_throw_exception_);
  DCHECK(func);
  should_allow_wrapper_creation_or_throw_exception_ = func;
}

// static
void BindingSecurityForPlatform::SetRethrowWrapperCreationException(
    RethrowWrapperCreationExceptionFunction func) {
  DCHECK(!rethrow_wrapper_creation_exception_);
  DCHECK(func);
  rethrow_wrapper_creation_exception_ = func;
}

}  // namespace blink
