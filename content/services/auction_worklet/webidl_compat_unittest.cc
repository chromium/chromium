// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/webidl_compat.h"

#include <memory>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace auction_worklet {

class WebIDLCompatTest : public testing::Test {
 public:
  void SetUp() override {
    v8_helper_ = AuctionV8Helper::Create(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    // Here since we're using the same thread for everything, we need to spin
    // the event loop to let AuctionV8Helper finish initializing "off-thread";
    // normally PostTask semantics will ensure that anything that uses it on its
    // thread would happen after such initialization.
    base::RunLoop().RunUntilIdle();
    v8_scope_ =
        std::make_unique<AuctionV8Helper::FullIsolateScope>(v8_helper_.get());
    time_limit_ = v8_helper_->CreateTimeLimit(/*script_timeout=*/absl::nullopt);
    time_limit_scope_ =
        std::make_unique<AuctionV8Helper::TimeLimitScope>(time_limit_.get());
  }

  void TearDown() override {
    v8_scope_.reset();
    time_limit_scope_.reset();
    time_limit_.reset();
    v8_helper_.reset();
    task_environment_.RunUntilIdle();
  }

  // Calls the `make` method in the given script to produce the value passed
  // to DictConverter.
  std::unique_ptr<DictConverter> MakeFromScript(
      v8::Local<v8::Context> context,
      const std::string& script_source) {
    absl::optional<std::string> error;
    v8::MaybeLocal<v8::UnboundScript> maybe_script =
        v8_helper_->Compile(script_source, GURL("https://example.org"),
                            /*debug_id=*/nullptr, error);
    EXPECT_EQ(error.value_or(""), "");
    v8::Local<v8::UnboundScript> script = maybe_script.ToLocalChecked();

    std::vector<std::string> errors;
    EXPECT_TRUE(v8_helper_->RunScript(context, script, /*debug_id=*/nullptr,
                                      time_limit_.get(), errors));
    EXPECT_THAT(errors, ElementsAre());

    v8::MaybeLocal<v8::Value> maybe_result = v8_helper_->CallFunction(
        context, /*debug_id=*/nullptr, "some script", "make", /*args=*/{},
        time_limit_.get(), errors);
    v8::Local<v8::Value> result;
    if (!maybe_result.ToLocal(&result)) {
      result = v8::Undefined(v8_helper_->isolate());
    }
    EXPECT_THAT(errors, ElementsAre());

    return std::make_unique<DictConverter>(v8_helper_.get(), *time_limit_scope_,
                                           "<error prefix>", result);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<AuctionV8Helper> v8_helper_;
  std::unique_ptr<AuctionV8Helper::TimeLimit> time_limit_;
  std::unique_ptr<AuctionV8Helper::TimeLimitScope> time_limit_scope_;
  std::unique_ptr<AuctionV8Helper::FullIsolateScope> v8_scope_;
};

// WebIDL treats undefined as empty dictionary.
TEST_F(WebIDLCompatTest, UndefinedEmptyDict) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    function make() { return undefined; }
  )";

  auto converter = MakeFromScript(context, kScript);

  absl::optional<std::string> out;
  EXPECT_TRUE(converter->GetOptional("a", out));
  EXPECT_FALSE(out.has_value());

  std::string out2;
  EXPECT_FALSE(converter->GetRequired("a", out2));
  EXPECT_EQ("<error prefix> Required field 'a' missing.",
            converter->ErrorMessage());
}

