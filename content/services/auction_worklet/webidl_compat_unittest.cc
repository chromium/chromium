// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/webidl_compat.h"

#include <cmath>
#include <initializer_list>
#include <memory>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
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

  // Calls the `make` method in the given script to produce a value.
  v8::Local<v8::Value> MakeValueFromScript(v8::Local<v8::Context> context,
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
    return result;
  }

  // Calls the `make` method in the given script to produce the value passed
  // to DictConverter.
  std::unique_ptr<DictConverter> MakeFromScript(
      v8::Local<v8::Context> context,
      const std::string& script_source) {
    return std::make_unique<DictConverter>(
        v8_helper_.get(), *time_limit_scope_, "<error prefix> ",
        MakeValueFromScript(context, script_source));
  }

  bool GetSequence(DictConverter* converter,
                   base::StringPiece field,
                   std::vector<v8::Local<v8::Value>>& out) {
    out.clear();  // For tests that re-use `out`.
    bool got_it = false;
    bool result = converter->GetOptionalSequence(
        field, base::BindLambdaForTesting([&]() { got_it = true; }),
        base::BindLambdaForTesting([&](v8::Local<v8::Value> value) -> bool {
          out.push_back(value);
          return true;
        }));
    return got_it && result;
  }

  void ExpectStringList(std::initializer_list<std::string> expected,
                        std::vector<v8::Local<v8::Value>> actual) {
    ASSERT_EQ(expected.size(), actual.size());
    size_t pos = 0;
    for (const std::string& e : expected) {
      v8::Local<v8::Value> a = actual[pos];
      ASSERT_TRUE(a->IsString()) << pos;
      std::string str;
      EXPECT_TRUE(
          gin::Converter<std::string>::FromV8(v8_helper_->isolate(), a, &str))
          << pos;
      EXPECT_EQ(e, str);
      ++pos;
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<AuctionV8Helper> v8_helper_;
  std::unique_ptr<AuctionV8Helper::TimeLimit> time_limit_;
  std::unique_ptr<AuctionV8Helper::TimeLimitScope> time_limit_scope_;
  std::unique_ptr<AuctionV8Helper::FullIsolateScope> v8_scope_;
};

TEST_F(WebIDLCompatTest, StandaloneDouble) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  {
    auto in_value = MakeValueFromScript(context, "make = () => 124.5");
    double out = -1;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test1",
                                   {"v1", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(out, 124.5);
  }

  {
    auto in_value = MakeValueFromScript(context, "make = () => (0/0)");
    double out_unchecked;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test2:", {"v2", "scalar"},
                            in_value, out_unchecked);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kErrorMessage);
    EXPECT_EQ(
        "test2:Converting v2scalar to a Number did not "
        "produce a finite double.",
        res.ConvertToErrorString(v8_helper_->isolate()));
  }

  {
    auto in_value = MakeValueFromScript(context, R"(
      make = () => {
        return {
          valueOf: () => { while(true) {} }
        }
      }
    )");
    double out_unchecked;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test3:", {"v3", "scalar"},
                            in_value, out_unchecked);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kTimeout);
    EXPECT_EQ("test3:Converting v3scalar to Number timed out.",
              res.ConvertToErrorString(v8_helper_->isolate()));
  }

  {
    auto in_value = MakeValueFromScript(context, R"(
      make = () => {
        return {
          valueOf: () => { throw "boo"; }
        }
      }
    )");
    double out_unchecked;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test4:", {"v4", "scalar"},
                            in_value, out_unchecked);
    ASSERT_EQ(res.type(), IdlConvert::Status::Type::kException);
    EXPECT_EQ("https://example.org/:4 Uncaught boo.",
              res.ConvertToErrorString(v8_helper_->isolate()));
    std::string exception_str;
    EXPECT_TRUE(gin::Converter<std::string>::FromV8(
        v8_helper_->isolate(), res.GetException().exception, &exception_str));
    EXPECT_EQ("boo", exception_str);
  }

  // Successful conversions can happen. Here the string '345' turns into
  // Number 345.
  {
    auto in_value = MakeValueFromScript(context, "make = () => '345'");
    double out = -1;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test5",
                                   {"v5", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(out, 345);
  }
}

