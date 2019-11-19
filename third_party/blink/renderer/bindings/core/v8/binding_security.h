/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_BINDING_SECURITY_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_BINDING_SECURITY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/binding_security_for_platform.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWindow;
class ExceptionState;
class Frame;
class LocalDOMWindow;
class Location;
class Node;
struct WrapperTypeInfo;

// BindingSecurity provides utility functions that determine access permission
// between two realms. For example, is the current Window allowed to access the
// target window?
class CORE_EXPORT BindingSecurity {
  STATIC_ONLY(BindingSecurity);

 public:
  using ErrorReportOption = BindingSecurityForPlatform::ErrorReportOption;

  static void Init();

  // Checks if the caller (|accessing_window|) is allowed to access the JS
  // receiver object (|target|), where the receiver object is the JS object
  // for which the DOM attribute or DOM operation is being invoked (in the
  // form of receiver.domAttr or receiver.domOp()).
  // Note that only Window and Location objects are cross-origin accessible, so
  // the receiver object must be of type DOMWindow or Location.
  //
  // DOMWindow
  static bool ShouldAllowAccessTo(const LocalDOMWindow* accessing_window,
                                  const DOMWindow* target,
                                  ExceptionState&);
  static bool ShouldAllowAccessTo(const LocalDOMWindow* accessing_window,
                                  const DOMWindow* target,
                                  ErrorReportOption);

  // Location
  static bool ShouldAllowAccessTo(const LocalDOMWindow* accessing_window,
                                  const Location* target,
                                  ExceptionState&);
  static bool ShouldAllowAccessTo(const LocalDOMWindow* accessing_window,
                                  const Location* target,
                                  ErrorReportOption);

  // Checks if the caller (|accessing_window|) is allowed to access the JS
  // returned object (|target|), where the returned object is the JS object
  // which is returned as a result of invoking a DOM attribute or DOM
  // operation (in the form of
  //   var x = receiver.domAttr // or receiver.domOp()
  // where |x| is the returned object).
  // See window.frameElement for example, which may return a frame object.
  // The object returned from window.frameElement must be the same origin if
  // it's not null.
  //
  // Node
  static bool ShouldAllowAccessTo(const LocalDOMWindow* accessing_window,
                                  const Node* target,
                                  ExceptionState&);
  static bool ShouldAllowAccessTo(const LocalDOMWindow* accessing_window,
                                  const Node* target,
                                  ErrorReportOption);

  // These overloads should be used only when checking a general access from
  // one context to another context.  For access to a receiver object or
  // returned object, you should use the above overloads.
  static bool ShouldAllowAccessToFrame(const LocalDOMWindow* accessing_window,
                                       const Frame* target,
                                       ExceptionState&);
  static bool ShouldAllowAccessToFrame(const LocalDOMWindow* accessing_window,
                                       const Frame* target,
                                       ErrorReportOption);

  // These overloads should be used only when checking a general access from
  // one context to another context.  For access to a receiver object or
  // returned object, you should use the above overloads.
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

  static void FailedAccessCheckFor(v8::Isolate*,
                                   const WrapperTypeInfo*,
                                   v8::Local<v8::Object> holder);

 private:
  // Returns true if |accessingWindow| is allowed named access to |targetWindow|
  // because they're the same origin.  Note that named access should be allowed
  // even if they're cross origin as long as the browsing context name matches
  // the browsing context container's name.
  //
  // Unlike shouldAllowAccessTo, this function returns true even when
  // |accessingWindow| or |targetWindow| is a RemoteDOMWindow, but remember that
  // only limited operations are allowed on a RemoteDOMWindow.
  //
  // This function should be only used from V8Window::NamedPropertyGetterCustom.
  friend class V8Window;
  static bool ShouldAllowNamedAccessTo(const DOMWindow* accessing_window,
                                       const DOMWindow* target_window);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_BINDING_SECURITY_H_