// WebIDL treats null as empty dictionary.
TEST_F(WebIDLCompatTest, NullEmptyDict) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    function make() { return null; }
  )";

  auto converter = MakeFromScript(context, kScript);

  absl::optional<std::string> out;
  EXPECT_TRUE(converter->GetOptional("a", out));
  EXPECT_FALSE(out.has_value());

  std::string out2;
  EXPECT_FALSE(converter->GetRequired("a", out2));
  EXPECT_EQ("<error prefix> Required field 'a' missing.",
            converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, OptionalOrRequired) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    function make() { return {a: "hi"}; }
  )";

  auto converter = MakeFromScript(context, kScript);

  absl::optional<std::string> out;
  std::string out_required;

  EXPECT_TRUE(converter->GetOptional("a", out));
  EXPECT_TRUE(out.has_value());
  EXPECT_EQ("hi", *out);

  EXPECT_TRUE(converter->GetRequired("a", out_required));
  EXPECT_EQ("hi", out_required);

  EXPECT_TRUE(converter->GetOptional("b", out));
  EXPECT_FALSE(out.has_value());

  EXPECT_FALSE(converter->GetRequired("b", out_required));
  EXPECT_EQ("<error prefix> Required field 'b' missing.",
            converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, NullUndefinedValues) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    function make() { return {a: undefined, b: null}; }
  )";

  auto converter = MakeFromScript(context, kScript);

  absl::optional<std::string> out;
  EXPECT_TRUE(converter->GetOptional("a", out));
  EXPECT_FALSE(out.has_value());

  EXPECT_TRUE(converter->GetOptional("b", out));
  EXPECT_TRUE(out.has_value());
  EXPECT_EQ("null", *out);
}

TEST_F(WebIDLCompatTest, NotDict) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    function make() { return 42; }
  )";

  auto converter = MakeFromScript(context, kScript);

  absl::optional<std::string> out;
  EXPECT_FALSE(converter->GetOptional("a", out));
  EXPECT_EQ(
      "<error prefix> Value passed as dictionary is neither object, null, nor "
      "undefined.",
      converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, ErrorLatch) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    function make() { return {b: 42}; }
  )";

  auto converter = MakeFromScript(context, kScript);

  absl::optional<std::string> out;
  std::string out_required;
  EXPECT_TRUE(converter->GetOptional("b", out));
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ("42", *out);

  EXPECT_TRUE(converter->GetRequired("b", out_required));
  EXPECT_EQ("42", out_required);

  EXPECT_FALSE(converter->GetRequired("a", out_required));
  EXPECT_EQ("<error prefix> Required field 'a' missing.",
            converter->ErrorMessage());

  // Further look ups fail.
  EXPECT_FALSE(converter->GetOptional("b", out));
  EXPECT_FALSE(converter->GetRequired("b", out_required));

  // .. and don't mess up the error message.
  EXPECT_EQ("<error prefix> Required field 'a' missing.",
            converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, Double) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    function make() { return {a: 1, b: "2", c: "2.0", d: [3], e: 0/0 } }
  )";

  auto converter = MakeFromScript(context, kScript);
  double out = -1;

  EXPECT_TRUE(converter->GetRequired("a", out));
  EXPECT_EQ(1, out);

  EXPECT_TRUE(converter->GetRequired("b", out));
  EXPECT_EQ(2, out);

  EXPECT_TRUE(converter->GetRequired("c", out));
  EXPECT_EQ(2, out);

  EXPECT_TRUE(converter->GetRequired("d", out));
  EXPECT_EQ(3, out);

  EXPECT_FALSE(converter->GetRequired("e", out));
  EXPECT_EQ(
      "<error prefix> Converting field 'e' to a Number did not produce a "
      "finite double.",
      converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, DoubleCoercion) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      valueOf: () => 4,
      toString: () => 5
    }
    o2 = {
      valueOf: () => "6",
      toString: () => "7"
    }
    o3 = {
      valueOf: () => {throw "valueOf threw"},
      toString: () => {throw "toString threw"}
    }
    function make() { return {a: o1, b: o2, c: o3 } }
  )";

  auto converter = MakeFromScript(context, kScript);
  double out = -1;

  EXPECT_TRUE(converter->GetRequired("a", out));
  EXPECT_EQ(4, out);

  EXPECT_TRUE(converter->GetRequired("b", out));
  EXPECT_EQ(6, out);

  EXPECT_FALSE(converter->GetRequired("c", out));
  EXPECT_EQ("https://example.org/:11 Uncaught valueOf threw.",
            converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, DoubleCoercionNonTermination) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      valueOf: () => { while (true) {} },
      toString: () => 5
    }
    function make() { return {a: o1 } }
  )";

  auto converter = MakeFromScript(context, kScript);
  double out = -1;

  EXPECT_FALSE(converter->GetRequired("a", out));
  EXPECT_EQ("<error prefix> Converting field 'a' to Number timed out.",
            converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, DoubleCoercionResultNotFinite) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      valueOf: () => 0/0,
    }
    function make() { return {a: o1 } }
  )";

  auto converter = MakeFromScript(context, kScript);
  double out = -1;

  EXPECT_FALSE(converter->GetRequired("a", out));
  EXPECT_EQ(
      "<error prefix> Converting field 'a' to a Number did not produce a "
      "finite double.",
      converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, Boolean) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    function make() { return {a: true, b: false, c: 0, d: "100",
                              e: new Object()}; }
  )";

  auto converter = MakeFromScript(context, kScript);
  bool out = false;
  EXPECT_TRUE(converter->GetRequired("a", out));
  EXPECT_TRUE(out);

  EXPECT_TRUE(converter->GetRequired("b", out));
  EXPECT_FALSE(out);

  EXPECT_TRUE(converter->GetRequired("c", out));
  EXPECT_FALSE(out);

  EXPECT_TRUE(converter->GetRequired("d", out));
  EXPECT_TRUE(out);

  EXPECT_TRUE(converter->GetRequired("e", out));
  EXPECT_TRUE(out);

  EXPECT_FALSE(converter->GetRequired("f", out));
  EXPECT_EQ("<error prefix> Required field 'f' missing.",
            converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, String) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    function make() { return {a: 1, b: "2", c: 0/0 } }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::string out;

  EXPECT_TRUE(converter->GetRequired("a", out));
  EXPECT_EQ("1", out);

  EXPECT_TRUE(converter->GetRequired("b", out));
  EXPECT_EQ("2", out);

  EXPECT_TRUE(converter->GetRequired("c", out));
  EXPECT_EQ("NaN", out);
}

