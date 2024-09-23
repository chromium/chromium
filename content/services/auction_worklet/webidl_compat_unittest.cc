// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/webidl_compat.h"

#include <cmath>
#include <initializer_list>
#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "gin/converter.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function.h"

using testing::ElementsAre;
using testing::Pair;

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
    // Using a large timeout because bots are sometimes very slow.
    time_limit_ =
        v8_helper_->CreateTimeLimit(/*script_timeout=*/base::Milliseconds(500));
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

  // Compiles and runs script, returning error vector.
  std::vector<std::string> RunScript(v8::Local<v8::Context> context,
                                     const std::string& script_source,
                                     AuctionV8Helper::Result expect_result) {
    std::optional<std::string> error;
    v8::MaybeLocal<v8::UnboundScript> maybe_script =
        v8_helper_->Compile(script_source, GURL("https://example.org"),
                            /*debug_id=*/nullptr, error);
    EXPECT_EQ(error.value_or(""), "");
    v8::Local<v8::UnboundScript> script = maybe_script.ToLocalChecked();

    std::vector<std::string> errors;
    EXPECT_EQ(expect_result,
              v8_helper_->RunScript(context, script, /*debug_id=*/nullptr,
                                    time_limit_.get(), errors));
    return errors;
  }

  // Calls the "make" method in the given script to produce a value.
  v8::Local<v8::Value> MakeValueFromScript(v8::Local<v8::Context> context,
                                           const std::string& script_source) {
    std::vector<std::string> errors =
        RunScript(context, script_source,
                  /*expect_result=*/AuctionV8Helper::Result::kSuccess);
    EXPECT_THAT(errors, ElementsAre());

    v8::MaybeLocal<v8::Value> maybe_result;
    v8_helper_->CallFunction(context, /*debug_id=*/nullptr, "some script",
                             "make", /*args=*/{}, time_limit_.get(),
                             maybe_result, errors);
    v8::Local<v8::Value> result;
    if (!maybe_result.ToLocal(&result)) {
      result = v8::Undefined(v8_helper_->isolate());
    }
    EXPECT_THAT(errors, ElementsAre());
    return result;
  }

  // Calls the "make" method in the given script, and passes it to a freshly
  // created DictConverter.
  std::unique_ptr<DictConverter> MakeFromScript(
      v8::Local<v8::Context> context,
      const std::string& script_source) {
    return std::make_unique<DictConverter>(
        v8_helper_.get(), *time_limit_scope_, "<error prefix> ",
        MakeValueFromScript(context, script_source));
  }

  bool GetSequence(DictConverter* converter,
                   std::string_view field,
                   v8::LocalVector<v8::Value>& out) {
    out.clear();  // For tests that re-use `out`.
    bool got_it = false;
    bool result = converter->GetOptionalSequence(
        field, base::BindLambdaForTesting([&]() { got_it = true; }),
        base::BindLambdaForTesting(
            [&](v8::Local<v8::Value> value) -> IdlConvert::Status {
              out.push_back(value);
              return IdlConvert::Status::MakeSuccess();
            }));
    return got_it && result;
  }

  void ExpectStringList(std::initializer_list<std::string> expected,
                        v8::LocalVector<v8::Value> actual) {
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

  // Creates a JS entry point "binding" that dispatches to `binding_callback_`.
  void SetBinding(v8::Local<v8::Context> context) {
    v8::Local<v8::External> v8_this =
        v8::External::New(v8_helper_->isolate(), this);
    v8::Local<v8::Function> v8_function =
        v8::Function::New(context, &WebIDLCompatTest::DispatchBinding, v8_this)
            .ToLocalChecked();
    context->Global()
        ->Set(context, v8_helper_->CreateStringFromLiteral("binding"),
              v8_function)
        .Check();
  }

  // Helper for the various ArgsConverter tests.
  //
  // Binds a method called "binding" that takes two arguments and attempts to
  // convert the first to a string, the second to a double. Any conversion
  // errors are reported to V8.
  void SetBindingForArgConverterTest(v8::Local<v8::Context> context) {
    SetBinding(context);
    binding_callback_ = base::BindRepeating(
        [](scoped_refptr<AuctionV8Helper> v8_helper, WebIDLCompatTest& fixture,
           const v8::FunctionCallbackInfo<v8::Value>& args) {
          AuctionV8Helper::TimeLimitScope time_limit_scope(
              v8_helper->GetTimeLimit());
          ArgsConverter args_convert(v8_helper.get(), time_limit_scope,
                                     "binding(): ", &args,
                                     /*min_required_args=*/2);
          bool ok = args_convert.ConvertArg(0, "arg0", fixture.arg0_) &&
                    args_convert.ConvertArg(1, "arg1", fixture.arg1_);

          auto status = args_convert.TakeStatus();
          EXPECT_EQ(status.is_success(), ok);
          status.PropagateErrorsToV8(v8_helper.get());
        },
        v8_helper_, std::ref(*this));
  }

 protected:
  static void DispatchBinding(const v8::FunctionCallbackInfo<v8::Value>& args) {
    WebIDLCompatTest* self = static_cast<WebIDLCompatTest*>(
        v8::External::Cast(*args.Data())->Value());
    if (!self->binding_callback_.is_null()) {
      self->binding_callback_.Run(args);
    }
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<AuctionV8Helper> v8_helper_;
  std::unique_ptr<AuctionV8Helper::TimeLimit> time_limit_;
  std::unique_ptr<AuctionV8Helper::TimeLimitScope> time_limit_scope_;
  std::unique_ptr<AuctionV8Helper::FullIsolateScope> v8_scope_;
  base::RepeatingCallback<void(const v8::FunctionCallbackInfo<v8::Value>&)>
      binding_callback_;

  // Output from SetBindingForArgConverterTest.
  std::string arg0_;
  double arg1_ = -1;
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

TEST_F(WebIDLCompatTest, StandaloneString16) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  {
    auto in_value = MakeValueFromScript(context, "make = () => '\u0491'");
    std::u16string out;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test1",
                                   {"v1", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(out.length(), 1u);
    EXPECT_EQ(out[0], 0x0491);
  }

  {
    const char kScript[] = R"(
      function make() {
        return {
          toString: () => {
            return {};
          }
        }
      }
    )";
    auto in_value = MakeValueFromScript(context, kScript);
    std::u16string out_unchecked;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test2",
                                   {"v2", "scalar"}, in_value, out_unchecked);
    ASSERT_FALSE(res.is_success());
    EXPECT_EQ(
        "undefined:0 Uncaught TypeError: Cannot convert object to primitive "
        "value.",
        res.ConvertToErrorString(v8_helper_->isolate()));
  }

  {
    auto in_value = MakeValueFromScript(context, "make = () => 12");
    std::u16string out;
    auto res = IdlConvert::Convert(v8_helper_->isolate(), "test3",
                                   {"v1", "scalar"}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    ASSERT_EQ(out.length(), 2u);
    EXPECT_EQ(out[0], '1');
    EXPECT_EQ(out[1], '2');
  }
}

TEST_F(WebIDLCompatTest, StandaloneBigInt) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  {
    auto in_value = MakeValueFromScript(context, "make = () => BigInt(123)");
    v8::Local<v8::BigInt> out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test1", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    ASSERT_FALSE(out.IsEmpty());
    bool lossless = false;
    EXPECT_EQ(123, out->Int64Value(&lossless));
    EXPECT_TRUE(lossless);
  }

  {
    auto in_value = MakeValueFromScript(context, "make = () => '123'");
    v8::Local<v8::BigInt> out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test2", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    ASSERT_FALSE(out.IsEmpty());
    bool lossless = false;
    EXPECT_EQ(123, out->Int64Value(&lossless));
    EXPECT_TRUE(lossless);
  }

  {
    auto in_value = MakeValueFromScript(context, "make = () => 123");
    v8::Local<v8::BigInt> out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test3", {}, in_value, out);
    EXPECT_FALSE(res.is_success());
    EXPECT_EQ("undefined:0 Uncaught TypeError: Cannot convert 123 to a BigInt.",
              res.ConvertToErrorString(v8_helper_->isolate()));
  }
}

