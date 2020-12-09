// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_injection_callback.h"

#include "third_party/blink/public/platform/web_vector.h"

namespace extensions {

ScriptInjectionCallback::ScriptInjectionCallback(
    CompleteCallback injection_completed_callback)
    : injection_completed_callback_(std::move(injection_completed_callback)) {}

ScriptInjectionCallback::~ScriptInjectionCallback() {
}

void ScriptInjectionCallback::Completed(
    const blink::WebVector<v8::Local<v8::Value>>& result) {
  std::vector<v8::Local<v8::Value>> stl_result(result.begin(), result.end());
  std::move(injection_completed_callback_).Run(stl_result);
  delete this;
}

}  // namespace extensions
