// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_TEST_FEATURES_NATIVE_HANDLER_H_
#define EXTENSIONS_RENDERER_TEST_FEATURES_NATIVE_HANDLER_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {

class TestFeaturesNativeHandler : public ObjectBackedNativeHandler {
 public:
  explicit TestFeaturesNativeHandler(ScriptContext* context);

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  void GetAPIFeatures(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_TEST_FEATURES_NATIVE_HANDLER_H_