TEST_F(WebIDLCompatTest, StandaloneLong) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  {
    auto in_value = MakeValueFromScript(context, "make = () => -123");
    int32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test1", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(-123, out);
  }

  {
    // Rules for handling signs.
    auto in_value = MakeValueFromScript(context, "make = () => 3e9");
    int32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test2", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(-1294967296, out);
  }

  {
    // Rules for taking modulo.
    auto in_value = MakeValueFromScript(context, "make = () => 5e9");
    int32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test3", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(705032704, out);
  }

  {
    // Can round.
    auto in_value = MakeValueFromScript(context, "make = () => 3.14");
    int32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test4", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(3, out);
  }

  {
    // Rounding is towards zero.
    auto in_value = MakeValueFromScript(context, "make = () => -3.14");
    int32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test5", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(-3, out);
  }

  {
    // This can fail.
    auto in_value = MakeValueFromScript(context, "make = () => BigInt(123)");
    int32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test6", {}, in_value, out);
    EXPECT_FALSE(res.is_success());
    EXPECT_EQ(
        "undefined:0 Uncaught TypeError: Cannot convert a BigInt value to a "
        "number.",
        res.ConvertToErrorString(v8_helper_->isolate()));
  }
}

TEST_F(WebIDLCompatTest, StandaloneUnsignedLong) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  {
    auto in_value = MakeValueFromScript(context, "make = () => -1");
    uint32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test1", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(0xFFFFFFFFu, out);
  }

  {
    // Rules for handling signs.
    auto in_value = MakeValueFromScript(context, "make = () => 3e9");
    uint32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test2", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(3000000000u, out);
  }

  {
    // Rules for taking modulo.
    auto in_value = MakeValueFromScript(context, "make = () => 5e9");
    uint32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test3", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(705032704u, out);
  }

  {
    // Can round.
    auto in_value = MakeValueFromScript(context, "make = () => 3.14");
    uint32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test4", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(3u, out);
  }

  {
    // Rounding is towards zero.
    auto in_value = MakeValueFromScript(context, "make = () => -1.14");
    uint32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test5", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(0xFFFFFFFFu, out);
  }

  {
    // This can fail.
    auto in_value = MakeValueFromScript(context, "make = () => BigInt(123)");
    uint32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test6", {}, in_value, out);
    EXPECT_FALSE(res.is_success());
    EXPECT_EQ(
        "undefined:0 Uncaught TypeError: Cannot convert a BigInt value to a "
        "number.",
        res.ConvertToErrorString(v8_helper_->isolate()));
  }

  {
    // NaN gets converted to 0.
    auto in_value = MakeValueFromScript(context, "make = () => 0/0");
    uint32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test7", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(0u, out);
  }

  {
    // +inf gets converted to 0.
    auto in_value = MakeValueFromScript(context, "make = () => 1/0");
    uint32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test8", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(0u, out);
  }

  {
    // -inf gets converted to 0.
    auto in_value = MakeValueFromScript(context, "make = () => -1/0");
    uint32_t out;
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test9", {}, in_value, out);
    EXPECT_EQ(res.type(), IdlConvert::Status::Type::kSuccess);
    EXPECT_EQ(0u, out);
  }
}

