// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/web_agent_api_interface.h.tmpl
// by the script code_generator_web_agent_api.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_INTERFACE_3_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_INTERFACE_3_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {
class TestInterface3;
}

namespace web {

class TestInterface3 : public blink::GarbageCollected<TestInterface3> {
 public:
  virtual ~TestInterface3() = default;

  static TestInterface3* Create(blink::TestInterface3*);

  void Trace(blink::Visitor*);

 protected:
  explicit TestInterface3(blink::TestInterface3* test_interface_3);
  blink::TestInterface3* test_interface_3() const;

 private:
  blink::Member<blink::TestInterface3> test_interface_3_;
};

}  // namespace web

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_TEST_INTERFACE_3_H_
