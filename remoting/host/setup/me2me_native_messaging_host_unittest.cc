// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/host/setup/me2me_native_messaging_host.h"

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/stringize_macros.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/base/file_stream.h"
#include "net/base/network_interfaces.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/mock_oauth_client.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/native_messaging/log_message_handler.h"
#include "remoting/host/native_messaging/native_messaging_pipe.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/test_util.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using remoting::protocol::MockPairingRegistryDelegate;
using remoting::protocol::PairingRegistry;
using remoting::protocol::SynchronousPairingRegistry;
using ::testing::Optional;

void VerifyHelloResponse(const base::Value::Dict& response) {
  const std::string* value = response.FindString("type");
  ASSERT_TRUE(value);
  EXPECT_EQ("helloResponse", *value);

  value = response.FindString("version");
  ASSERT_TRUE(value);

// The check below will compile but fail if VERSION isn't defined (STRINGIZE
// silently converts undefined values).
#ifndef VERSION
#error VERSION must be defined
#endif
  EXPECT_EQ(STRINGIZE(VERSION), *value);
}

void VerifyGetHostNameResponse(const base::Value::Dict& response) {
  const std::string* value = response.FindString("type");
  ASSERT_TRUE(value);
  EXPECT_EQ("getHostNameResponse", *value);
  value = response.FindString("hostname");
  ASSERT_TRUE(value);
  EXPECT_EQ(net::GetHostName(), *value);
}

void VerifyGetPinHashResponse(const base::Value::Dict& response) {
  const std::string* value = response.FindString("type");
  ASSERT_TRUE(value);
  EXPECT_EQ("getPinHashResponse", *value);
  value = response.FindString("hash");
  ASSERT_TRUE(value);
  EXPECT_EQ(remoting::MakeHostPinHash("my_host", "1234"), *value);
}

void VerifyGenerateKeyPairResponse(const base::Value::Dict& response) {
  const std::string* value = response.FindString("type");
  ASSERT_TRUE(value);
  EXPECT_EQ("generateKeyPairResponse", *value);
  EXPECT_TRUE(response.FindString("privateKey"));
  EXPECT_TRUE(response.FindString("publicKey"));
}

void VerifyGetDaemonConfigResponse(const base::Value::Dict& response) {
  const std::string* value = response.FindString("type");
  ASSERT_TRUE(value);
  EXPECT_EQ("getDaemonConfigResponse", *value);
  const base::Value::Dict* config = response.FindDict("config");
  ASSERT_TRUE(config);
  EXPECT_EQ(base::Value::Dict(), *config);
}

void VerifyGetUsageStatsConsentResponse(const base::Value::Dict& response) {
  const std::string* value = response.FindString("type");
  ASSERT_TRUE(value);
  EXPECT_EQ("getUsageStatsConsentResponse", *value);

  EXPECT_THAT(response.FindBool("supported"), Optional(true));
  EXPECT_THAT(response.FindBool("allowed"), Optional(true));
  EXPECT_THAT(response.FindBool("setByPolicy"), Optional(true));
}

void VerifyStopDaemonResponse(const base::Value::Dict& response) {
  const std::string* value = response.FindString("type");
  ASSERT_TRUE(value);
  EXPECT_EQ("stopDaemonResponse", *value);
  value = response.FindString("result");
  ASSERT_TRUE(value);
  EXPECT_EQ("OK", *value);
}

void VerifyGetDaemonStateResponse(const base::Value::Dict& response) {
  const std::string* value = response.FindString("type");
  ASSERT_TRUE(value);
  EXPECT_EQ("getDaemonStateResponse", *value);
  value = response.FindString("state");
  ASSERT_TRUE(value);
  EXPECT_EQ("STARTED", *value);
}

void VerifyUpdateDaemonConfigResponse(const base::Value::Dict& response) {
  const std::string* value = response.FindString("type");
  ASSERT_TRUE(value);
  EXPECT_EQ("updateDaemonConfigResponse", *value);
  value = response.FindString("result");
  ASSERT_TRUE(value);
  EXPECT_EQ("OK", *value);
}

void VerifyStartDaemonResponse(const base::Value::Dict& response) {
  const std::string* value = response.FindString("type");
  ASSERT_TRUE(value);
  EXPECT_EQ("startDaemonResponse", *value);
  value = response.FindString("result");
  ASSERT_TRUE(value);
  EXPECT_EQ("OK", *value);
}