TEST_F(WebIDLCompatTest, SequenceDetection) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Context::Scope ctx(context);

  {
    // An empty object isn't iterable.
    const char kScript[] = R"(
      make = () => {
        return {};
      }
    )";
    auto in_obj = MakeValueFromScript(context, kScript).As<v8::Object>();
    v8::Local<v8::Object> iterator_factory;

    IdlConvert::Status status = IdlConvert::CheckForSequence(
        isolate, "test1 ", {"v1", "scalar"}, in_obj, iterator_factory);
    // This is just a non-iterable --- so it's success, and `iterator_factory`
    // is kept empty.
    EXPECT_TRUE(status.is_success());
    EXPECT_TRUE(iterator_factory.IsEmpty());
  }

  {
    // Having an explicit null for @@iterator is non-iterable, too.
    const char kScript[] = R"(
      make = () => {
        let o = {};
        o[Symbol.iterator] = null;
        return o;
      }
    )";
    auto in_obj = MakeValueFromScript(context, kScript).As<v8::Object>();
    v8::Local<v8::Object> iterator_factory;

    IdlConvert::Status status = IdlConvert::CheckForSequence(
        isolate, "test2 ", {"v1", "scalar"}, in_obj, iterator_factory);
    // This is just a non-iterable --- so it's success, and `iterator_factory`
    // is kept empty.
    EXPECT_TRUE(status.is_success());
    EXPECT_TRUE(iterator_factory.IsEmpty());
  }

  {
    // If get for @iterator throws an error, however, that's trouble. We can use
    // a proxy object to inject that.
    const char kScript[] = R"(
      make = () => {
        let o = {};
        let handler = {
          get: () => { throw "Nope"; }
        }
        return new Proxy(o, handler);
      }
    )";
    auto in_obj = MakeValueFromScript(context, kScript).As<v8::Object>();
    v8::Local<v8::Object> iterator_factory;

    IdlConvert::Status status = IdlConvert::CheckForSequence(
        isolate, "test3 ", {"v1", "scalar"}, in_obj, iterator_factory);
    EXPECT_FALSE(status.is_success());
    EXPECT_TRUE(iterator_factory.IsEmpty());
    EXPECT_EQ("https://example.org/:5 Uncaught Nope.",
              status.ConvertToErrorString(isolate));
  }

  {
    // A non-object @iterator is an error.
    const char kScript[] = R"(
      make = () => {
        let o = {};
        o[Symbol.iterator] = 123;
        return o;
      }
    )";
    auto in_obj = MakeValueFromScript(context, kScript).As<v8::Object>();
    v8::Local<v8::Object> iterator_factory;

    IdlConvert::Status status = IdlConvert::CheckForSequence(
        isolate, "test3 ", {"v1", "scalar"}, in_obj, iterator_factory);
    EXPECT_FALSE(status.is_success());
    EXPECT_TRUE(iterator_factory.IsEmpty());
    EXPECT_EQ("test3 Trouble iterating over v1scalar.",
              status.ConvertToErrorString(isolate));
  }

  {
    // A non-callable @iterator is also an error.
    const char kScript[] = R"(
      make = () => {
        let o = {};
        o[Symbol.iterator] = {};
        return o;
      }
    )";
    auto in_obj = MakeValueFromScript(context, kScript).As<v8::Object>();
    v8::Local<v8::Object> iterator_factory;

    IdlConvert::Status status = IdlConvert::CheckForSequence(
        isolate, "test4 ", {"v1", "scalar"}, in_obj, iterator_factory);
    EXPECT_FALSE(status.is_success());
    EXPECT_TRUE(iterator_factory.IsEmpty());
    EXPECT_EQ("test4 Trouble iterating over v1scalar.",
              status.ConvertToErrorString(isolate));
  }

  {
    // As far as CheckForSequence, any function for @@iterator is good enough;
    // the actual iteration will fail, but it's precise enough to resolve the
    // union properly.
    const char kScript[] = R"(
      make = () => {
        let o = {};
        o[Symbol.iterator] = function() {};
        return o;
      }
    )";
    auto in_obj = MakeValueFromScript(context, kScript).As<v8::Object>();
    v8::Local<v8::Object> iterator_factory;

    IdlConvert::Status status = IdlConvert::CheckForSequence(
        isolate, "test5 ", {"v1", "scalar"}, in_obj, iterator_factory);

    EXPECT_TRUE(status.is_success());
    EXPECT_FALSE(iterator_factory.IsEmpty());
    EXPECT_TRUE(iterator_factory->IsCallable());
  }
}

