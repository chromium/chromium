// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_math.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "mojo/public/c/system/macros.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_dispatcher.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/validation_test_input_parser.h"
#include "mojo/public/cpp/system/message.h"
#include "mojo/public/cpp/test_support/test_support.h"
#include "mojo/public/interfaces/bindings/tests/validation_test_associated_interfaces.mojom.h"
#include "mojo/public/interfaces/bindings/tests/validation_test_interfaces.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

Message CreateRawMessage(size_t size) {
  ScopedMessageHandle handle;
  MojoResult rv = CreateMessage(&handle, MOJO_CREATE_MESSAGE_FLAG_NONE);
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  DCHECK(handle.is_valid());

  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(size));
  MojoAppendMessageDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
  void* buffer;
  uint32_t buffer_size;
  rv = MojoAppendMessageData(handle->value(), static_cast<uint32_t>(size),
                             nullptr, 0, &options, &buffer, &buffer_size);
  DCHECK_EQ(MOJO_RESULT_OK, rv);

  return Message::CreateFromMessageHandle(&handle);
}

template <typename T>
void Append(std::vector<uint8_t>* data_vector, T data) {
  size_t pos = data_vector->size();
  data_vector->resize(pos + sizeof(T));
  memcpy(&(*data_vector)[pos], &data, sizeof(T));
}

bool TestInputParser(const std::string& input,
                     bool expected_result,
                     const std::vector<uint8_t>& expected_data,
                     size_t expected_num_handles) {
  std::vector<uint8_t> data;
  size_t num_handles;
  std::string error_message;

  bool result =
      ParseValidationTestInput(input, &data, &num_handles, &error_message);
  if (expected_result) {
    if (result && error_message.empty() && expected_data == data &&
        expected_num_handles == num_handles) {
      return true;
    }

    // Compare with an empty string instead of checking |error_message.empty()|,
    // so that the message will be printed out if the two are not equal.
    EXPECT_EQ(std::string(), error_message);
    EXPECT_EQ(expected_data, data);
    EXPECT_EQ(expected_num_handles, num_handles);
    return false;
  }

  EXPECT_FALSE(error_message.empty());
  return !result && !error_message.empty();
}

std::vector<std::string> GetMatchingTests(const std::vector<std::string>& names,
                                          const std::string& prefix) {
  const std::string suffix = ".data";
  std::vector<std::string> tests;
  for (size_t i = 0; i < names.size(); ++i) {
    if (names[i].size() >= suffix.size() &&
        names[i].substr(0, prefix.size()) == prefix &&
        names[i].substr(names[i].size() - suffix.size()) == suffix)
      tests.push_back(names[i].substr(0, names[i].size() - suffix.size()));
  }
  return tests;
}

bool ReadFile(const std::string& path, std::string* result) {
  FILE* fp = OpenSourceRootRelativeFile(path.c_str());
  if (!fp) {
    ADD_FAILURE() << "File not found: " << path;
    return false;
  }
  fseek(fp, 0, SEEK_END);
  size_t size = static_cast<size_t>(ftell(fp));
  if (size == 0) {
    result->clear();
    fclose(fp);
    return true;
  }
  fseek(fp, 0, SEEK_SET);
  result->resize(size);
  size_t size_read = fread(&result->at(0), 1, size, fp);
  fclose(fp);
  return size == size_read;
}

bool ReadAndParseDataFile(const std::string& path,
                          std::vector<uint8_t>* data,
                          size_t* num_handles) {
  std::string input;
  if (!ReadFile(path, &input))
    return false;

  std::string error_message;
  if (!ParseValidationTestInput(input, data, num_handles, &error_message)) {
    ADD_FAILURE() << error_message;
    return false;
  }

  return true;
}