// unrestricted double, unlike double, doesn't reject +/- inf and NaN.
TEST_F(WebIDLCompatTest, StandaloneUnrestrictedDouble) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  {
    auto in_value = MakeValueFromScript(context, "make = () => 124.5");
    UnrestrictedDouble out;
    out.number = -1;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test1",
                                   {"v1", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(out.number, 124.5);
  }

  {
    auto in_value = MakeValueFromScript(context, "make = () => (0/0)");
    UnrestrictedDouble out;
    out.number = -1;
    auto res = IdlConvert::Convert(v8_helper_->isolate(),
                                   "test2:", {"v2", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_TRUE(std::isnan(out.number));
  }

  {
    auto in_value = MakeValueFromScript(context, R"(
      make = () => {
        return {
          valueOf: () => { while(true) {} }
        }
      }
    )");
    UnrestrictedDouble out_unchecked;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test3:", {"v3", "scalar"},
                            in_value, out_unchecked);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kTimeout);
    EXPECT_EQ("test3:Converting v3scalar to Number timed out.",
              res.ConvertToErrorString(v8_helper_->isolate()));
  }

  {
    auto in_value = MakeValueFromScript(context, R"(
      make = () => {
        return {
          valueOf: () => { throw "boo"; }
        }
      }
    )");
    UnrestrictedDouble out_unchecked;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test4:", {"v4", "scalar"},
                            in_value, out_unchecked);
    ASSERT_EQ(res.type(), IdlConvert::Status::Type::kException);
    EXPECT_EQ("https://example.org/:4 Uncaught boo.",
              res.ConvertToErrorString(v8_helper_->isolate()));
    std::string exception_str;
    EXPECT_TRUE(gin::Converter<std::string>::FromV8(
        v8_helper_->isolate(), res.GetException().exception, &exception_str));
    EXPECT_EQ("boo", exception_str);
  }
}