TEST_F(WebIDLCompatTest, StandaloneSequence) {
  // Sequences are tested more thoroughly as parts of dictionaries for historic
  // reasons.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Context::Scope ctx(context);

  {
    auto in_value = MakeValueFromScript(context, "make = () => [1, 2, 3]");
    std::vector<double> out;
    ASSERT_TRUE(in_value->IsObject());
    v8::Local<v8::Object> in_obj = in_value.As<v8::Object>();
    v8::Local<v8::Object> iterator_factory;

    IdlConvert::Status status = IdlConvert::CheckForSequence(
        v8_helper_->isolate(), "test1 ", {"v1", "scalar"}, in_obj,
        iterator_factory);
    ASSERT_TRUE(status.is_success());
    ASSERT_FALSE(iterator_factory.IsEmpty());

    status = IdlConvert::ConvertSequence(
        v8_helper_.get(), "test1 ", {"v1", "scalar"}, in_obj, iterator_factory,
        base::BindLambdaForTesting(
            [&](v8::Local<v8::Value> in) -> IdlConvert::Status {
              double result = -1;
              IdlConvert::Status status = IdlConvert::Convert(
                  isolate, "inner ", {"sequence item"}, in, result);
              out.push_back(result);
              return status;
            }));
    EXPECT_TRUE(status.is_success());
    ASSERT_EQ(3u, out.size());
    EXPECT_THAT(out, ElementsAre(1.0, 2.0, 3.0));
  }

  {
    // Sequence where conversion fails in the middle.
    auto in_value =
        MakeValueFromScript(context, "make = () => [1, 0.0/0.0, 3]");
    std::vector<double> out;
    ASSERT_TRUE(in_value->IsObject());
    v8::Local<v8::Object> in_obj = in_value.As<v8::Object>();
    v8::Local<v8::Object> iterator_factory;

    IdlConvert::Status status = IdlConvert::CheckForSequence(
        v8_helper_->isolate(), "test2 ", {"v1", "scalar"}, in_obj,
        iterator_factory);
    ASSERT_TRUE(status.is_success());
    ASSERT_FALSE(iterator_factory.IsEmpty());

    status = IdlConvert::ConvertSequence(
        v8_helper_.get(), "test2 ", {"v1", "scalar"}, in_obj, iterator_factory,
        base::BindLambdaForTesting(
            [&](v8::Local<v8::Value> in) -> IdlConvert::Status {
              double result = -1;
              IdlConvert::Status status = IdlConvert::Convert(
                  isolate, "inner2 ", {"a sequence item"}, in, result);
              if (status.is_success()) {
                out.push_back(result);
              }
              return status;
            }));
    EXPECT_FALSE(status.is_success());
    EXPECT_EQ(
        "inner2 Converting a sequence item to a Number did not produce a "
        "finite double.",
        status.ConvertToErrorString(isolate));
    ASSERT_EQ(1u, out.size());
    EXPECT_THAT(out, ElementsAre(1.0));
  }
}

