// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle.h"
#include "third_party/blink/renderer/core/mojo/tests/JsToCpp.mojom-blink.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/loader/fetch/access_control_status.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace {

// Global value updated by some checks to prevent compilers from optimizing
// reads out of existence.
uint32_t g_waste_accumulator = 0;

// Negative numbers with different values in each byte, the last of
// which can survive promotion to double and back.
const int8_t kExpectedInt8Value = -65;
const int16_t kExpectedInt16Value = -16961;
const int32_t kExpectedInt32Value = -1145258561;
const int64_t kExpectedInt64Value = -77263311946305LL;

// Positive numbers with different values in each byte, the last of
// which can survive promotion to double and back.
const uint8_t kExpectedUInt8Value = 65;
const uint16_t kExpectedUInt16Value = 16961;
const uint32_t kExpectedUInt32Value = 1145258561;
const uint64_t kExpectedUInt64Value = 77263311946305LL;

// Double/float values, including special case constants.
const double kExpectedDoubleVal = 3.14159265358979323846;
const double kExpectedDoubleInf = std::numeric_limits<double>::infinity();
const double kExpectedDoubleNan = std::numeric_limits<double>::quiet_NaN();
const float kExpectedFloatVal = static_cast<float>(kExpectedDoubleVal);
const float kExpectedFloatInf = std::numeric_limits<float>::infinity();
const float kExpectedFloatNan = std::numeric_limits<float>::quiet_NaN();

// NaN has the property that it is not equal to itself.
#define EXPECT_NAN(x) EXPECT_NE(x, x)

String MojoBindingsScriptPath() {
  String filepath = test::ExecutableDir();
  filepath.append("/gen/mojo/public/js/mojo_bindings.js");
  return filepath;
}

String TestBindingsScriptPath() {
  String filepath = test::ExecutableDir();
  filepath.append(
      "/gen/third_party/blink/renderer/core/mojo/tests/JsToCpp.mojom.js");
  return filepath;
}

String TestScriptPath() {
  String filepath = test::BlinkRootDir();
  filepath.append("/renderer/core/mojo/tests/JsToCppTest.js");
  return filepath;
}

v8::Local<v8::Value> ExecuteScript(const String& script_path,
                                   LocalFrame& frame) {
  scoped_refptr<SharedBuffer> script_src = test::ReadFromFile(script_path);
  return frame.GetScriptController().ExecuteScriptInMainWorldAndReturnValue(
      ScriptSourceCode(String(script_src->Data(), script_src->size())), KURL(),
      kOpaqueResource);
}

void CheckDataPipe(mojo::DataPipeConsumerHandle data_pipe_handle) {
  MojoResult result = Wait(data_pipe_handle, MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, result);

  const void* buffer = nullptr;
  unsigned num_bytes = 0;
  result = data_pipe_handle.BeginReadData(&buffer, &num_bytes,
                                          MOJO_READ_DATA_FLAG_NONE);
  EXPECT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(64u, num_bytes);
  for (unsigned i = 0; i < num_bytes; ++i) {
    EXPECT_EQ(i, static_cast<unsigned>(static_cast<const char*>(buffer)[i]));
  }
  data_pipe_handle.EndReadData(num_bytes);
}

void CheckMessagePipe(mojo::MessagePipeHandle message_pipe_handle) {
  MojoResult result = Wait(message_pipe_handle, MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK, result);

  std::vector<uint8_t> bytes;
  std::vector<mojo::ScopedHandle> handles;
  result = ReadMessageRaw(message_pipe_handle, &bytes, &handles, 0);
  EXPECT_EQ(MOJO_RESULT_OK, result);
  EXPECT_EQ(64u, bytes.size());
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(255 - i, bytes[i]);
  }
}