TEST_F(WebIDLCompatTest, StringCoercion) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      valueOf: () => 4,
      toString: () => 5
    }
    o2 = {
      valueOf: () => "6",
      toString: () => "7"
    }
    o3 = {
      valueOf: () => {throw "valueOf threw"},
      toString: () => {throw "toString threw"}
    }
    function make() { return {a: o1, b: o2, c: o3 } }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::string out;

  EXPECT_TRUE(converter->GetRequired("a", out));
  EXPECT_EQ("5", out);

  EXPECT_TRUE(converter->GetRequired("b", out));
  EXPECT_EQ("7", out);

  EXPECT_FALSE(converter->GetRequired("c", out));
  EXPECT_EQ("https://example.org/:12 Uncaught toString threw.",
            converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, StringCoercionNonTermination) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      valueOf: () => 4,
      toString: () => { while (true) {} }
    }
    function make() { return {a: o1 } }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::string out;

  EXPECT_FALSE(converter->GetRequired("a", out));
  EXPECT_EQ("<error prefix> Converting field 'a' to String timed out.",
            converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, FieldAccessThrows) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      get a() {
        return "hi";
      },
      get b() {
        throw "oh no!";
      }
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::string out;
  EXPECT_TRUE(converter->GetRequired("a", out));
  EXPECT_EQ("hi", out);

  EXPECT_FALSE(converter->GetRequired("b", out));
  EXPECT_EQ("https://example.org/:7 Uncaught oh no!.",
            converter->ErrorMessage());
}

TEST_F(WebIDLCompatTest, FieldAccessNonTermination) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      get a() {
        while (true) {}
      },
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::string out;
  EXPECT_FALSE(converter->GetRequired("a", out));
  EXPECT_EQ("<error prefix> Execution timed out trying to access field 'a'.",
            converter->ErrorMessage());
}

}  // namespace auction_worklet
