// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_view_util.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/extra_response_data.mojom.h"
#include "extensions/common/mojom/response_wrapper.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class TestExtensionFunction : public ExtensionFunction {
 public:
  ResponseAction Run() override { return RespondNow(NoArguments()); }

  void RespondWithJsonArgument(std::string_view json) {
    Respond(JsonArgument(mojo_base::BigBuffer(base::as_byte_span(json))));
  }

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

// Verifies that a JSON argument is sent in the `json` field of
// `ResponseWrapper`.
TEST(ExtensionFunctionTest, JsonArgument) {
  content::BrowserTaskEnvironment task_environment;

  constexpr std::string_view kJson = R"({"foo":[1,"bar"]})";
  auto function = base::MakeRefCounted<TestExtensionFunction>();
  base::test::TestFuture<ExtensionFunction::ResponseType,
                         mojom::ResponseWrapperPtr, const std::string&,
                         mojom::ExtraResponseDataPtr>
      response_future;
  function->set_response_callback(response_future.GetCallback());
  function->RespondWithJsonArgument(kJson);

  auto [type, response_wrapper, error, extra_data] = response_future.Take();
  EXPECT_EQ(ExtensionFunction::ResponseType::kSucceeded, type);
  EXPECT_TRUE(error.empty());
  EXPECT_FALSE(extra_data);
  ASSERT_TRUE(response_wrapper->is_json());
  EXPECT_EQ(kJson,
            base::as_string_view(base::span(response_wrapper->get_json())));
}

// Verifies that GetResultListForTest() returns the parsed JSON argument when
// results are preserved for testing.
TEST(ExtensionFunctionTest, JsonArgumentPreservedForTesting) {
  content::BrowserTaskEnvironment task_environment;

  auto function = base::MakeRefCounted<TestExtensionFunction>();
  function->preserve_results_for_testing();
  base::test::TestFuture<ExtensionFunction::ResponseType,
                         mojom::ResponseWrapperPtr, const std::string&,
                         mojom::ExtraResponseDataPtr>
      response_future;
  function->set_response_callback(response_future.GetCallback());
  function->RespondWithJsonArgument(R"({"foo":[1,"bar"]})");

  auto [type, response_wrapper, error, extra_data] = response_future.Take();
  EXPECT_TRUE(response_wrapper->is_json());

  ASSERT_TRUE(function->GetResultListForTest());
  EXPECT_EQ(base::ListValue().Append(base::DictValue().Set(
                "foo", base::ListValue().Append(1).Append("bar"))),
            *function->GetResultListForTest());
}

}  // namespace
}  // namespace extensions