TEST_F(WebIDLCompatTest, StandaloneBool) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  {
    auto in_value = MakeValueFromScript(context, "make = () => false");
    bool out = true;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test1",
                                   {"v1", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_FALSE(out);
  }

  {
    auto in_value = MakeValueFromScript(context, "make = () => true");
    bool out = false;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test2",
                                   {"v2", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_TRUE(out);
  }

  {
    auto in_value = MakeValueFromScript(context, "make = () => 0");
    bool out = true;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test3",
                                   {"v3", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_FALSE(out);
  }

  {
    auto in_value = MakeValueFromScript(context, "make = () => 'hi'");
    bool out = false;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test4",
                                   {"v4", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_TRUE(out);
  }
}

TEST_F(WebIDLCompatTest, StandaloneString) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  {
    auto in_value = MakeValueFromScript(context, "make = () => 'hi'");
    std::string out;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test1",
                                   {"v1", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(out, "hi");
  }

  {
    auto in_value = MakeValueFromScript(context, R"(
      make = () => {
        return {
          toString: () => { while(true) {} }
        }
      }
    )");
    std::string out_unchecked;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test2:", {"v2", "scalar"},
                            in_value, out_unchecked);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kTimeout);
    EXPECT_EQ("test2:Converting v2scalar to String timed out.",
              res.ConvertToErrorString(v8_helper_->isolate()));
  }

  {
    auto in_value = MakeValueFromScript(context, R"(
      make = () => {
        return {
          toString: () => { throw "boo"; }
        }
      }
    )");
    std::string out_unchecked;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test3:", {"v3", "scalar"},
                            in_value, out_unchecked);
    ASSERT_EQ(res.type(), IdlConvert::Status::Type::kException);
    EXPECT_EQ("https://example.org/:4 Uncaught boo.",
              res.ConvertToErrorString(v8_helper_->isolate()));
    std::string exception_str;
    EXPECT_TRUE(gin::Converter<std::string>::FromV8(
        v8_helper_->isolate(), res.GetException().exception, &exception_str));
    EXPECT_EQ("boo", exception_str);
  }

  // Successful conversions can happen.
  {
    auto in_value = MakeValueFromScript(context, "make = () => 123");
    std::string out;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test4",
                                   {"v4", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(out, "123");
  }
}

TEST_F(WebIDLCompatTest, StandaloneAny) {
  // 'any' handling is just passthrough; it's there to help the dictionary
  // code out.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, "make = () => 'hi'");
  v8::Local<v8::Value> out;
  auto res = IdlConvert::Convert(v8_helper_->isolate(), "test1",
                                 {"v1", "scalar"}, in_value, out);
  EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
  EXPECT_EQ(out, in_value);
}

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
  EXPECT_EQ("<error prefix> Required field 'a' is undefined.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
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
  EXPECT_EQ("<error prefix> Required field 'a' is undefined.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
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
  EXPECT_EQ("<error prefix> Required field 'b' is undefined.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
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
  EXPECT_FALSE(converter->FailureIsTimeout());
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
  EXPECT_EQ("<error prefix> Required field 'a' is undefined.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());

  // Further look ups fail.
  EXPECT_FALSE(converter->GetOptional("b", out));
  EXPECT_FALSE(converter->GetRequired("b", out_required));

  // .. and don't mess up the error message.
  EXPECT_EQ("<error prefix> Required field 'a' is undefined.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
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
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, UnrestrictedDouble) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    function make() { return {a: 1, b: "2", c: "2.5", d: [3], e: 0/0,
                              f: 1/0, g: -1/0} }
  )";

  auto converter = MakeFromScript(context, kScript);
  UnrestrictedDouble out;
  out.number = -1;

  EXPECT_TRUE(converter->GetRequired("a", out));
  EXPECT_EQ(1, out.number);

  EXPECT_TRUE(converter->GetRequired("b", out));
  EXPECT_EQ(2, out.number);

  EXPECT_TRUE(converter->GetRequired("c", out));
  EXPECT_EQ(2.5, out.number);

  EXPECT_TRUE(converter->GetRequired("d", out));
  EXPECT_EQ(3, out.number);

  EXPECT_TRUE(converter->GetRequired("e", out));
  EXPECT_TRUE(std::isnan(out.number));

  EXPECT_TRUE(converter->GetRequired("f", out));
  EXPECT_TRUE(std::isinf(out.number));
  EXPECT_FALSE(std::signbit(out.number));

  EXPECT_TRUE(converter->GetRequired("g", out));
  EXPECT_TRUE(std::isinf(out.number));
  EXPECT_TRUE(std::signbit(out.number));
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
  v8::MaybeLocal<v8::Value> exception = converter->FailureException();
  ASSERT_FALSE(exception.IsEmpty());
  std::string exception_str;
  EXPECT_TRUE(gin::Converter<std::string>::FromV8(
      v8_helper_->isolate(), exception.ToLocalChecked(), &exception_str));
  EXPECT_EQ("valueOf threw", exception_str);
  EXPECT_FALSE(converter->FailureIsTimeout());
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
  EXPECT_TRUE(converter->FailureIsTimeout());
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
  EXPECT_FALSE(converter->FailureIsTimeout());
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
  EXPECT_EQ("<error prefix> Required field 'f' is undefined.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
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
  v8::MaybeLocal<v8::Value> exception = converter->FailureException();
  ASSERT_FALSE(exception.IsEmpty());
  std::string exception_str;
  EXPECT_TRUE(gin::Converter<std::string>::FromV8(
      v8_helper_->isolate(), exception.ToLocalChecked(), &exception_str));
  EXPECT_EQ("toString threw", exception_str);
  EXPECT_FALSE(converter->FailureIsTimeout());
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
  EXPECT_TRUE(converter->FailureIsTimeout());
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
  v8::MaybeLocal<v8::Value> exception = converter->FailureException();
  ASSERT_FALSE(exception.IsEmpty());
  std::string exception_str;
  EXPECT_TRUE(gin::Converter<std::string>::FromV8(
      v8_helper_->isolate(), exception.ToLocalChecked(), &exception_str));
  EXPECT_EQ("oh no!", exception_str);
  EXPECT_FALSE(converter->FailureIsTimeout());
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
  EXPECT_TRUE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, Sequence) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      a: ["1", "2", "3"],
      b: new String("abcd"),
      c: (function*() {
        yield "well";
        yield "pipe";
      })()
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_TRUE(GetSequence(converter.get(), "a", out));
  ExpectStringList({"1", "2", "3"}, out);

  EXPECT_TRUE(GetSequence(converter.get(), "b", out));
  ExpectStringList({"a", "b", "c", "d"}, out);

  EXPECT_TRUE(GetSequence(converter.get(), "c", out));
  ExpectStringList({"well", "pipe"}, out);
}

TEST_F(WebIDLCompatTest, EmptySequence) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      a: [],
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  bool saw_a = false;
  bool saw_a_item = false;

  EXPECT_TRUE(converter->GetOptionalSequence(
      "a", base::BindLambdaForTesting([&]() { saw_a = true; }),
      base::BindLambdaForTesting([&](v8::Local<v8::Value> item) -> bool {
        saw_a_item = true;
        return false;
      })));
  EXPECT_TRUE(saw_a);
  EXPECT_FALSE(saw_a_item);

  bool saw_b = false;
  bool saw_b_item = false;

  EXPECT_TRUE(converter->GetOptionalSequence(
      "b", base::BindLambdaForTesting([&]() { saw_b = true; }),
      base::BindLambdaForTesting([&](v8::Local<v8::Value> item) -> bool {
        saw_b_item = true;
        return false;
      })));
  EXPECT_FALSE(saw_b);
  EXPECT_FALSE(saw_b_item);
}

TEST_F(WebIDLCompatTest, SeqItemError) {
  // Test of `item_callback` returning failure in the middle of a sequence.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      f1: ["a", "b", "error", "d"],
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  bool saw_field = false;
  std::vector<v8::Local<v8::Value>> out;

  EXPECT_FALSE(converter->GetOptionalSequence(
      "f1", base::BindLambdaForTesting([&]() { saw_field = true; }),
      base::BindLambdaForTesting([&](v8::Local<v8::Value> item) -> bool {
        std::string str;
        EXPECT_TRUE(gin::Converter<std::string>::FromV8(v8_helper_->isolate(),
                                                        item, &str));
        if (str == "error") {
          return false;
        }
        out.push_back(item);
        return true;
      })));
  ExpectStringList({"a", "b"}, out);
  EXPECT_EQ(
      "<error prefix> Conversion for an item for sequence field 'f1' failed.",
      converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SeqItemErrorPropagation) {
  // Test of conversion in the middle of sequence failing and propagating
  // an exception.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      f1: [{"f": 1}, {"f":2}, {"f": {valueOf: () => { throw "Ouch" } } } ],
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  bool saw_field = false;
  std::vector<double> out;

  EXPECT_FALSE(converter->GetOptionalSequence(
      "f1", base::BindLambdaForTesting([&]() { saw_field = true; }),
      base::BindLambdaForTesting([&](v8::Local<v8::Value> item) -> bool {
        DictConverter inner(v8_helper_.get(), *time_limit_scope_,
                            "'f1' entry: ", item);
        double entry;
        bool ok = inner.GetRequired("f", entry);
        if (ok) {
          out.push_back(entry);
          return true;
        } else {
          converter->PropagateErrorsFrom(inner);
          return false;
        }
      })));
  EXPECT_THAT(out, ElementsAre(1.0, 2.0));
  EXPECT_EQ("https://example.org/:3 Uncaught Ouch.", converter->ErrorMessage());
  v8::MaybeLocal<v8::Value> exception = converter->FailureException();
  ASSERT_FALSE(exception.IsEmpty());
  std::string exception_str;
  EXPECT_TRUE(gin::Converter<std::string>::FromV8(
      v8_helper_->isolate(), exception.ToLocalChecked(), &exception_str));
  EXPECT_EQ("Ouch", exception_str);
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SeqItemTimeoutPropagation) {
  // Test of conversion in the middle of sequence timing out and the propagation
  // of that.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      f1: [{"f": 1}, {"f":2}, {"f": {valueOf: () => { while(true) {} } } } ],
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  bool saw_field = false;
  std::vector<double> out;

  EXPECT_FALSE(converter->GetOptionalSequence(
      "f1", base::BindLambdaForTesting([&]() { saw_field = true; }),
      base::BindLambdaForTesting([&](v8::Local<v8::Value> item) -> bool {
        DictConverter inner(v8_helper_.get(), *time_limit_scope_,
                            "'f1' entry: ", item);
        double entry;
        bool ok = inner.GetRequired("f", entry);
        if (ok) {
          out.push_back(entry);
          return true;
        } else {
          converter->PropagateErrorsFrom(inner);
          return false;
        }
      })));
  EXPECT_THAT(out, ElementsAre(1.0, 2.0));
  EXPECT_EQ("'f1' entry: Converting field 'f' to Number timed out.",
            converter->ErrorMessage());
  v8::MaybeLocal<v8::Value> exception = converter->FailureException();
  EXPECT_TRUE(exception.IsEmpty());
  EXPECT_TRUE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceSimpleIter) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    let iterable = {}
    iterable[Symbol.iterator] = () => {
      let pos = 0;
      return {
        next() {
          let result = {
            value: String(pos)
          }
          ++pos;
          if (pos === 5) {
            result.done = true;
          }
          return result;
        }
      }
    }

    o1 = {
      a: iterable
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_TRUE(GetSequence(converter.get(), "a", out));
  ExpectStringList({"0", "1", "2", "3"}, out);
}

TEST_F(WebIDLCompatTest, SequenceNonObj) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      a: 42,
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("<error prefix> Sequence field 'a' must be an Object.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceNonIter) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      a: {},
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("<error prefix> Trouble iterating over 'a'.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceNonIter2) {
  // @@iterator isn't a function.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      a: {},
    }
    o1.a[Symbol.iterator] = {}
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("<error prefix> Trouble iterating over 'a'.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceNonIter3) {
  // Calling the returned iterator failed.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      a: {},
    }
    o1.a[Symbol.iterator] = () => { throw "no iterating!" }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("https://example.org/:5 Uncaught no iterating!.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceNonIter4) {
  // Returned next method isn't an object.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      a: {},
    }
    o1.a[Symbol.iterator] = () => { return {next: 5} }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("<error prefix> Trouble iterating over 'a'.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceNonIter5) {
  // Returned next method isn't a function.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      a: {},
    }
    o1.a[Symbol.iterator] = () => { return {next: {}} }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("<error prefix> Trouble iterating over 'a'.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceNonIter6) {
  // Returned next method fails.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      a: {},
    }
    o1.a[Symbol.iterator] = () => { return {next: () => { throw "boo" } } }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("https://example.org/:5 Uncaught boo.", converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceNonIter7) {
  // Getting "done" fails.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      a: {},
    }
    o1.a[Symbol.iterator] = () => {
      return {
        next: () => {
          return {
            get done() { throw "dunno"; }
          }
        }
      }
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("https://example.org/:9 Uncaught dunno.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceNonIter8) {
  // Getting "value" fails.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    o1 = {
      a: {},
    }
    o1.a[Symbol.iterator] = () => {
      return {
        next: () => {
          return {
            get value() { throw "have an abrupt completion"; }
          }
        }
      }
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("https://example.org/:9 Uncaught have an abrupt completion.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceInfiniteIter) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    let iterable = {}
    iterable[Symbol.iterator] = () => {
      return {
        next() {
          return {
            value: "aaaa"
          }
        }
      }
    }

    o1 = {
      a: iterable
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("<error prefix> Length limit for sequence field 'a' exceeded.",
            converter->ErrorMessage());
  EXPECT_FALSE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceNonTermIter) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    let iterable = {}
    iterable[Symbol.iterator] = () => {
      return {
        next() {
          while (true) {}
        }
      }
    }

    o1 = {
      a: iterable
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("<error prefix> Timeout iterating over 'a'.",
            converter->ErrorMessage());
  EXPECT_TRUE(converter->FailureIsTimeout());
}

TEST_F(WebIDLCompatTest, SequenceUnsetValueOk) {
  // The iterator doesn't actually have to set value if it wants to produce
  // undefined (thanks, MDN!).
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    let iterable = {}
    iterable[Symbol.iterator] = () => {
      let pos = 0;
      return {
        next() {
          let result = {
          }
          ++pos;
          if (pos === 5) {
            result.done = true;
          }
          return result;
        }
      }
    }

    o1 = {
      a: iterable
    }
    function make() { return o1 }
  )";

  auto converter = MakeFromScript(context, kScript);
  std::vector<v8::Local<v8::Value>> out;
  EXPECT_TRUE(GetSequence(converter.get(), "a", out));
  ASSERT_EQ(out.size(), 4u);
  for (const auto& entry : out) {
    EXPECT_TRUE(entry->IsUndefined());
  }
}

}  // namespace auction_worklet