TEST_F(WebIDLCompatTest, BigIntOrLong) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  {
    // Number goes towards long.
    absl::variant<int32_t, v8::Local<v8::BigInt>> out;
    auto in_value = MakeValueFromScript(context, "make = () => -123");
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test1", {}, in_value, out);
    EXPECT_TRUE(res.is_success());
    ASSERT_TRUE(absl::holds_alternative<int32_t>(out));
    EXPECT_EQ(-123, absl::get<int32_t>(out));
  }

  {
    // BigInt goes towards bigint.
    absl::variant<int32_t, v8::Local<v8::BigInt>> out;
    auto in_value = MakeValueFromScript(context, "make = () => BigInt(-123)");
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test2", {}, in_value, out);
    EXPECT_TRUE(res.is_success());
    ASSERT_TRUE(absl::holds_alternative<v8::Local<v8::BigInt>>(out));
    v8::Local<v8::BigInt> bigint_out = absl::get<v8::Local<v8::BigInt>>(out);
    bool lossless = false;
    ASSERT_FALSE(bigint_out.IsEmpty());
    EXPECT_EQ(-123, bigint_out->Int64Value(&lossless));
    EXPECT_TRUE(lossless);
  }

  {
    // Other things may need conversions.
    absl::variant<int32_t, v8::Local<v8::BigInt>> out;
    auto in_value = MakeValueFromScript(context, R"(
      make = () => {
        return {
          valueOf: () => { throw "Surprise!" }
        }
      }
    )");
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test3", {}, in_value, out);
    EXPECT_FALSE(res.is_success());
    EXPECT_EQ("https://example.org/:4 Uncaught Surprise!.",
              res.ConvertToErrorString(v8_helper_->isolate()));
  }

  {
    // Conversion produces BigInt.
    absl::variant<int32_t, v8::Local<v8::BigInt>> out;
    auto in_value = MakeValueFromScript(context, R"(
      make = () => {
        return {
          valueOf: () => BigInt(456)
        }
      }
    )");
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test4", {}, in_value, out);
    EXPECT_TRUE(res.is_success());
    ASSERT_TRUE(absl::holds_alternative<v8::Local<v8::BigInt>>(out));
    v8::Local<v8::BigInt> bigint_out = absl::get<v8::Local<v8::BigInt>>(out);
    bool lossless = false;
    ASSERT_FALSE(bigint_out.IsEmpty());
    EXPECT_EQ(456, bigint_out->Int64Value(&lossless));
    EXPECT_TRUE(lossless);
  }

  {
    // Conversion produces a bool --- that goes towards the number branch.
    absl::variant<int32_t, v8::Local<v8::BigInt>> out;
    auto in_value = MakeValueFromScript(context, R"(
      make = () => {
        return {
          valueOf: () => true
        }
      }
    )");
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test5", {}, in_value, out);
    EXPECT_TRUE(res.is_success());
    ASSERT_TRUE(absl::holds_alternative<int32_t>(out));
    EXPECT_EQ(1, absl::get<int32_t>(out));
  }

  {
    // Conversion produces a string that converts to a number.
    absl::variant<int32_t, v8::Local<v8::BigInt>> out;
    auto in_value = MakeValueFromScript(context, R"(
      make = () => {
        return {
          valueOf: () => "789"
        }
      }
    )");
    auto res =
        IdlConvert::Convert(v8_helper_->isolate(), "test6", {}, in_value, out);
    EXPECT_TRUE(res.is_success());
    ASSERT_TRUE(absl::holds_alternative<int32_t>(out));
    EXPECT_EQ(789, absl::get<int32_t>(out));
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

TEST_F(WebIDLCompatTest, PropagateErrorsToV8Success) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);
  v8::TryCatch try_catch(v8_helper_->isolate());
  IdlConvert::Status::MakeSuccess().PropagateErrorsToV8(v8_helper_.get());
  EXPECT_FALSE(try_catch.HasCaught());
  EXPECT_FALSE(try_catch.HasTerminated());
}

TEST_F(WebIDLCompatTest, PropagateErrorsToV8Timeout) {
  // Testing timeouts is tricky --- PropagateErrorsToV8 doesn't synthesize the
  // timeout, it merely preserves it, so we need to actually trigger a timeout
  // to test it, and further we need to be in a nested context for it to be
  // noticeable.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);
  SetBinding(context);
  binding_callback_ = base::BindLambdaForTesting(
      [&](const v8::FunctionCallbackInfo<v8::Value>& args) {
        std::string out_unchecked;
        AuctionV8Helper::TimeLimitScope time_limit_scope(
            v8_helper_->GetTimeLimit());
        auto status = IdlConvert::Convert(
            v8_helper_->isolate(), "ctx:", {"arg 0"}, args[0], out_unchecked);
        EXPECT_EQ(status.type(), IdlConvert::Status::Type::kTimeout);
        status.PropagateErrorsToV8(v8_helper_.get());
      });

  const char kScript[] = R"(
    try {
      binding({
          toString: () => { while(true) {} }
        }
      );
    } catch(e) {}
  )";

  std::vector<std::string> errors = RunScript(
      context, kScript, /*expect_result=*/AuctionV8Helper::Result::kTimeout);
  EXPECT_THAT(
      errors,
      ElementsAre("https://example.org/ top-level execution timed out."));
}

