// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_BINDING_SECURITY_FOR_PLATFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_BINDING_SECURITY_FOR_PLATFORM_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
struct WrapperTypeInfo;

// BindingSecurityForPlatform provides utility functions that determine access
// permission between two realms.
//
// This class is a collection of trampolines to the actual implementations in
// BindingSecurity in core/ component.
class PLATFORM_EXPORT BindingSecurityForPlatform {
  STATIC_ONLY(BindingSecurityForPlatform);

 public:
  enum class ErrorReportOption {
    kDoNotReport,
    kReport,
  };

  // These overloads should be used only when checking a general access from
  // one context to another context. For access to a receiver object or
  // returned object, you should use BindingSecurity::ShouldAllowAccessTo
  // family.
  static bool ShouldAllowAccessToV8Context(
      v8::Local<v8::Context> accessing_context,
      v8::Local<v8::Context> target_context,
      ExceptionState&);
  static bool ShouldAllowAccessToV8Context(
      v8::Local<v8::Context> accessing_context,
      v8::Local<v8::Context> target_context,
      ErrorReportOption);

  // Checks if a wrapper creation of the given wrapper type associated with
  // |creation_context| is allowed in |accessing_context|.
  static bool ShouldAllowWrapperCreationOrThrowException(
      v8::Local<v8::Context> accessing_context,
      v8::Local<v8::Context> creation_context,
      const WrapperTypeInfo* wrapper_type_info);

  // Rethrows a cross context exception, that is possibly cross origin.
  // A SecurityError may be rethrown instead of the exception if necessary.
  static void RethrowWrapperCreationException(
      v8::Local<v8::Context> accessing_context,
      v8::Local<v8::Context> creation_context,
      const WrapperTypeInfo* wrapper_type_info,
      v8::Local<v8::Value> cross_context_exception);

 private:
  using ShouldAllowAccessToV8ContextWithExceptionStateFunction =
      bool (*)(v8::Local<v8::Context> accessing_context,
               v8::Local<v8::Context> target_context,
               ExceptionState&);
  using ShouldAllowAccessToV8ContextWithErrorReportOptionFunction =
      bool (*)(v8::Local<v8::Context> accessing_context,
               v8::Local<v8::Context> target_context,
               ErrorReportOption);
  using ShouldAllowWrapperCreationOrThrowExceptionFunction =
      bool (*)(v8::Local<v8::Context> accessing_context,
               v8::Local<v8::Context> creation_context,
               const WrapperTypeInfo* wrapper_type_info);
  using RethrowWrapperCreationExceptionFunction =
      void (*)(v8::Local<v8::Context> accessing_context,
               v8::Local<v8::Context> creation_context,
               const WrapperTypeInfo* wrapper_type_info,
               v8::Local<v8::Value> cross_context_exception);

  static void SetShouldAllowAccessToV8ContextWithExceptionState(
      ShouldAllowAccessToV8ContextWithExceptionStateFunction);
  static void SetShouldAllowAccessToV8ContextWithErrorReportOption(
      ShouldAllowAccessToV8ContextWithErrorReportOptionFunction);
  static void SetShouldAllowWrapperCreationOrThrowException(
      ShouldAllowWrapperCreationOrThrowExceptionFunction);
  static void SetRethrowWrapperCreationException(
      RethrowWrapperCreationExceptionFunction);

  static ShouldAllowAccessToV8ContextWithExceptionStateFunction
      should_allow_access_to_v8context_with_exception_state_;
  static ShouldAllowAccessToV8ContextWithErrorReportOptionFunction
      should_allow_access_to_v8context_with_error_report_option_;
  static ShouldAllowWrapperCreationOrThrowExceptionFunction
      should_allow_wrapper_creation_or_throw_exception_;
  static RethrowWrapperCreationExceptionFunction
      rethrow_wrapper_creation_exception_;

  friend class BindingSecurity;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_BINDING_SECURITY_FOR_PLATFORM_H_