void VerifyGetCredentialsFromAuthCodeResponse(
    const base::Value::Dict& response) {
  const std::string* value = response.FindString("type");
  ASSERT_TRUE(value);
  EXPECT_EQ("getCredentialsFromAuthCodeResponse", *value);
  value = response.FindString("userEmail");
  ASSERT_TRUE(value);
  EXPECT_EQ("fake_user_email", *value);
  value = response.FindString("refreshToken");
  ASSERT_TRUE(value);
  EXPECT_EQ("fake_refresh_token", *value);
}

}  // namespace

namespace remoting {

class MockDaemonControllerDelegate : public DaemonController::Delegate {
 public:
  MockDaemonControllerDelegate();

  MockDaemonControllerDelegate(const MockDaemonControllerDelegate&) = delete;
  MockDaemonControllerDelegate& operator=(const MockDaemonControllerDelegate&) =
      delete;

  ~MockDaemonControllerDelegate() override;

  // DaemonController::Delegate interface.
  DaemonController::State GetState() override;
  std::optional<base::Value::Dict> GetConfig() override;
  void CheckPermission(bool it2me,
                       DaemonController::BoolCallback callback) override;
  void SetConfigAndStart(base::Value::Dict config,
                         bool consent,
                         DaemonController::CompletionCallback done) override;
  void UpdateConfig(base::Value::Dict config,
                    DaemonController::CompletionCallback done) override;
  void Stop(DaemonController::CompletionCallback done) override;
  DaemonController::UsageStatsConsent GetUsageStatsConsent() override;
};

MockDaemonControllerDelegate::MockDaemonControllerDelegate() = default;

MockDaemonControllerDelegate::~MockDaemonControllerDelegate() = default;

DaemonController::State MockDaemonControllerDelegate::GetState() {
  return DaemonController::STATE_STARTED;
}

std::optional<base::Value::Dict> MockDaemonControllerDelegate::GetConfig() {
  return base::Value::Dict();
}

void MockDaemonControllerDelegate::CheckPermission(
    bool it2me,
    DaemonController::BoolCallback callback) {
  std::move(callback).Run(true);
}

void MockDaemonControllerDelegate::SetConfigAndStart(
    base::Value::Dict config,
    bool consent,
    DaemonController::CompletionCallback done) {
  // Verify parameters passed in.
  if (consent && config.Find("start")) {
    std::move(done).Run(DaemonController::RESULT_OK);
  } else {
    std::move(done).Run(DaemonController::RESULT_FAILED);
  }
}

void MockDaemonControllerDelegate::UpdateConfig(
    base::Value::Dict config,
    DaemonController::CompletionCallback done) {
  if (config.Find("update")) {
    std::move(done).Run(DaemonController::RESULT_OK);
  } else {
    std::move(done).Run(DaemonController::RESULT_FAILED);
  }
}

void MockDaemonControllerDelegate::Stop(
    DaemonController::CompletionCallback done) {
  std::move(done).Run(DaemonController::RESULT_OK);
}

DaemonController::UsageStatsConsent
MockDaemonControllerDelegate::GetUsageStatsConsent() {
  DaemonController::UsageStatsConsent consent;
  consent.supported = true;
  consent.allowed = true;
  consent.set_by_policy = true;
  return consent;
}

class Me2MeNativeMessagingHostTest : public testing::Test {
 public:
  Me2MeNativeMessagingHostTest();

  Me2MeNativeMessagingHostTest(const Me2MeNativeMessagingHostTest&) = delete;
  Me2MeNativeMessagingHostTest& operator=(const Me2MeNativeMessagingHostTest&) =
      delete;

  ~Me2MeNativeMessagingHostTest() override;

  void SetUp() override;
  void TearDown() override;

  std::optional<base::Value::Dict> ReadMessageFromOutputPipe();

  void WriteMessageToInputPipe(const base::ValueView& message);

  // The Host process should shut down when it receives a malformed request.
  // This is tested by sending a known-good request, followed by |message|,
  // followed by the known-good request again. The response file should only
  // contain a single response from the first good request.
  void TestBadRequest(const base::Value& message);

 protected:
  // Reference to the MockDaemonControllerDelegate, which is owned by
  // |channel_|.
  raw_ptr<MockDaemonControllerDelegate, AcrossTasksDanglingUntriaged>
      daemon_controller_delegate_;

 private:
  void StartHost();
  void StopHost();
  void ExitTest();

  // Each test creates two unidirectional pipes: "input" and "output".
  // Me2MeNativeMessagingHost reads from input_read_handle and writes to
  // output_write_file. The unittest supplies data to input_write_handle, and
  // verifies output from output_read_handle.
  //
  // unittest -> [input] -> Me2MeNativeMessagingHost -> [output] -> unittest
  base::File input_write_file_;
  base::File output_read_file_;

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<base::RunLoop> test_run_loop_;