bool ReadResultFile(const std::string& path, std::string* result) {
  if (!ReadFile(path, result))
    return false;

  // Result files are new-line delimited text files. Remove any CRs.
  result->erase(std::remove(result->begin(), result->end(), '\r'),
                result->end());

  // Remove trailing LFs.
  size_t pos = result->find_last_not_of('\n');
  if (pos == std::string::npos)
    result->clear();
  else
    result->resize(pos + 1);

  return true;
}

std::string GetPath(const std::string& root, const std::string& suffix) {
  return "mojo/public/interfaces/bindings/tests/data/validation/" + root +
         suffix;
}

// |message| should be a newly created object.
bool ReadTestCase(const std::string& test,
                  Message* message,
                  std::string* expected) {
  std::vector<uint8_t> data;
  size_t num_handles;
  if (!ReadAndParseDataFile(GetPath(test, ".data"), &data, &num_handles) ||
      !ReadResultFile(GetPath(test, ".expected"), expected)) {
    return false;
  }

  *message = CreateRawMessage(data.size());
  if (!data.empty())
    memcpy(message->mutable_data(), &data[0], data.size());
  message->mutable_handles()->resize(num_handles);

  return true;
}

void RunValidationTests(const std::string& prefix,
                        MessageReceiver* test_message_receiver) {
  std::vector<std::string> names =
      EnumerateSourceRootRelativeDirectory(GetPath("", ""));
  std::vector<std::string> tests = GetMatchingTests(names, prefix);
  ASSERT_FALSE(tests.empty());

  for (size_t i = 0; i < tests.size(); ++i) {
    Message message;
    std::string expected;
    ASSERT_TRUE(ReadTestCase(tests[i], &message, &expected));

    std::string result;
    base::RunLoop run_loop;
    mojo::internal::ValidationErrorObserverForTesting observer(
        run_loop.QuitClosure());
    std::ignore = test_message_receiver->Accept(&message);
    if (expected != "PASS")  // Observer only gets called on errors.
      run_loop.Run();
    if (observer.last_error() == mojo::internal::VALIDATION_ERROR_NONE)
      result = "PASS";
    else
      result = mojo::internal::ValidationErrorToString(observer.last_error());

    EXPECT_EQ(expected, result) << "failed test: " << tests[i];
  }
}

class TwoStepValidator : public MessageReceiver {
 public:
  TwoStepValidator(std::unique_ptr<MessageReceiver> first_validator,
                   std::unique_ptr<MessageReceiver> second_validator)
      : first_validator_(std::move(first_validator)),
        second_validator_(std::move(second_validator)) {}

  bool Accept(Message* message) override {
    return first_validator_->Accept(message) &&
           second_validator_->Accept(message);
  }

 private:
  std::unique_ptr<MessageReceiver> first_validator_;
  std::unique_ptr<MessageReceiver> second_validator_;
};

class DummyMessageReceiver : public MessageReceiver {
 public:
  bool Accept(Message* message) override {
    return true;  // Any message is OK.
  }
};

