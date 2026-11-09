// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class TestExtensionFunction : public ExtensionFunction {
 public:
  ResponseAction Run() override { return RespondNow(NoArguments()); }

  using ExtensionFunction::args;
  using ExtensionFunction::GetMutableArgs;

 protected:
  ~TestExtensionFunction() override = default;
};

TEST(ExtensionFunctionTest, GetMutableArgsAndGetOriginalArgs) {
  content::BrowserTaskEnvironment task_environment;

  for (bool avoid_clone : {false, true}) {
    SCOPED_TRACE(avoid_clone);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(
        extensions_features::kAvoidCloneMutableExtensionFunctionArgs,
        avoid_clone);

    auto function = base::MakeRefCounted<TestExtensionFunction>();
    function->SetArgs(base::ListValue().Append("arg"));
    EXPECT_EQ(function->GetOriginalArgs(), base::ListValue().Append("arg"));

    function->GetMutableArgs().clear();
    EXPECT_TRUE(function->args().empty());

    if (avoid_clone) {
      EXPECT_CHECK_DEATH(function->GetOriginalArgs());
    } else {
      EXPECT_EQ(function->GetOriginalArgs(), base::ListValue().Append("arg"));
    }
  }
}

}  // namespace
}  // namespace extensions