  std::unique_ptr<base::Thread> host_thread_;
  std::unique_ptr<base::RunLoop> host_run_loop_;

  scoped_refptr<network::TestSharedURLLoaderFactory> test_url_loader_factory_;

  // Task runner of the host thread.
  scoped_refptr<AutoThreadTaskRunner> host_task_runner_;
  std::unique_ptr<NativeMessagingPipe> native_messaging_pipe_;
};

Me2MeNativeMessagingHostTest::Me2MeNativeMessagingHostTest() = default;

Me2MeNativeMessagingHostTest::~Me2MeNativeMessagingHostTest() = default;

void Me2MeNativeMessagingHostTest::SetUp() {
  base::File input_read_file;
  base::File output_write_file;

  ASSERT_TRUE(MakePipe(&input_read_file, &input_write_file_));
  ASSERT_TRUE(MakePipe(&output_read_file_, &output_write_file));

  task_environment_ = std::make_unique<base::test::TaskEnvironment>();
  test_run_loop_ = std::make_unique<base::RunLoop>();

  // Run the host on a dedicated thread.
  host_thread_ = std::make_unique<base::Thread>("host_thread");
  host_thread_->Start();

  // Arrange to run |task_environment_| until no components depend on it.
  host_task_runner_ = new AutoThreadTaskRunner(
      host_thread_->task_runner(),
      base::BindOnce(&Me2MeNativeMessagingHostTest::ExitTest,
                     base::Unretained(this)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  test_url_loader_factory_ = new network::TestSharedURLLoaderFactory();
#endif

  host_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Me2MeNativeMessagingHostTest::StartHost,
                                base::Unretained(this)));

  // Wait until the host finishes starting.
  test_run_loop_->Run();
}

void Me2MeNativeMessagingHostTest::StartHost() {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());

  base::File input_read_file;
  base::File output_write_file;

  ASSERT_TRUE(MakePipe(&input_read_file, &input_write_file_));
  ASSERT_TRUE(MakePipe(&output_read_file_, &output_write_file));

  daemon_controller_delegate_ = new MockDaemonControllerDelegate();
  scoped_refptr<DaemonController> daemon_controller(new DaemonController(
      base::WrapUnique(daemon_controller_delegate_.get())));

  scoped_refptr<PairingRegistry> pairing_registry =
      new SynchronousPairingRegistry(
          base::WrapUnique(new MockPairingRegistryDelegate()));

  native_messaging_pipe_ = std::make_unique<NativeMessagingPipe>();

  std::unique_ptr<extensions::NativeMessagingChannel> channel(
      new PipeMessagingChannel(std::move(input_read_file),
                               std::move(output_write_file)));

  std::unique_ptr<OAuthClient> oauth_client(
      new MockOAuthClient("fake_user_email", "fake_refresh_token"));

  std::unique_ptr<ChromotingHostContext> context =
      ChromotingHostContext::CreateForTesting(
          new remoting::AutoThreadTaskRunner(
              host_task_runner_,
              base::BindOnce(&Me2MeNativeMessagingHostTest::StopHost,
                             base::Unretained(this))),
          test_url_loader_factory_);

  std::unique_ptr<remoting::Me2MeNativeMessagingHost> host(
      new Me2MeNativeMessagingHost(false, 0, std::move(context),
                                   daemon_controller, pairing_registry,
                                   std::move(oauth_client)));
  host->Start(native_messaging_pipe_.get());

  native_messaging_pipe_->Start(std::move(host), std::move(channel));

  // Notify the test that the host has finished starting up.
  test_run_loop_->Quit();
}

void Me2MeNativeMessagingHostTest::StopHost() {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());

  native_messaging_pipe_.reset();

  // Wait till all shutdown tasks have completed.
  base::RunLoop().RunUntilIdle();

  // Trigger a test shutdown via ExitTest().
  host_task_runner_ = nullptr;
}

void Me2MeNativeMessagingHostTest::ExitTest() {
  if (!task_environment_->GetMainThreadTaskRunner()
           ->RunsTasksInCurrentSequence()) {
    task_environment_->GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&Me2MeNativeMessagingHostTest::ExitTest,
                                  base::Unretained(this)));
    return;
  }
  test_run_loop_->Quit();
}

