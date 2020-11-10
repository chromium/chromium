// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MODULE_TEST_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MODULE_TEST_BASE_H_

#include <gtest/gtest.h>
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "v8/include/v8.h"

namespace blink {

// Helper used to enable or disable top-level await in parametrized tests.
class ParametrizedModuleTestBase {
 protected:
  void SetUp(bool use_top_level_await);
  void TearDown() {}
  // Get the results of a ScriptEvaluationResult from a module.
  // If top-level await is enabled, the method will wait for the result
  // Promise to be resolved.
  v8::Local<v8::Value> GetResult(ScriptState* script_state,
                                 ScriptEvaluationResult result);
  // Get the exception of a ScriptEvaluationResult from a module.
  // If top-level await is enabled, the method will wait for the result
  // Promise to be rejected.
  v8::Local<v8::Value> GetException(ScriptState* script_state,
                                    ScriptEvaluationResult result);

 private:
  void SetV8Flags(bool use_top_level_await);
  base::test::ScopedFeatureList feature_list_;
};

class ParametrizedModuleTest : public ParametrizedModuleTestBase,
                               public testing::WithParamInterface<bool> {
 protected:
  void SetUp();

  bool UseTopLevelAwait() { return GetParam(); }
};

// Used in INSTANTIATE_TEST_SUITE_P for returning more readable test names.
struct ParametrizedModuleTestParamName {
  std::string operator()(
      const testing::TestParamInfo<ParametrizedModuleTest::ParamType>& info) {
    return info.param ? "TopLevelAwait" : "noTopLevelAwait";
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MODULE_TEST_BASE_H_