TEST_F(WebIDLCompatTest, PropagateErrorsToV8ErrorMessage) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);
  v8::TryCatch try_catch(v8_helper_->isolate());
  IdlConvert::Status::MakeErrorMessage("Bad bug.")
      .PropagateErrorsToV8(v8_helper_.get());
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_FALSE(try_catch.HasTerminated());
  EXPECT_EQ(
      "undefined:0 Uncaught TypeError: Bad bug.",
      AuctionV8Helper::FormatExceptionMessage(context, try_catch.Message()));
}

TEST_F(WebIDLCompatTest, PropagateErrorsToV8Exception) {
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::Value> exception = v8::Exception::SyntaxError(
      v8_helper_->CreateUtf8String("typo").ToLocalChecked());
  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate, exception);
  auto status = IdlConvert::Status::MakeException(exception, message);
  status.PropagateErrorsToV8(v8_helper_.get());
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_FALSE(try_catch.HasTerminated());
  EXPECT_EQ(
      "undefined:0 Uncaught SyntaxError: typo.",
      AuctionV8Helper::FormatExceptionMessage(context, try_catch.Message()));
  EXPECT_EQ("undefined:0 Uncaught SyntaxError: typo.",
            status.ConvertToErrorString(isolate));
}

TEST_F(WebIDLCompatTest, RecordBasic) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      return {e:1, 2:"b", c: undefined, 3: {}}
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  EXPECT_TRUE(res.is_success());
  // Array index keys go first, and then others in insertion order.
  EXPECT_THAT(out, ElementsAre(Pair("2", "b"), Pair("3", "[object Object]"),
                               Pair("e", "1"), Pair("c", "undefined")));
}

TEST_F(WebIDLCompatTest, RecordArray) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      return ['a', 4, 'b'];
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  EXPECT_TRUE(res.is_success());
  // Array index keys go first, and then others in insertion order.
  EXPECT_THAT(out, ElementsAre(Pair("0", "a"), Pair("1", "4"), Pair("2", "b")));
}

TEST_F(WebIDLCompatTest, RecordNonObject) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, "make = () => 42");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1 ",
                           {"'a'"}, in_value, out);
  ASSERT_FALSE(res.is_success());
  EXPECT_EQ("test1 Cannot convert 'a' to a record since it's not an Object.",
            res.ConvertToErrorString(v8_helper_->isolate()));
}

TEST_F(WebIDLCompatTest, RecordGetOwnPropertyNamesFailure) {
  // GetOwnPropertyNames only fails in case of a proxy object.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      let o = { a: 1, b: 2};
      let handler = {
          ownKeys(target) {
            return ["a", "a"];
          }
      }
      return new Proxy(o, handler);
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  ASSERT_FALSE(res.is_success());
  EXPECT_EQ(
      "undefined:0 Uncaught TypeError: 'ownKeys' on proxy: trap returned "
      "duplicate entries.",
      res.ConvertToErrorString(v8_helper_->isolate()));
}

TEST_F(WebIDLCompatTest, RecordGetFieldFailure) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      let o = {
        a: 1,
        b: 2,
        get c() { throw "No C for you!"; }
      };
      return o;
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  ASSERT_FALSE(res.is_success());
  EXPECT_EQ("https://example.org/:6 Uncaught No C for you!.",
            res.ConvertToErrorString(v8_helper_->isolate()));
}

TEST_F(WebIDLCompatTest, RecordGetOwnPropertyDescriptorFailure) {
  // Proxies are an easy way of injecting failures in GetOwnPropertyDescriptor.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      let o = { a: 1, b: 2};
      let handler = {
          getOwnPropertyDescriptor(target, prop) {
            return 50;
          }
      }
      return new Proxy(o, handler);
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  ASSERT_FALSE(res.is_success());
  EXPECT_EQ(
      "undefined:0 Uncaught TypeError: 'getOwnPropertyDescriptor' on proxy: "
      "trap returned neither object nor undefined for property 'a'.",
      res.ConvertToErrorString(v8_helper_->isolate()));
}

TEST_F(WebIDLCompatTest, RecordSkips) {
  // Skip things with undefined descriptors or non-enumerable properties.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      let o = { a: 1, b: 2, c: 3, d: 4, e: 5};
      let handler = {
          getOwnPropertyDescriptor(target, prop) {
            if (prop === 'b')
              return undefined;
            let desc = Reflect.getOwnPropertyDescriptor(target, prop);
            if (prop == 'd')
              desc.enumerable = false;
            return desc;
          }
      }
      return new Proxy(o, handler);
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  EXPECT_TRUE(res.is_success());
  EXPECT_THAT(out, ElementsAre(Pair("a", "1"), Pair("c", "3"), Pair("e", "5")));
}

TEST_F(WebIDLCompatTest, RecordKeyConvertFailure) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      let o = { a: 1, b: 2};
      o[Symbol('c')] = 3;
      return o;
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  ASSERT_FALSE(res.is_success());
  EXPECT_EQ(
      "undefined:0 Uncaught TypeError: Cannot convert a Symbol value to a "
      "string.",
      res.ConvertToErrorString(v8_helper_->isolate()));
}