void Me2MeNativeMessagingHostTest::TearDown() {
  // Closing the write-end of the input will send an EOF to the native
  // messaging reader. This will trigger a host shutdown.
  input_write_file_.Close();

  // Start a new RunLoop and Wait until the host finishes shutting down.
  test_run_loop_ = std::make_unique<base::RunLoop>();
  test_run_loop_->Run();

  // Verify there are no more message in the output pipe.
  std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);

  // The It2MeMe2MeNativeMessagingHost dtor closes the handles that are passed
  // to it. So the only handle left to close is |output_read_file_|.
  output_read_file_.Close();
}

std::optional<base::Value::Dict>
Me2MeNativeMessagingHostTest::ReadMessageFromOutputPipe() {
  while (true) {
    uint32_t length;
    int read_result = output_read_file_.ReadAtCurrentPos(
        reinterpret_cast<char*>(&length), sizeof(length));
    if (read_result != sizeof(length)) {
      return std::nullopt;
    }

    std::string message_json(length, '\0');
    read_result =
        output_read_file_.ReadAtCurrentPos(std::data(message_json), length);
    if (read_result != static_cast<int>(length)) {
      return std::nullopt;
    }

    std::optional<base::Value> message = base::JSONReader::Read(message_json);
    if (!message || !message->is_dict()) {
      return std::nullopt;
    }

    base::Value::Dict& result = message->GetDict();
    const std::string* type = result.FindString("type");
    // If this is a debug message log, ignore it, otherwise return it.
    if (!type || *type != LogMessageHandler::kDebugMessageTypeName) {
      return std::move(result);
    }
  }
}

void Me2MeNativeMessagingHostTest::WriteMessageToInputPipe(
    const base::ValueView& message) {
  std::string message_json;
  base::JSONWriter::Write(message, &message_json);

  uint32_t length = base::checked_cast<uint32_t>(message_json.length());
  input_write_file_.WriteAtCurrentPos(base::byte_span_from_ref(length));
  input_write_file_.WriteAtCurrentPos(base::as_byte_span(message_json));
}

void Me2MeNativeMessagingHostTest::TestBadRequest(const base::Value& message) {
  base::Value::Dict good_message;
  good_message.Set("type", "hello");

  // This test currently relies on synchronous processing of hello messages and
  // message parameters verification.
  WriteMessageToInputPipe(good_message);
  WriteMessageToInputPipe(message);
  WriteMessageToInputPipe(good_message);

  // Read from output pipe, and verify responses.
  std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
  ASSERT_TRUE(response);
  VerifyHelloResponse(std::move(*response));

  response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);
}

// TODO (weitaosu): crbug.com/323306. Re-enable these tests.
// Test all valid request-types.
TEST_F(Me2MeNativeMessagingHostTest, All) {
  int next_id = 0;
  base::Value::Dict message;
  message.Set("id", next_id++);
  message.Set("type", "hello");
  WriteMessageToInputPipe(message);

  message.Set("id", next_id++);
  message.Set("type", "getHostName");
  WriteMessageToInputPipe(message);

  message.Set("id", next_id++);
  message.Set("type", "getPinHash");
  message.Set("hostId", "my_host");
  message.Set("pin", "1234");
  WriteMessageToInputPipe(message);

  message.clear();
  message.Set("id", next_id++);
  message.Set("type", "generateKeyPair");
  WriteMessageToInputPipe(message);

  message.Set("id", next_id++);
  message.Set("type", "getDaemonConfig");
  WriteMessageToInputPipe(message);

  message.Set("id", next_id++);
  message.Set("type", "getUsageStatsConsent");
  WriteMessageToInputPipe(message);

  message.Set("id", next_id++);
  message.Set("type", "stopDaemon");
  WriteMessageToInputPipe(message);

  message.Set("id", next_id++);
  message.Set("type", "getDaemonState");
  WriteMessageToInputPipe(message);

  // Following messages require a "config" dictionary.
  base::Value::Dict config;
  config.Set("update", true);
  message.Set("config", config.Clone());
  message.Set("id", next_id++);
  message.Set("type", "updateDaemonConfig");
  WriteMessageToInputPipe(message);

  config.clear();
  config.Set("start", true);
  message.Set("config", config.Clone());
  message.Set("consent", true);
  message.Set("id", next_id++);
  message.Set("type", "startDaemon");
  WriteMessageToInputPipe(message);

  message.Set("id", next_id++);
  message.Set("type", "getCredentialsFromAuthCode");
  message.Set("authorizationCode", "fake_auth_code");
  WriteMessageToInputPipe(message);

  void (*verify_routines[])(const base::Value::Dict&) = {
      &VerifyHelloResponse,
      &VerifyGetHostNameResponse,
      &VerifyGetPinHashResponse,
      &VerifyGenerateKeyPairResponse,
      &VerifyGetDaemonConfigResponse,
      &VerifyGetUsageStatsConsentResponse,
      &VerifyStopDaemonResponse,
      &VerifyGetDaemonStateResponse,
      &VerifyUpdateDaemonConfigResponse,
      &VerifyStartDaemonResponse,
      &VerifyGetCredentialsFromAuthCodeResponse,
  };
  ASSERT_EQ(std::size(verify_routines), static_cast<size_t>(next_id));

  // Read all responses from output pipe, and verify them.
  for (int i = 0; i < next_id; ++i) {
    std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
    ASSERT_TRUE(response);

    // Make sure that id is available and is in the range.
    std::optional<int> id = response->FindInt("id");
    ASSERT_TRUE(id);
    ASSERT_TRUE(0 <= *id && *id < next_id);

    // Call the verification routine corresponding to the message id.
    ASSERT_TRUE(verify_routines[*id]);
    verify_routines[*id](std::move(*response));

    // Clear the pointer so that the routine cannot be called the second time.
    verify_routines[*id] = nullptr;
  }
}