js_to_cpp::blink::EchoArgsPtr BuildSampleEchoArgs() {
  auto args = js_to_cpp::blink::EchoArgs::New();
  args->si64 = kExpectedInt64Value;
  args->si32 = kExpectedInt32Value;
  args->si16 = kExpectedInt16Value;
  args->si8 = kExpectedInt8Value;
  args->ui64 = kExpectedUInt64Value;
  args->ui32 = kExpectedUInt32Value;
  args->ui16 = kExpectedUInt16Value;
  args->ui8 = kExpectedUInt8Value;
  args->float_val = kExpectedFloatVal;
  args->float_inf = kExpectedFloatInf;
  args->float_nan = kExpectedFloatNan;
  args->double_val = kExpectedDoubleVal;
  args->double_inf = kExpectedDoubleInf;
  args->double_nan = kExpectedDoubleNan;
  args->name = "coming";
  args->string_array.emplace(3);
  (*args->string_array)[0] = "one";
  (*args->string_array)[1] = "two";
  (*args->string_array)[2] = "three";
  return args;
}

void CheckSampleEchoArgs(const js_to_cpp::blink::EchoArgsPtr& arg) {
  EXPECT_EQ(kExpectedInt64Value, arg->si64);
  EXPECT_EQ(kExpectedInt32Value, arg->si32);
  EXPECT_EQ(kExpectedInt16Value, arg->si16);
  EXPECT_EQ(kExpectedInt8Value, arg->si8);
  EXPECT_EQ(kExpectedUInt64Value, arg->ui64);
  EXPECT_EQ(kExpectedUInt32Value, arg->ui32);
  EXPECT_EQ(kExpectedUInt16Value, arg->ui16);
  EXPECT_EQ(kExpectedUInt8Value, arg->ui8);
  EXPECT_EQ(kExpectedFloatVal, arg->float_val);
  EXPECT_EQ(kExpectedFloatInf, arg->float_inf);
  EXPECT_NAN(arg->float_nan);
  EXPECT_EQ(kExpectedDoubleVal, arg->double_val);
  EXPECT_EQ(kExpectedDoubleInf, arg->double_inf);
  EXPECT_NAN(arg->double_nan);
  EXPECT_EQ(String("coming"), arg->name);
  EXPECT_EQ(String("one"), (*arg->string_array)[0]);
  EXPECT_EQ(String("two"), (*arg->string_array)[1]);
  EXPECT_EQ(String("three"), (*arg->string_array)[2]);
  CheckDataPipe(arg->data_handle.get());
  CheckMessagePipe(arg->message_handle.get());
}

void CheckSampleEchoArgsList(const js_to_cpp::blink::EchoArgsListPtr& list) {
  if (list.is_null())
    return;
  CheckSampleEchoArgs(list->item);
  CheckSampleEchoArgsList(list->next);
}

// More forgiving checks are needed in the face of potentially corrupt
// messages. The values don't matter so long as all accesses are within
// bounds.
void CheckCorruptedString(const String& arg) {
  for (wtf_size_t i = 0; i < arg.length(); ++i)
    g_waste_accumulator += arg[i];
}

void CheckCorruptedStringArray(
    const base::Optional<Vector<String>>& string_array) {
  if (!string_array)
    return;
  for (const String& element : *string_array)
    CheckCorruptedString(element);
}

void CheckCorruptedDataPipe(mojo::DataPipeConsumerHandle data_pipe_handle) {
  unsigned char buffer[100];
  uint32_t buffer_size = static_cast<uint32_t>(sizeof(buffer));
  MojoResult result =
      data_pipe_handle.ReadData(buffer, &buffer_size, MOJO_READ_DATA_FLAG_NONE);
  if (result != MOJO_RESULT_OK)
    return;
  for (uint32_t i = 0; i < buffer_size; ++i)
    g_waste_accumulator += buffer[i];
}

void CheckCorruptedMessagePipe(mojo::MessagePipeHandle message_pipe_handle) {
  std::vector<uint8_t> bytes;
  std::vector<mojo::ScopedHandle> handles;
  MojoResult result = ReadMessageRaw(message_pipe_handle, &bytes, &handles, 0);
  if (result != MOJO_RESULT_OK)
    return;
  for (uint32_t i = 0; i < bytes.size(); ++i)
    g_waste_accumulator += bytes[i];
}

void CheckCorruptedEchoArgs(const js_to_cpp::blink::EchoArgsPtr& arg) {
  if (arg.is_null())
    return;
  CheckCorruptedString(arg->name);
  CheckCorruptedStringArray(arg->string_array);
  if (arg->data_handle.is_valid())
    CheckCorruptedDataPipe(arg->data_handle.get());
  if (arg->message_handle.is_valid())
    CheckCorruptedMessagePipe(arg->message_handle.get());
}