class ValidationTest : public testing::Test {
 public:
  ValidationTest() {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

class ValidationIntegrationTest : public ValidationTest {
 public:
  ValidationIntegrationTest() = default;
  ~ValidationIntegrationTest() override = default;

  void SetUp() override {
    ScopedMessagePipeHandle tester_endpoint;
    ASSERT_EQ(MOJO_RESULT_OK,
              CreateMessagePipe(nullptr, &tester_endpoint, &testee_endpoint_));
    test_message_receiver_ =
        std::make_unique<TestMessageReceiver>(this, std::move(tester_endpoint));
  }

  void TearDown() override {
    test_message_receiver_.reset();

    // Make sure that the other end receives the OnConnectionError()
    // notification.
    PumpMessages();
  }

  MessageReceiver* test_message_receiver() {
    return test_message_receiver_.get();
  }

  ScopedMessagePipeHandle testee_endpoint() {
    return std::move(testee_endpoint_);
  }

 private:
  class TestMessageReceiver : public MessageReceiver {
   public:
    TestMessageReceiver(ValidationIntegrationTest* owner,
                        ScopedMessagePipeHandle handle)
        : owner_(owner),
          connector_(std::move(handle),
                     mojo::Connector::SINGLE_THREADED_SEND,
                     base::SingleThreadTaskRunner::GetCurrentDefault()) {
      connector_.set_enforce_errors_from_incoming_receiver(false);
    }
    ~TestMessageReceiver() override {}

    bool Accept(Message* message) override {
      return connector_.Accept(message);
    }

   public:
    raw_ptr<ValidationIntegrationTest> owner_;
    mojo::Connector connector_;
  };

  void PumpMessages() { base::RunLoop().RunUntilIdle(); }

  std::unique_ptr<TestMessageReceiver> test_message_receiver_;
  ScopedMessagePipeHandle testee_endpoint_;
};

class IntegrationTestInterfaceImpl : public IntegrationTestInterface {
 public:
  ~IntegrationTestInterfaceImpl() override {}

  void Method0(BasicStructPtr param0, Method0Callback callback) override {
    std::move(callback).Run(std::vector<uint8_t>());
  }
};

TEST_F(ValidationTest, InputParser) {
  {
    // The parser, as well as Append() defined above, assumes that this code is
    // running on a little-endian platform. Test whether that is true.
    uint16_t x = 1;
    ASSERT_EQ(1, *(reinterpret_cast<char*>(&x)));
  }
  {
    // Test empty input.
    std::string input;
    std::vector<uint8_t> expected;

    EXPECT_TRUE(TestInputParser(input, true, expected, 0));
  }
  {
    // Test input that only consists of comments and whitespaces.
    std::string input = "    \t  // hello world \n\r \t// the answer is 42   ";
    std::vector<uint8_t> expected;

    EXPECT_TRUE(TestInputParser(input, true, expected, 0));
  }
  {
    std::string input =
        "[u1]0x10// hello world !! \n\r  \t [u2]65535 \n"
        "[u4]65536 [u8]0xFFFFFFFFFFFFFFFF 0 0Xff";
    std::vector<uint8_t> expected;
    Append(&expected, static_cast<uint8_t>(0x10));
    Append(&expected, static_cast<uint16_t>(65535));
    Append(&expected, static_cast<uint32_t>(65536));
    Append(&expected, static_cast<uint64_t>(0xffffffffffffffff));
    Append(&expected, static_cast<uint8_t>(0));
    Append(&expected, static_cast<uint8_t>(0xff));

    EXPECT_TRUE(TestInputParser(input, true, expected, 0));
  }
  {
    std::string input = "[s8]-0x800 [s1]-128\t[s2]+0 [s4]-40";
    std::vector<uint8_t> expected;
    Append(&expected, -static_cast<int64_t>(0x800));
    Append(&expected, static_cast<int8_t>(-128));
    Append(&expected, static_cast<int16_t>(0));
    Append(&expected, static_cast<int32_t>(-40));

    EXPECT_TRUE(TestInputParser(input, true, expected, 0));
  }
  {
    std::string input = "[b]00001011 [b]10000000  // hello world\r [b]00000000";
    std::vector<uint8_t> expected;
    Append(&expected, static_cast<uint8_t>(11));
    Append(&expected, static_cast<uint8_t>(128));
    Append(&expected, static_cast<uint8_t>(0));

    EXPECT_TRUE(TestInputParser(input, true, expected, 0));
  }
  {
    std::string input = "[f]+.3e9 [d]-10.03";
    std::vector<uint8_t> expected;
    Append(&expected, +.3e9f);
    Append(&expected, -10.03);

    EXPECT_TRUE(TestInputParser(input, true, expected, 0));
  }
  {
    std::string input = "[dist4]foo 0 [dist8]bar 0 [anchr]foo [anchr]bar";
    std::vector<uint8_t> expected;
    Append(&expected, static_cast<uint32_t>(14));
    Append(&expected, static_cast<uint8_t>(0));
    Append(&expected, static_cast<uint64_t>(9));
    Append(&expected, static_cast<uint8_t>(0));

    EXPECT_TRUE(TestInputParser(input, true, expected, 0));
  }
  {
    std::string input = "// This message has handles! \n[handles]50 [u8]2";
    std::vector<uint8_t> expected;
    Append(&expected, static_cast<uint64_t>(2));

    EXPECT_TRUE(TestInputParser(input, true, expected, 50));
  }

  // Test some failure cases.
  {
    const char* error_inputs[] = {"/ hello world",
                                  "[u1]x",
                                  "[u2]-1000",
                                  "[u1]0x100",
                                  "[s2]-0x8001",
                                  "[b]1",
                                  "[b]1111111k",
                                  "[dist4]unmatched",
                                  "[anchr]hello [dist8]hello",
                                  "[dist4]a [dist4]a [anchr]a",
                                  "[dist4]a [anchr]a [dist4]a [anchr]a",
                                  "0 [handles]50",
                                  nullptr};

    for (size_t i = 0; error_inputs[i]; ++i) {
      std::vector<uint8_t> expected;
      if (!TestInputParser(error_inputs[i], false, expected, 0))
        ADD_FAILURE() << "Unexpected test result for: " << error_inputs[i];
    }
  }
}

TEST_F(ValidationTest, Conformance) {
  DummyMessageReceiver dummy_receiver;
  mojo::MessageDispatcher validators(&dummy_receiver);
  validators.SetValidator(std::make_unique<TwoStepValidator>(
      std::make_unique<mojo::MessageHeaderValidator>(),
      std::make_unique<ConformanceTestInterface::RequestValidator_>()));

  RunValidationTests("conformance_", &validators);
}

TEST_F(ValidationTest, AssociatedConformace) {
  DummyMessageReceiver dummy_receiver;
  mojo::MessageDispatcher validators(&dummy_receiver);
  validators.SetValidator(std::make_unique<TwoStepValidator>(
      std::make_unique<mojo::MessageHeaderValidator>(),
      std::make_unique<
          AssociatedConformanceTestInterface::RequestValidator_>()));

  RunValidationTests("associated_conformance_", &validators);
}

// This test is similar to Conformance test but its goal is specifically
// do bounds-check testing of message validation. For example we test the
// detection of off-by-one errors in method ordinals.
TEST_F(ValidationTest, BoundsCheck) {
  DummyMessageReceiver dummy_receiver;
  mojo::MessageDispatcher validators(&dummy_receiver);
  validators.SetValidator(std::make_unique<TwoStepValidator>(
      std::make_unique<mojo::MessageHeaderValidator>(),
      std::make_unique<BoundsCheckTestInterface::RequestValidator_>()));
  RunValidationTests("boundscheck_", &validators);
}

// This test is similar to the Conformance test but for responses.
TEST_F(ValidationTest, ResponseConformance) {
  DummyMessageReceiver dummy_receiver;
  mojo::MessageDispatcher validators(&dummy_receiver);
  validators.SetValidator(std::make_unique<TwoStepValidator>(
      std::make_unique<mojo::MessageHeaderValidator>(),
      std::make_unique<ConformanceTestInterface::ResponseValidator_>()));

  RunValidationTests("resp_conformance_", &validators);
}

// This test is similar to the BoundsCheck test but for responses.
TEST_F(ValidationTest, ResponseBoundsCheck) {
  DummyMessageReceiver dummy_receiver;
  mojo::MessageDispatcher validators(&dummy_receiver);
  validators.SetValidator(std::make_unique<TwoStepValidator>(
      std::make_unique<mojo::MessageHeaderValidator>(),
      std::make_unique<BoundsCheckTestInterface::ResponseValidator_>()));

  RunValidationTests("resp_boundscheck_", &validators);
}

// Test that Remote<X> applies the correct validators and they don't conflict
// with each other:
//   - MessageHeaderValidator
//   - X::ResponseValidator_
TEST_F(ValidationIntegrationTest, Remote) {
  Remote<IntegrationTestInterface> remote(
      PendingRemote<IntegrationTestInterface>(testee_endpoint(), 0u));
  remote.internal_state()->EnableTestingMode();

  RunValidationTests("integration_intf_resp", test_message_receiver());
  RunValidationTests("integration_msghdr", test_message_receiver());
}

// Test that Receiver<X> applies the correct validators and they don't conflict
// with each other:
//   - MessageHeaderValidator
//   - X::RequestValidator_
TEST_F(ValidationIntegrationTest, Receiver) {
  IntegrationTestInterfaceImpl interface_impl;
  Receiver<IntegrationTestInterface> receiver(
      &interface_impl,
      PendingReceiver<IntegrationTestInterface>(testee_endpoint()));
  receiver.internal_state()->EnableTestingMode();

  RunValidationTests("integration_intf_rqst", test_message_receiver());
  RunValidationTests("integration_msghdr", test_message_receiver());
}

// Test pointer validation (specifically, that the encoded offset is 32-bit)
TEST_F(ValidationTest, ValidateEncodedPointer) {
  uint64_t offset;

  offset = 0ULL;
  EXPECT_TRUE(mojo::internal::ValidateEncodedPointer(&offset));

  offset = 1ULL;
  EXPECT_TRUE(mojo::internal::ValidateEncodedPointer(&offset));

  // offset must be <= 32-bit.
  offset = std::numeric_limits<uint32_t>::max() + 1ULL;
  EXPECT_FALSE(mojo::internal::ValidateEncodedPointer(&offset));
}

// Tests the IsKnownEnumValue() function generated for BasicEnum.
TEST(EnumValueValidationTest, BasicEnum) {
  // BasicEnum can have -3,0,1,10 as possible integral values.
  EXPECT_FALSE(IsKnownEnumValue(static_cast<BasicEnum>(-4)));
  EXPECT_TRUE(IsKnownEnumValue(static_cast<BasicEnum>(-3)));
  EXPECT_FALSE(IsKnownEnumValue(static_cast<BasicEnum>(-2)));
  EXPECT_FALSE(IsKnownEnumValue(static_cast<BasicEnum>(-1)));
  EXPECT_TRUE(IsKnownEnumValue(static_cast<BasicEnum>(0)));
  EXPECT_TRUE(IsKnownEnumValue(static_cast<BasicEnum>(1)));
  EXPECT_FALSE(IsKnownEnumValue(static_cast<BasicEnum>(2)));
  EXPECT_FALSE(IsKnownEnumValue(static_cast<BasicEnum>(9)));
  // In the mojom, we represent this value as hex (0xa).
  EXPECT_TRUE(IsKnownEnumValue(static_cast<BasicEnum>(10)));
  EXPECT_FALSE(IsKnownEnumValue(static_cast<BasicEnum>(11)));
}

// Tests the IsKnownEnumValue() method generated for StructWithEnum.
TEST(EnumValueValidationTest, EnumWithin) {
  // StructWithEnum::EnumWithin can have [0,4] as possible integral values.
  EXPECT_FALSE(IsKnownEnumValue(static_cast<StructWithEnum::EnumWithin>(-1)));
  EXPECT_TRUE(IsKnownEnumValue(static_cast<StructWithEnum::EnumWithin>(0)));
  EXPECT_TRUE(IsKnownEnumValue(static_cast<StructWithEnum::EnumWithin>(1)));
  EXPECT_TRUE(IsKnownEnumValue(static_cast<StructWithEnum::EnumWithin>(2)));
  EXPECT_TRUE(IsKnownEnumValue(static_cast<StructWithEnum::EnumWithin>(3)));
  EXPECT_FALSE(IsKnownEnumValue(static_cast<StructWithEnum::EnumWithin>(4)));
}

}  // namespace
}  // namespace test
}  // namespace mojo
