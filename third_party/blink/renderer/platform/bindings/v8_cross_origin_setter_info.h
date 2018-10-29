// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_CROSS_ORIGIN_SETTER_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_CROSS_ORIGIN_SETTER_INFO_H_

#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "v8/include/v8.h"

namespace blink {

// Simple adapter that is used in place of v8::PropertyCallbackInfo for setters
// that are accessible cross-origin. This is needed because a named access check
// interceptor takes a v8::PropertyCallbackInfo<v8::Value> argument, while a
// normal setter interceptor takes a v8::PropertyCallbackInfo<void> argument.
//
// Since the generated bindings only care about two fields (the isolate and the
// holder), the generated bindings just substitutes this for the normal
// v8::PropertyCallbackInfo argument, so the same generated function can be used
// to handle intercepted cross-origin sets and normal sets.
class V8CrossOriginSetterInfo {
  STACK_ALLOCATED();

 public:
  V8CrossOriginSetterInfo(v8::Isolate* isolate, v8::Local<v8::Object> holder)
      : isolate_(isolate), holder_(holder) {}

  v8::Isolate* GetIsolate() const { return isolate_; }
  v8::Local<v8::Object> Holder() const { return holder_; }

 private:
  v8::Isolate* isolate_;
  v8::Local<v8::Object> holder_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_V8_CROSS_ORIGIN_SETTER_INFO_H_