void CheckCorruptedEchoArgsList(const js_to_cpp::blink::EchoArgsListPtr& list) {
  if (list.is_null())
    return;
  CheckCorruptedEchoArgs(list->item);
  CheckCorruptedEchoArgsList(list->next);
}

// Base Provider implementation class. It's expected that tests subclass and
// override the appropriate Provider functions. When test is done quit the
// run_loop().
class CppSideConnection : public js_to_cpp::blink::CppSide {
 public:
  CppSideConnection() : mishandled_messages_(0), binding_(this) {}
  ~CppSideConnection() override = default;

  void set_js_side(js_to_cpp::blink::JsSidePtr js_side) {
    js_side_ = std::move(js_side);
  }
  js_to_cpp::blink::JsSide* js_side() { return js_side_.get(); }

  void Bind(mojo::InterfaceRequest<js_to_cpp::blink::CppSide> request) {
    binding_.Bind(std::move(request));
    // Keep the pipe open even after validation errors.
    binding_.EnableTestingMode();
  }

  // js_to_cpp::CppSide:
  void StartTest() override { NOTREACHED(); }

  void TestFinished() override { NOTREACHED(); }

  void PingResponse() override { mishandled_messages_ += 1; }

  void EchoResponse(js_to_cpp::blink::EchoArgsListPtr list) override {
    mishandled_messages_ += 1;
  }

  void BitFlipResponse(
      js_to_cpp::blink::EchoArgsListPtr list,
      js_to_cpp::blink::ForTestingAssociatedPtrInfo not_used) override {
    mishandled_messages_ += 1;
  }

  void BackPointerResponse(js_to_cpp::blink::EchoArgsListPtr list) override {
    mishandled_messages_ += 1;
  }

 protected:
  js_to_cpp::blink::JsSidePtr js_side_;
  int mishandled_messages_;
  mojo::Binding<js_to_cpp::blink::CppSide> binding_;
};

// Trivial test to verify a message sent from JS is received.
class PingCppSideConnection : public CppSideConnection {
 public:
  PingCppSideConnection() : got_message_(false) {}
  ~PingCppSideConnection() override = default;

  // js_to_cpp::CppSide:
  void StartTest() override { js_side_->Ping(); }

  void PingResponse() override {
    got_message_ = true;
    test::ExitRunLoop();
  }

  bool DidSucceed() { return got_message_ && !mishandled_messages_; }

 private:
  bool got_message_;
};

// Test that parameters are passed with correct values.
class EchoCppSideConnection : public CppSideConnection {
 public:
  EchoCppSideConnection() : message_count_(0), termination_seen_(false) {}
  ~EchoCppSideConnection() override = default;

  // js_to_cpp::CppSide:
  void StartTest() override {
    js_side_->Echo(kExpectedMessageCount, BuildSampleEchoArgs());
  }

  void EchoResponse(js_to_cpp::blink::EchoArgsListPtr list) override {
    message_count_ += 1;

    const js_to_cpp::blink::EchoArgsPtr& special_arg = list->item;
    EXPECT_EQ(-1, special_arg->si64);
    EXPECT_EQ(-1, special_arg->si32);
    EXPECT_EQ(-1, special_arg->si16);
    EXPECT_EQ(-1, special_arg->si8);
    EXPECT_EQ(String("going"), special_arg->name);
    CheckDataPipe(special_arg->data_handle.get());
    CheckMessagePipe(special_arg->message_handle.get());

    CheckSampleEchoArgsList(list->next);
  }

  void TestFinished() override {
    termination_seen_ = true;
    test::ExitRunLoop();
  }

  bool DidSucceed() {
    return termination_seen_ && !mishandled_messages_ &&
           message_count_ == kExpectedMessageCount;
  }

 private:
  static const int kExpectedMessageCount = 10;
  int message_count_;
  bool termination_seen_;
};

// Test that corrupted messages don't wreak havoc.
class BitFlipCppSideConnection : public CppSideConnection {
 public:
  BitFlipCppSideConnection() : termination_seen_(false) {}
  ~BitFlipCppSideConnection() override = default;

