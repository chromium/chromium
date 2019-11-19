// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_CROSS_ORIGIN_CALLBACK_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_CROSS_ORIGIN_CALLBACK_INFO_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

// Simple adapter that is used in place of v8::PropertyCallbackInfo and
// v8::FunctionCallbackInfo for getters and setters that are accessible
// cross-origin. This is needed because a named access check interceptor takes
// a v8::PropertyCallbackInfo<v8::Value> or v8::FunctionCallbackInfo<v8::Value>
// argument, while a normal setter interceptor takes a
// v8::PropertyCallbackInfo<void> or v8::FunctionCallbackInfo<v8::Value>
// argument.
//
// Since the generated bindings only care about two fields (the isolate and the
// holder), the generated bindings just substitutes this for the normal
// v8::PropertyCallbackInfo and v8::FunctionCallbackInfo argument, so the same
// generated function can be used to handle intercepted cross-origin sets and
// normal sets.
class V8CrossOriginCallbackInfo {
  STACK_ALLOCATED();

 public:
  explicit V8CrossOriginCallbackInfo(
      const v8::PropertyCallbackInfo<v8::Value>& info)
      : isolate_(info.GetIsolate()), holder_(info.Holder()) {}
  explicit V8CrossOriginCallbackInfo(const v8::PropertyCallbackInfo<void>& info)
      : isolate_(info.GetIsolate()), holder_(info.Holder()) {}
  explicit V8CrossOriginCallbackInfo(
      const v8::FunctionCallbackInfo<v8::Value>& info)
      : isolate_(info.GetIsolate()), holder_(info.Holder()) {}

  v8::Isolate* GetIsolate() const { return isolate_; }
  v8::Local<v8::Object> Holder() const { return holder_; }

 private:
  v8::Isolate* isolate_;
  v8::Local<v8::Object> holder_;

  V8CrossOriginCallbackInfo(const V8CrossOriginCallbackInfo&) = delete;
  V8CrossOriginCallbackInfo& operator=(const V8CrossOriginCallbackInfo&) =
      delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_CROSS_ORIGIN_CALLBACK_INFO_H_