// Verify that response ID matches request ID.
TEST_F(Me2MeNativeMessagingHostTest, Id) {
  base::Value::Dict message;
  message.Set("type", "hello");
  WriteMessageToInputPipe(message);
  message.Set("id", "42");
  WriteMessageToInputPipe(message);

  std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
  EXPECT_TRUE(response);
  std::string* value = response->FindString("id");
  EXPECT_FALSE(value);

  response = ReadMessageFromOutputPipe();
  EXPECT_TRUE(response);
  value = response->FindString("id");
  EXPECT_TRUE(value);
  EXPECT_EQ("42", *value);
}

// Verify non-Dictionary requests are rejected.
TEST_F(Me2MeNativeMessagingHostTest, WrongFormat) {
  TestBadRequest(base::Value(base::Value::Type::LIST));
}

// Verify requests with no type are rejected.
TEST_F(Me2MeNativeMessagingHostTest, MissingType) {
  TestBadRequest(base::Value(base::Value::Type::DICT));
}

// Verify rejection if type is unrecognized.
TEST_F(Me2MeNativeMessagingHostTest, InvalidType) {
  base::Value::Dict message;
  message.Set("type", "xxx");
  TestBadRequest(base::Value(std::move(message)));
}

// Verify rejection if getPinHash request has no hostId.
TEST_F(Me2MeNativeMessagingHostTest, GetPinHashNoHostId) {
  base::Value::Dict message;
  message.Set("type", "getPinHash");
  message.Set("pin", "1234");
  TestBadRequest(base::Value(std::move(message)));
}

// Verify rejection if getPinHash request has no pin.
TEST_F(Me2MeNativeMessagingHostTest, GetPinHashNoPin) {
  base::Value::Dict message;
  message.Set("type", "getPinHash");
  message.Set("hostId", "my_host");
  TestBadRequest(base::Value(std::move(message)));
}

// Verify rejection if updateDaemonConfig request has invalid config.
TEST_F(Me2MeNativeMessagingHostTest, UpdateDaemonConfigInvalidConfig) {
  base::Value::Dict message;
  message.Set("type", "updateDaemonConfig");
  message.Set("config", "xxx");
  TestBadRequest(base::Value(std::move(message)));
}

// Verify rejection if startDaemon request has invalid config.
TEST_F(Me2MeNativeMessagingHostTest, StartDaemonInvalidConfig) {
  base::Value::Dict message;
  message.Set("type", "startDaemon");
  message.Set("config", "xxx");
  message.Set("consent", true);
  TestBadRequest(base::Value(std::move(message)));
}

// Verify rejection if startDaemon request has no "consent" parameter.
TEST_F(Me2MeNativeMessagingHostTest, StartDaemonNoConsent) {
  base::Value::Dict message;
  message.Set("type", "startDaemon");
  message.Set("config", base::Value::Dict());
  TestBadRequest(base::Value(std::move(message)));
}

// Verify rejection if getCredentialsFromAuthCode has no auth code.
TEST_F(Me2MeNativeMessagingHostTest, GetCredentialsFromAuthCodeNoAuthCode) {
  base::Value::Dict message;
  message.Set("type", "getCredentialsFromAuthCode");
  TestBadRequest(base::Value(std::move(message)));
}

}  // namespace remoting