  // js_to_cpp::CppSide:
  void StartTest() override { js_side_->BitFlip(BuildSampleEchoArgs()); }

  void BitFlipResponse(
      js_to_cpp::blink::EchoArgsListPtr list,
      js_to_cpp::blink::ForTestingAssociatedPtrInfo not_used) override {
    CheckCorruptedEchoArgsList(list);
  }

  void TestFinished() override {
    termination_seen_ = true;
    test::ExitRunLoop();
  }

  bool DidSucceed() { return termination_seen_; }

 private:
  bool termination_seen_;
};

// Test that severely random messages don't wreak havoc.
class BackPointerCppSideConnection : public CppSideConnection {
 public:
  BackPointerCppSideConnection() : termination_seen_(false) {}
  ~BackPointerCppSideConnection() override = default;

  // js_to_cpp::CppSide:
  void StartTest() override { js_side_->BackPointer(BuildSampleEchoArgs()); }

  void BackPointerResponse(js_to_cpp::blink::EchoArgsListPtr list) override {
    CheckCorruptedEchoArgsList(list);
  }

  void TestFinished() override {
    termination_seen_ = true;
    test::ExitRunLoop();
  }

  bool DidSucceed() { return termination_seen_; }

 private:
  bool termination_seen_;
};

class JsToCppTest : public testing::Test {
 public:
  void RunTest(CppSideConnection* cpp_side) {
    js_to_cpp::blink::CppSidePtr cpp_side_ptr;
    cpp_side->Bind(MakeRequest(&cpp_side_ptr));

    js_to_cpp::blink::JsSidePtr js_side_ptr;
    auto js_side_request = MakeRequest(&js_side_ptr);
    js_side_ptr->SetCppSide(std::move(cpp_side_ptr));
    cpp_side->set_js_side(std::move(js_side_ptr));

    V8TestingScope scope;
    scope.GetPage().GetSettings().SetScriptEnabled(true);
    ExecuteScript(MojoBindingsScriptPath(), scope.GetFrame());
    ExecuteScript(TestBindingsScriptPath(), scope.GetFrame());

    v8::Local<v8::Value> start_fn =
        ExecuteScript(TestScriptPath(), scope.GetFrame());
    ASSERT_FALSE(start_fn.IsEmpty());
    ASSERT_TRUE(start_fn->IsFunction());
    v8::Local<v8::Object> global_proxy = scope.GetContext()->Global();
    v8::Local<v8::Value> args[1] = {
        ToV8(MojoHandle::Create(
                 mojo::ScopedHandle::From(js_side_request.PassMessagePipe())),
             global_proxy, scope.GetIsolate())};
    V8ScriptRunner::CallFunction(start_fn.As<v8::Function>(),
                                 scope.GetExecutionContext(), global_proxy,
                                 arraysize(args), args, scope.GetIsolate());
    test::EnterRunLoop();
  }
};

TEST_F(JsToCppTest, Ping) {
  PingCppSideConnection cpp_side_connection;
  RunTest(&cpp_side_connection);
  EXPECT_TRUE(cpp_side_connection.DidSucceed());
}

TEST_F(JsToCppTest, Echo) {
  EchoCppSideConnection cpp_side_connection;
  RunTest(&cpp_side_connection);
  EXPECT_TRUE(cpp_side_connection.DidSucceed());
}

TEST_F(JsToCppTest, BitFlip) {
  // These tests generate a lot of expected validation errors. Suppress logging.
  mojo::internal::ScopedSuppressValidationErrorLoggingForTests log_suppression;

  BitFlipCppSideConnection cpp_side_connection;
  RunTest(&cpp_side_connection);
  EXPECT_TRUE(cpp_side_connection.DidSucceed());
}

TEST_F(JsToCppTest, BackPointer) {
  // These tests generate a lot of expected validation errors. Suppress logging.
  mojo::internal::ScopedSuppressValidationErrorLoggingForTests log_suppression;

  BackPointerCppSideConnection cpp_side_connection;
  RunTest(&cpp_side_connection);
  EXPECT_TRUE(cpp_side_connection.DidSucceed());
}

}  // namespace
}  // namespace blink