TEST_F(WebIDLCompatTest, RecordKeyConvertFailureOrder) {
  // Make sure that we do key conversion before the Get.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      let o = {};
      Object.defineProperty(o, Symbol('c'), {
        enumerable: true,
        get: () => { throw "get failure"; }
      })
      return o;
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  ASSERT_FALSE(res.is_success());
  EXPECT_EQ(
      "undefined:0 Uncaught TypeError: Cannot convert a Symbol value to a "
      "string.",
      res.ConvertToErrorString(v8_helper_->isolate()));
}

TEST_F(WebIDLCompatTest, RecordValConvertFailure) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      let o = { a: 1, b: 2, c: Symbol(3)};
      return o;
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  ASSERT_FALSE(res.is_success());
  EXPECT_EQ(
      "undefined:0 Uncaught TypeError: Cannot convert a Symbol value to a "
      "string.",
      res.ConvertToErrorString(v8_helper_->isolate()));
}

TEST_F(WebIDLCompatTest, RecordGetFailue) {
  // Make sure that we do key conversion before the Get.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      let o = {};
      Object.defineProperty(o, 'a', {
        enumerable: true,
        get: () => { throw "get failure"; }
      })
      return o;
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  ASSERT_FALSE(res.is_success());
  EXPECT_EQ("https://example.org/:6 Uncaught get failure.",
            res.ConvertToErrorString(v8_helper_->isolate()));
}

TEST_F(WebIDLCompatTest, RecordValidUTF16) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      return {'\ud835\udd39' : '\ud835\udca9'}
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  EXPECT_TRUE(res.is_success());
  EXPECT_THAT(out, ElementsAre(Pair("\xf0\x9d\x94\xb9", "\xf0\x9d\x92\xa9")));
}

TEST_F(WebIDLCompatTest, RecordInvalidUTF16Key) {
  // We decode keys as DOMString, so they should pass in mis-matched surrogates
  // as-is.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      return {'\ud835' : 'OK'}
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  EXPECT_TRUE(res.is_success());
  EXPECT_THAT(out, ElementsAre(Pair("\xED\xA0\xB5", "OK")));
}

TEST_F(WebIDLCompatTest, RecordInvalidUTF16Val) {
  // We decode values as USVString, so they should replace mis-matched
  // surrogates with replacement characters
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  auto in_value = MakeValueFromScript(context, R"(
    make = () => {
      return {'key' : '<<\ud835>>'}
    }
  )");
  std::vector<std::pair<std::string, std::string>> out;
  auto res = ConvertRecord(v8_helper_.get(), *time_limit_scope_, "test1", {"a"},
                           in_value, out);
  EXPECT_TRUE(res.is_success());
  EXPECT_THAT(out, ElementsAre(Pair("key", "<<\xEF\xBF\xBD>>")));
}

