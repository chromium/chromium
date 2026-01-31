// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/binding_access_checker.h"

#include "base/strings/stringprintf.h"
#include "gin/converter.h"

namespace extensions {

BindingAccessChecker::BindingAccessChecker(
    APIAvailabilityCallback api_available)
    : api_available_(std::move(api_available)) {}
BindingAccessChecker::~BindingAccessChecker() = default;

bool BindingAccessChecker::HasAccess(v8::Local<v8::Context> context,
                                     const std::string& full_name) const {
  return api_available_.Run(context, full_name);
}

bool BindingAccessChecker::HasAccessOrThrowError(
    v8::Local<v8::Context> context,
    const std::string& full_name) const {
  if (!HasAccess(context, full_name)) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    isolate->ThrowException(v8::Exception::Error(gin::StringToV8(
        isolate, base::StringPrintf("'%s' is not available in this context.",
                                    full_name.c_str()))));
    return false;
  }

  return true;
}

}  // namespace extensions
