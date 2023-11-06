// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TEST_CROS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TEST_CROS_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"

namespace blink {

class FakeMLService;
class FakeMLModelLoader;
class FakeWebNNModel;
class ScopedSetMLServiceBinder;

// This class sets up MLService on CrOS to infer tflite model that is converted
// from WebNN Graph.
class ScopedMLService final {
  STACK_ALLOCATED();

 public:
  ScopedMLService();
  ~ScopedMLService();

  void SetUpMLService(V8TestingScope& scope);

 private:
  const std::unique_ptr<FakeMLModelLoader> loader_;
  const std::unique_ptr<FakeWebNNModel> model_;
  const std::unique_ptr<FakeMLService> ml_service_;
  std::unique_ptr<ScopedSetMLServiceBinder> ml_service_binder_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_GRAPH_TEST_CROS_H_