// WebIDL treats undefined as empty dictionary.
TEST_F(WebIDLCompatTest, UndefinedEmptyDict) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);

  const char kScript[] = R"(
    function make() { return undefined; }
  )";

  auto converter = MakeFromScript(context, kScript);

  std::optional<std::string> out;
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

  std::optional<std::string> out;
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

  std::optional<std::string> out;
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

  std::optional<std::string> out;
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

  std::optional<std::string> out;
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

  std::optional<std::string> out;
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
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
      base::BindLambdaForTesting(
          [&](v8::Local<v8::Value> item) -> IdlConvert::Status {
            saw_a_item = true;
            return IdlConvert::Status::MakeErrorMessage("Badness");
          })));
  EXPECT_TRUE(saw_a);
  EXPECT_FALSE(saw_a_item);

  bool saw_b = false;
  bool saw_b_item = false;

  EXPECT_TRUE(converter->GetOptionalSequence(
      "b", base::BindLambdaForTesting([&]() { saw_b = true; }),
      base::BindLambdaForTesting(
          [&](v8::Local<v8::Value> item) -> IdlConvert::Status {
            saw_b_item = true;
            return IdlConvert::Status::MakeErrorMessage("Badness");
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());

  EXPECT_FALSE(converter->GetOptionalSequence(
      "f1", base::BindLambdaForTesting([&]() { saw_field = true; }),
      base::BindLambdaForTesting(
          [&](v8::Local<v8::Value> item) -> IdlConvert::Status {
            std::string str;
            EXPECT_TRUE(gin::Converter<std::string>::FromV8(
                v8_helper_->isolate(), item, &str));
            if (str == "error") {
              return IdlConvert::Status::MakeErrorMessage("Helpful error");
            }
            out.push_back(item);
            return IdlConvert::Status::MakeSuccess();
          })));
  ExpectStringList({"a", "b"}, out);
  EXPECT_EQ("Helpful error", converter->ErrorMessage());
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
      base::BindLambdaForTesting(
          [&](v8::Local<v8::Value> item) -> IdlConvert::Status {
            DictConverter inner(v8_helper_.get(), *time_limit_scope_,
                                "'f1' entry: ", item);
            double entry;
            bool ok = inner.GetRequired("f", entry);
            if (ok) {
              out.push_back(entry);
              return IdlConvert::Status::MakeSuccess();
            } else {
              return inner.TakeStatus();
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
      base::BindLambdaForTesting(
          [&](v8::Local<v8::Value> item) -> IdlConvert::Status {
            DictConverter inner(v8_helper_.get(), *time_limit_scope_,
                                "'f1' entry: ", item);
            double entry;
            bool ok = inner.GetRequired("f", entry);
            if (ok) {
              out.push_back(entry);
              return IdlConvert::Status::MakeSuccess();
            } else {
              return inner.TakeStatus();
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ(
      "<error prefix> Trouble iterating over field 'a' as it does not appear "
      "to be a sequence.",
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("<error prefix> Trouble iterating over field 'a'.",
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("<error prefix> Trouble iterating over field 'a'.",
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("<error prefix> Trouble iterating over field 'a'.",
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
  EXPECT_FALSE(GetSequence(converter.get(), "a", out));
  EXPECT_EQ("<error prefix> Timeout iterating over field 'a'.",
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
  v8::LocalVector<v8::Value> out(v8_helper_->isolate());
  EXPECT_TRUE(GetSequence(converter.get(), "a", out));
  ASSERT_EQ(out.size(), 4u);
  for (const auto& entry : out) {
    EXPECT_TRUE(entry->IsUndefined());
  }
}

TEST_F(WebIDLCompatTest, ArgsConverter) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);
  SetBindingForArgConverterTest(context);
  const char kTest[] = R"(
    binding();
  )";
  std::vector<std::string> errors = RunScript(
      context, kTest, /*expect_result=*/AuctionV8Helper::Result::kFailure);
  EXPECT_THAT(errors,
              ElementsAre("https://example.org/:2 Uncaught TypeError: "
                          "binding(): at least 2 argument(s) are required."));
}

TEST_F(WebIDLCompatTest, ArgsConverter2) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);
  SetBindingForArgConverterTest(context);
  const char kTest[] = R"(
    binding("hi");
  )";
  std::vector<std::string> errors = RunScript(
      context, kTest, /*expect_result=*/AuctionV8Helper::Result::kFailure);
  EXPECT_THAT(errors,
              ElementsAre("https://example.org/:2 Uncaught TypeError: "
                          "binding(): at least 2 argument(s) are required."));
}

TEST_F(WebIDLCompatTest, ArgsConverter3) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);
  SetBindingForArgConverterTest(context);
  const char kTest[] = R"(
    let notS = {
      toString: () => { return {}; }
    }
    binding(notS, 0/0);
  )";
  std::vector<std::string> errors = RunScript(
      context, kTest, /*expect_result=*/AuctionV8Helper::Result::kFailure);
  EXPECT_THAT(errors, ElementsAre("https://example.org/:5 Uncaught TypeError: "
                                  "Cannot convert object to primitive value."));
}

TEST_F(WebIDLCompatTest, ArgsConverter4) {
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);
  SetBindingForArgConverterTest(context);
  const char kTest[] = R"(
      binding("hi", 0/0);
  )";
  std::vector<std::string> errors = RunScript(
      context, kTest, /*expect_result=*/AuctionV8Helper::Result::kFailure);
  EXPECT_THAT(
      errors,
      ElementsAre(
          "https://example.org/:2 Uncaught TypeError: binding(): Converting "
          "argument 'arg1' to a Number did not produce a finite double."));
}

TEST_F(WebIDLCompatTest, ArgsConverter5) {
  // A successful call.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);
  SetBindingForArgConverterTest(context);
  const char kTest[] = R"(
      binding("hi", 10);
  )";
  std::vector<std::string> errors = RunScript(
      context, kTest, /*expect_result=*/AuctionV8Helper::Result::kSuccess);
  EXPECT_THAT(errors, ElementsAre());
  EXPECT_EQ(arg0_, "hi");
  EXPECT_EQ(arg1_, 10.0);
}

TEST_F(WebIDLCompatTest, ArgsConverter6) {
  // A successful call with some coercions.
  v8::Local<v8::Context> context = v8_helper_->CreateContext();
  v8::Context::Scope ctx(context);
  SetBindingForArgConverterTest(context);
  const char kTest[] = R"(
      binding(23, "12");
  )";
  std::vector<std::string> errors = RunScript(
      context, kTest, /*expect_result=*/AuctionV8Helper::Result::kSuccess);
  EXPECT_THAT(errors, ElementsAre());
  EXPECT_EQ(arg0_, "23");
  EXPECT_EQ(arg1_, 12.0);
}

}  // namespace auction_worklet
