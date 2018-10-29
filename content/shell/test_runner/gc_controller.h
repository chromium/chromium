// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_GC_CONTROLLER_H_
#define CONTENT_SHELL_TEST_RUNNER_GC_CONTROLLER_H_

#include "base/macros.h"
#include "gin/wrappable.h"

namespace blink {
class WebLocalFrame;
}

namespace gin {
class Arguments;
}

namespace test_runner {

class TestInterfaces;

class GCController : public gin::Wrappable<GCController> {
 public:
  static gin::WrapperInfo kWrapperInfo;
  static void Install(TestInterfaces* interfaces, blink::WebLocalFrame* frame);

 private:
  // In the first GC cycle, a weak callback of the DOM wrapper is called back
  // and the weak callback disposes a persistent handle to the DOM wrapper.
  // In the second GC cycle, the DOM wrapper is reclaimed.
  // Given that two GC cycles are needed to collect one DOM wrapper,
  // more than two GC cycles are needed to collect all DOM wrappers
  // that are chained. Seven GC cycles look enough in most tests.
  static constexpr int kNumberOfGCsForFullCollection = 7;

  explicit GCController(TestInterfaces* interfaces);
  ~GCController() override;

  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  void Collect(const gin::Arguments& args);
  void CollectAll(const gin::Arguments& args);
  void AsyncCollectAll(const gin::Arguments& args);
  void MinorCollect(const gin::Arguments& args);

  void AsyncCollectAllWithEmptyStack(
      v8::UniquePersistent<v8::Function> callback);

  TestInterfaces* interfaces_;

  DISALLOW_COPY_AND_ASSIGN(GCController);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_GC_CONTROLLER_H_
