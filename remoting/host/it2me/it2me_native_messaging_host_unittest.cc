// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/host/it2me/it2me_native_messaging_host.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/common/fake_async_policy_loader.h"
#include "components/policy/policy_constants.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/host/it2me/it2me_helpers.h"
#include "remoting/host/native_messaging/log_message_handler.h"
#include "remoting/host/native_messaging/native_messaging_pipe.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/host/setup/test_util.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/signaling/log_to_server.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using protocol::ErrorCode;

namespace {

const char kTestAccessCode[] = "888888";
constexpr base::TimeDelta kTestAccessCodeLifetime = base::Seconds(666);
const char kTestClientUsername[] = "some_user@gmail.com";
const char kTestStunServer[] = "test_relay_server.com";

void VerifyId(const base::Value::Dict& response, int expected_value) {
  std::optional<int> value = response.FindInt(kMessageId);
  ASSERT_TRUE(value);
  EXPECT_EQ(expected_value, *value);
}

void VerifyStringProperty(const base::Value::Dict& response,
                          const std::string& name,
                          const std::string& expected_value) {
  const std::string* value = response.FindString(name);
  ASSERT_TRUE(value);
  EXPECT_EQ(expected_value, *value);
}

// Verity the values of the "type" and "id" properties
void VerifyCommonProperties(const base::Value::Dict& response,
                            const std::string& type,
                            int id) {
  const std::string* string_value = response.FindString(kMessageType);
  ASSERT_TRUE(string_value);
  EXPECT_EQ(type, *string_value);

  std::optional<int> int_value = response.FindInt(kMessageId);
  ASSERT_TRUE(int_value);
  EXPECT_EQ(id, *int_value);
}

base::Value::Dict CreateConnectMessage(int id) {
  base::Value::Dict connect_message;
  connect_message.Set(kMessageId, id);
  connect_message.Set(kMessageType, kConnectMessage);
  connect_message.Set(kUserName, kTestClientUsername);
  connect_message.Set(kAuthServiceWithToken, "oauth2:sometoken");
  connect_message.Set(
      kIceConfig,
      base::test::ParseJsonDict("{ \"iceServers\": [ { \"urls\": [ \"stun:" +
                                std::string(kTestStunServer) + "\" ] } ] }"));

  return connect_message;
}

base::Value::Dict CreateDisconnectMessage(int id) {
  base::Value::Dict disconnect_message;
  disconnect_message.Set(kMessageId, id);
  disconnect_message.Set(kMessageType, kDisconnectMessage);
  return disconnect_message;
}

}  // namespace

class MockIt2MeHost : public It2MeHost {
 public:
  MockIt2MeHost() = default;

  MockIt2MeHost(const MockIt2MeHost&) = delete;
  MockIt2MeHost& operator=(const MockIt2MeHost&) = delete;

  // It2MeHost overrides
  void Connect(std::unique_ptr<ChromotingHostContext> context,
               base::Value::Dict policies,
               std::unique_ptr<It2MeConfirmationDialogFactory> dialog_factory,
               base::WeakPtr<It2MeHost::Observer> observer,
               CreateDeferredConnectContext create_connection_context,
               const std::string& username,
               const protocol::IceConfig& ice_config) override;
  void Disconnect() override;

 private:
  ~MockIt2MeHost() override = default;

  void CreateConnectionContextOnNetworkThread(
      CreateDeferredConnectContext create_connection_context);

  void RunSetState(It2MeHostState state);
};

void MockIt2MeHost::Connect(
    std::unique_ptr<ChromotingHostContext> context,
    base::Value::Dict policies,
    std::unique_ptr<It2MeConfirmationDialogFactory> dialog_factory,
    base::WeakPtr<It2MeHost::Observer> observer,
    CreateDeferredConnectContext create_connection_context,
    const std::string& username,
    const protocol::IceConfig& ice_config) {
  DCHECK(context->ui_task_runner()->BelongsToCurrentThread());

  // Verify that parameters are passed correctly.
  EXPECT_EQ(username, kTestClientUsername);
  EXPECT_EQ(ice_config.stun_servers[0].hostname(), kTestStunServer);

  host_context_ = std::move(context);
  observer_ = std::move(observer);

  host_context()->network_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockIt2MeHost::CreateConnectionContextOnNetworkThread,
                     this, std::move(create_connection_context)));

  OnPolicyUpdate(std::move(policies));

  RunSetState(It2MeHostState::kStarting);
  RunSetState(It2MeHostState::kRequestedAccessCode);

  host_context()->ui_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&It2MeHost::Observer::OnStoreAccessCode, observer_,
                     kTestAccessCode, kTestAccessCodeLifetime));

  RunSetState(It2MeHostState::kReceivedAccessCode);
  RunSetState(It2MeHostState::kConnecting);

  host_context()->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeHost::Observer::OnClientAuthenticated,
                                observer_, kTestClientUsername));

  RunSetState(It2MeHostState::kConnected);
}

void MockIt2MeHost::Disconnect() {
  if (!host_context()->network_task_runner()->BelongsToCurrentThread()) {
    DCHECK(host_context()->ui_task_runner()->BelongsToCurrentThread());
    host_context()->network_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&MockIt2MeHost::Disconnect, this));
    return;
  }

  log_to_server_.reset();
  register_request_.reset();
  signal_strategy_.reset();

  RunSetState(It2MeHostState::kDisconnected);
}

void MockIt2MeHost::CreateConnectionContextOnNetworkThread(
    CreateDeferredConnectContext create_connection_context) {
  DCHECK(host_context()->network_task_runner()->BelongsToCurrentThread());
  auto context = std::move(create_connection_context).Run(host_context());
  log_to_server_ = std::move(context->log_to_server);
  register_request_ = std::move(context->register_request);
  signal_strategy_ = std::move(context->signal_strategy);
}

void MockIt2MeHost::RunSetState(It2MeHostState state) {
  if (!host_context()->network_task_runner()->BelongsToCurrentThread()) {
    host_context()->network_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&It2MeHost::SetStateForTesting, this, state,
                                  ErrorCode::OK));
  } else {
    SetStateForTesting(state, ErrorCode::OK);
  }
}

class MockIt2MeHostFactory : public It2MeHostFactory {
 public:
  MockIt2MeHostFactory() : host(new MockIt2MeHost()) {}

  MockIt2MeHostFactory(const MockIt2MeHostFactory&) = delete;
  MockIt2MeHostFactory& operator=(const MockIt2MeHostFactory&) = delete;

  ~MockIt2MeHostFactory() override = default;

  scoped_refptr<It2MeHost> CreateIt2MeHost() override { return host; }

  scoped_refptr<MockIt2MeHost> host;
};

class It2MeNativeMessagingHostTest : public testing::Test {
 public:
  It2MeNativeMessagingHostTest() = default;

  It2MeNativeMessagingHostTest(const It2MeNativeMessagingHostTest&) = delete;
  It2MeNativeMessagingHostTest& operator=(const It2MeNativeMessagingHostTest&) =
      delete;

  ~It2MeNativeMessagingHostTest() override = default;

  void SetUp() override;
  void TearDown() override;

 protected:
  void SetPolicies(base::Value::Dict dict);
  std::optional<base::Value::Dict> ReadMessageFromOutputPipe();
  void WriteMessageToInputPipe(const base::Value::Dict& message);

  void VerifyHelloResponse(int request_id);
  void VerifyErrorResponse();
  void VerifyConnectResponses(int request_id);
  void VerifyDisconnectResponses(int request_id);
  void VerifyPolicyErrorResponse();

  // The Host process should shut down when it receives a malformed request.
  // This is tested by sending a known-good request, followed by |message|,
  // followed by the known-good request again. The response file should only
  // contain a single response from the first good request.
  void TestBadRequest(const base::Value::Dict& message,
                      bool expect_error_response);
  void TestConnect();

  const std::optional<ChromeOsEnterpriseParams>
  get_chrome_os_enterprise_params() {
    return factory_raw_ptr_->host->chrome_os_enterprise_params_;
  }

  // Raw pointer to host factory (owned by It2MeNativeMessagingHost).
  raw_ptr<MockIt2MeHostFactory, AcrossTasksDanglingUntriaged> factory_raw_ptr_ =
      nullptr;

 private:
  void StartHost();
  void ExitTest();
  void ExitPolicyRunLoop();

  // Each test creates two unidirectional pipes: "input" and "output".
  // It2MeNativeMessagingHost reads from input_read_file and writes to
  // output_write_file. The unittest supplies data to input_write_handle, and
  // verifies output from output_read_handle.
  //
  // unittest -> [input] -> It2MeNativeMessagingHost -> [output] -> unittest
  base::File input_write_file_;
  base::File output_read_file_;

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<base::RunLoop> test_run_loop_;

  std::unique_ptr<base::Thread> host_thread_;
  std::unique_ptr<base::RunLoop> host_run_loop_;

  std::unique_ptr<base::RunLoop> policy_run_loop_;

  // Retain a raw pointer to |policy_loader_| in order to control the policy
  // contents.
  raw_ptr<policy::FakeAsyncPolicyLoader, AcrossTasksDanglingUntriaged>
      policy_loader_ = nullptr;

  // Task runner of the host thread.
  scoped_refptr<AutoThreadTaskRunner> host_task_runner_;
  std::unique_ptr<remoting::NativeMessagingPipe> pipe_;

  scoped_refptr<network::TestSharedURLLoaderFactory> test_url_loader_factory_;
};

void It2MeNativeMessagingHostTest::SetUp() {
  task_environment_ = std::make_unique<base::test::TaskEnvironment>();
  test_run_loop_ = std::make_unique<base::RunLoop>();

  // Run the host on a dedicated thread.
  host_thread_ = std::make_unique<base::Thread>("host_thread");
  host_thread_->Start();

  host_task_runner_ = new AutoThreadTaskRunner(
      host_thread_->task_runner(),
      base::BindOnce(&It2MeNativeMessagingHostTest::ExitTest,
                     base::Unretained(this)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  test_url_loader_factory_ = new network::TestSharedURLLoaderFactory();
#endif

  host_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&It2MeNativeMessagingHostTest::StartHost,
                                base::Unretained(this)));

  // Wait until the host finishes starting.
  test_run_loop_->Run();
}

void It2MeNativeMessagingHostTest::TearDown() {
  // Release reference to AutoThreadTaskRunner, so the host thread can be shut
  // down.
  host_task_runner_ = nullptr;

  // Closing the write-end of the input will send an EOF to the native
  // messaging reader. This will trigger a host shutdown.
  input_write_file_.Close();

  // Start a new RunLoop and Wait until the host finishes shutting down.
  test_run_loop_ = std::make_unique<base::RunLoop>();
  test_run_loop_->Run();

  // Verify there are no more message in the output pipe.
  std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);

  // The It2MeNativeMessagingHost dtor closes the handles that are passed to it.
  // So the only handle left to close is |output_read_file_|.
  output_read_file_.Close();
}

void It2MeNativeMessagingHostTest::SetPolicies(base::Value::Dict dict) {
  DCHECK(task_environment_->GetMainThreadTaskRunner()
             ->RunsTasksInCurrentSequence());
  // Copy |dict| into |policy_bundle|.
  policy::PolicyNamespace policy_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
  policy::PolicyBundle policy_bundle;
  policy::PolicyMap& policy_map = policy_bundle.Get(policy_namespace);
  policy_map.LoadFrom(dict, policy::POLICY_LEVEL_MANDATORY,
                      policy::POLICY_SCOPE_MACHINE,
                      policy::POLICY_SOURCE_CLOUD);

  // Simulate a policy update and wait for it to complete.
  policy_run_loop_ = std::make_unique<base::RunLoop>();
  policy_loader_->SetPolicies(policy_bundle);
  policy_loader_->PostReloadOnBackgroundThread(true /* force reload asap */);
  policy_run_loop_->Run();
  policy_run_loop_.reset(nullptr);
}

std::optional<base::Value::Dict>
It2MeNativeMessagingHostTest::ReadMessageFromOutputPipe() {
  while (true) {
    uint32_t length;
    int read_result = output_read_file_.ReadAtCurrentPos(
        reinterpret_cast<char*>(&length), sizeof(length));
    if (read_result != sizeof(length)) {
      // The output pipe has been closed, return an empty message.
      return std::nullopt;
    }

    std::string message_json(length, '\0');
    read_result =
        output_read_file_.ReadAtCurrentPos(std::data(message_json), length);
    if (read_result != static_cast<int>(length)) {
      LOG(ERROR) << "Message size (" << read_result
                 << ") doesn't match the header (" << length << ").";
      return std::nullopt;
    }

    std::optional<base::Value> message = base::JSONReader::Read(message_json);
    if (!message || !message->is_dict()) {
      LOG(ERROR) << "Malformed message:" << message_json;
      return std::nullopt;
    }

    base::Value::Dict result = std::move(*message).TakeDict();
    // If this is a debug message log, ignore it, otherwise return it.
    const std::string* type = result.FindString(kMessageType);
    if (!type || *type != LogMessageHandler::kDebugMessageTypeName) {
      return result;
    }
  }
}

void It2MeNativeMessagingHostTest::WriteMessageToInputPipe(
    const base::Value::Dict& message) {
  std::string message_json;
  base::JSONWriter::Write(message, &message_json);

  uint32_t length = base::checked_cast<uint32_t>(message_json.length());
  input_write_file_.WriteAtCurrentPos(base::byte_span_from_ref(length));
  input_write_file_.WriteAtCurrentPos(base::as_byte_span(message_json));
}

void It2MeNativeMessagingHostTest::VerifyHelloResponse(int request_id) {
  std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
  ASSERT_TRUE(response);
  VerifyCommonProperties(*response, kHelloResponse, request_id);
}

void It2MeNativeMessagingHostTest::VerifyErrorResponse() {
  std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
  ASSERT_TRUE(response);
  VerifyStringProperty(*response, kMessageType, kErrorMessage);
}

void It2MeNativeMessagingHostTest::VerifyConnectResponses(int request_id) {
  bool connect_response_received = false;
  bool nat_policy_received = false;
  bool starting_received = false;
  bool requestedAccessCode_received = false;
  bool receivedAccessCode_received = false;
  bool connecting_received = false;
  bool connected_received = false;

  // We expect a total of 7 messages: 1 connectResponse, 1 natPolicyChanged,
  // and 5 hostStateChanged.
  for (int i = 0; i < 7; ++i) {
    std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
    ASSERT_TRUE(response);

    const std::string* type = response->FindString(kMessageType);
    ASSERT_TRUE(type);

    if (*type == kConnectResponse) {
      EXPECT_FALSE(connect_response_received);
      connect_response_received = true;
      VerifyId(*response, request_id);
    } else if (*type == kNatPolicyChangedMessage) {
      EXPECT_FALSE(nat_policy_received);
      nat_policy_received = true;
    } else if (*type == kHostStateChangedMessage) {
      const std::string* state = response->FindString(kState);
      ASSERT_TRUE(state);

      if (*state == It2MeHostStateToString(It2MeHostState::kStarting)) {
        EXPECT_FALSE(starting_received);
        starting_received = true;
      } else if (*state ==
                 It2MeHostStateToString(It2MeHostState::kRequestedAccessCode)) {
        EXPECT_FALSE(requestedAccessCode_received);
        requestedAccessCode_received = true;
      } else if (*state ==
                 It2MeHostStateToString(It2MeHostState::kReceivedAccessCode)) {
        EXPECT_FALSE(receivedAccessCode_received);
        receivedAccessCode_received = true;

        const std::string* value = response->FindString(kAccessCode);
        ASSERT_TRUE(value);
        EXPECT_EQ(kTestAccessCode, *value);

        std::optional<int> access_code_lifetime =
            response->FindInt(kAccessCodeLifetime);
        ASSERT_TRUE(access_code_lifetime);
        EXPECT_EQ(kTestAccessCodeLifetime.InSeconds(), *access_code_lifetime);
      } else if (*state ==
                 It2MeHostStateToString(It2MeHostState::kConnecting)) {
        EXPECT_FALSE(connecting_received);
        connecting_received = true;
      } else if (*state == It2MeHostStateToString(It2MeHostState::kConnected)) {
        EXPECT_FALSE(connected_received);
        connected_received = true;

        const std::string* value = response->FindString(kClient);
        ASSERT_TRUE(value);
        EXPECT_EQ(kTestClientUsername, *value);
      } else {
        ADD_FAILURE() << "Unexpected host state: " << state;
      }
    } else {
      ADD_FAILURE() << "Unexpected message type: " << type;
    }
  }
}

void It2MeNativeMessagingHostTest::VerifyDisconnectResponses(int request_id) {
  bool disconnect_response_received = false;
  bool disconnected_received = false;

  // We expect a total of 2 messages: disconnectResponse and hostStateChanged.
  for (int i = 0; i < 2; i++) {
    std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
    ASSERT_TRUE(response);

    const std::string* type = response->FindString(kMessageType);
    ASSERT_TRUE(type);

    if (*type == kDisconnectResponse) {
      EXPECT_FALSE(disconnect_response_received);
      disconnect_response_received = true;
      VerifyId(*response, request_id);
    } else if (*type == kHostStateChangedMessage) {
      const std::string* state = response->FindString(kState);
      ASSERT_TRUE(state);
      if (*state == It2MeHostStateToString(It2MeHostState::kDisconnected)) {
        EXPECT_FALSE(disconnected_received);
        disconnected_received = true;
        const std::string* error_code = response->FindString(kDisconnectReason);
        ASSERT_TRUE(error_code);
        EXPECT_EQ(ErrorCodeToString(protocol::ErrorCode::OK), *error_code);
      } else {
        ADD_FAILURE() << "Unexpected host state: " << state;
      }
    } else {
      ADD_FAILURE() << "Unexpected message type: " << type;
    }
  }
}

void It2MeNativeMessagingHostTest::VerifyPolicyErrorResponse() {
  std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
  ASSERT_TRUE(response);
  const std::string* type = response->FindString(kMessageType);
  ASSERT_TRUE(type);
  ASSERT_EQ(kPolicyErrorMessage, *type);
}

void It2MeNativeMessagingHostTest::TestBadRequest(
    const base::Value::Dict& message,
    bool expect_error_response) {
  base::Value::Dict good_message;
  good_message.Set(kMessageType, kHelloMessage);
  good_message.Set(kMessageId, 1);

  WriteMessageToInputPipe(good_message);
  WriteMessageToInputPipe(message);
  WriteMessageToInputPipe(good_message);

  VerifyHelloResponse(1);

  if (expect_error_response) {
    VerifyErrorResponse();
  }

  std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);
}

void It2MeNativeMessagingHostTest::StartHost() {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());

  base::File input_read_file;
  base::File output_write_file;

  ASSERT_TRUE(MakePipe(&input_read_file, &input_write_file_));
  ASSERT_TRUE(MakePipe(&output_read_file_, &output_write_file));

  pipe_ = std::make_unique<NativeMessagingPipe>();

  std::unique_ptr<extensions::NativeMessagingChannel> channel(
      new PipeMessagingChannel(std::move(input_read_file),
                               std::move(output_write_file)));

  // Creating a native messaging host with a mock It2MeHostFactory and policy
  // loader.
  std::unique_ptr<ChromotingHostContext> context =
      ChromotingHostContext::CreateForTesting(host_task_runner_,
                                              test_url_loader_factory_);
  auto policy_loader =
      std::make_unique<policy::FakeAsyncPolicyLoader>(host_task_runner_);
  policy_loader_ = policy_loader.get();
  std::unique_ptr<PolicyWatcher> policy_watcher =
      PolicyWatcher::CreateFromPolicyLoaderForTesting(std::move(policy_loader));
  auto factory = std::make_unique<MockIt2MeHostFactory>();
  factory_raw_ptr_ = factory.get();
  std::unique_ptr<It2MeNativeMessagingHost> it2me_host(
      new It2MeNativeMessagingHost(
          /*needs_elevation=*/false, std::move(policy_watcher),
          std::move(context), std::move(factory)));
  it2me_host->SetPolicyErrorClosureForTesting(base::BindOnce(
      base::IgnoreResult(&base::TaskRunner::PostTask),
      task_environment_->GetMainThreadTaskRunner(), FROM_HERE,
      base::BindOnce(&It2MeNativeMessagingHostTest::ExitPolicyRunLoop,
                     base::Unretained(this))));
  it2me_host->Start(pipe_.get());

  pipe_->Start(std::move(it2me_host), std::move(channel));

  // Notify the test that the host has finished starting up.
  test_run_loop_->Quit();
}

void It2MeNativeMessagingHostTest::ExitTest() {
  if (!task_environment_->GetMainThreadTaskRunner()
           ->RunsTasksInCurrentSequence()) {
    task_environment_->GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&It2MeNativeMessagingHostTest::ExitTest,
                                  base::Unretained(this)));
    return;
  }
  test_run_loop_->Quit();
}

void It2MeNativeMessagingHostTest::ExitPolicyRunLoop() {
  DCHECK(task_environment_->GetMainThreadTaskRunner()
             ->RunsTasksInCurrentSequence());
  if (policy_run_loop_) {
    policy_run_loop_->Quit();
  }
}

void It2MeNativeMessagingHostTest::TestConnect() {
  int next_id = 1;
  WriteMessageToInputPipe(CreateConnectMessage(next_id));
  VerifyConnectResponses(next_id);
  ++next_id;
  WriteMessageToInputPipe(CreateDisconnectMessage(next_id));
  VerifyDisconnectResponses(next_id);
}

// Test hello request.
TEST_F(It2MeNativeMessagingHostTest, Hello) {
  int next_id = 0;
  base::Value::Dict message;
  message.Set(kMessageId, ++next_id);
  message.Set(kMessageType, kHelloMessage);
  WriteMessageToInputPipe(message);

  VerifyHelloResponse(next_id);
}

// Verify that response ID matches request ID.
TEST_F(It2MeNativeMessagingHostTest, Id) {
  base::Value::Dict message;
  message.Set(kMessageType, kHelloMessage);
  WriteMessageToInputPipe(message);
  message.Set(kMessageId, "42");
  WriteMessageToInputPipe(message);

  std::optional<base::Value::Dict> response = ReadMessageFromOutputPipe();
  ASSERT_TRUE(response);
  const std::string* value = response->FindString(kMessageId);
  EXPECT_FALSE(value);

  response = ReadMessageFromOutputPipe();
  ASSERT_TRUE(response);
  value = response->FindString(kMessageId);
  ASSERT_TRUE(value);
  EXPECT_EQ("42", *value);
}

TEST_F(It2MeNativeMessagingHostTest, ConnectMultiple) {
  // A new It2MeHost instance is created for every it2me session. The native
  // messaging host, on the other hand, is long lived. This test verifies
  // multiple It2Me host startup and shutdowns.
  for (int i = 0; i < 3; ++i) {
    TestConnect();
  }
}

TEST_F(It2MeNativeMessagingHostTest,
       ConnectRespectsSuppressUserDialogsParameterOnChromeOsOnly) {
  int next_id = 1;
  base::Value::Dict connect_message = CreateConnectMessage(next_id);
  connect_message.Set(kIsEnterpriseAdminUser, true);
  connect_message.Set(kSuppressUserDialogs, true);
  WriteMessageToInputPipe(connect_message);
  VerifyConnectResponses(next_id);
#if BUILDFLAG(IS_CHROMEOS_ASH) || !defined(NDEBUG)
  ASSERT_TRUE(get_chrome_os_enterprise_params().has_value());
  ASSERT_TRUE(get_chrome_os_enterprise_params()->suppress_user_dialogs);
#else
  ASSERT_FALSE(get_chrome_os_enterprise_params().has_value());
#endif
  ++next_id;
  WriteMessageToInputPipe(CreateDisconnectMessage(next_id));
  VerifyDisconnectResponses(next_id);
}

TEST_F(It2MeNativeMessagingHostTest,
       ConnectRespectsSuppressNotificationsParameterOnChromeOsOnly) {
  int next_id = 1;
  base::Value::Dict connect_message = CreateConnectMessage(next_id);
  connect_message.Set(kIsEnterpriseAdminUser, true);
  connect_message.Set(kSuppressNotifications, true);
  WriteMessageToInputPipe(connect_message);
  VerifyConnectResponses(next_id);
#if BUILDFLAG(IS_CHROMEOS_ASH) || !defined(NDEBUG)
  ASSERT_TRUE(get_chrome_os_enterprise_params().has_value());
  ASSERT_TRUE(get_chrome_os_enterprise_params()->suppress_notifications);
#else
  ASSERT_FALSE(get_chrome_os_enterprise_params().has_value());
#endif
  ++next_id;
  WriteMessageToInputPipe(CreateDisconnectMessage(next_id));
  VerifyDisconnectResponses(next_id);
}

TEST_F(It2MeNativeMessagingHostTest,
       ConnectRespectsTerminateUponInputParameterOnChromeOsOnly) {
  int next_id = 1;
  base::Value::Dict connect_message = CreateConnectMessage(next_id);
  connect_message.Set(kIsEnterpriseAdminUser, true);
  connect_message.Set(kTerminateUponInput, true);
  WriteMessageToInputPipe(connect_message);
  VerifyConnectResponses(next_id);
#if BUILDFLAG(IS_CHROMEOS_ASH) || !defined(NDEBUG)
  ASSERT_TRUE(get_chrome_os_enterprise_params().has_value());
  ASSERT_TRUE(get_chrome_os_enterprise_params()->terminate_upon_input);
#else
  ASSERT_FALSE(get_chrome_os_enterprise_params().has_value());
#endif
  ++next_id;
  WriteMessageToInputPipe(CreateDisconnectMessage(next_id));
  VerifyDisconnectResponses(next_id);
}

TEST_F(It2MeNativeMessagingHostTest,
       ConnectRespectsIsEnterpriseAdminUserParameterOnChromeOsOnly) {
  int next_id = 1;
  base::Value::Dict connect_message = CreateConnectMessage(next_id);
  connect_message.Set(kIsEnterpriseAdminUser, true);
  WriteMessageToInputPipe(connect_message);
  VerifyConnectResponses(next_id);
#if BUILDFLAG(IS_CHROMEOS_ASH) || !defined(NDEBUG)
  EXPECT_TRUE(factory_raw_ptr_->host->is_enterprise_session());
#else
  EXPECT_FALSE(factory_raw_ptr_->host->is_enterprise_session());
#endif
  ++next_id;
  WriteMessageToInputPipe(CreateDisconnectMessage(next_id));
  VerifyDisconnectResponses(next_id);
}

TEST_F(It2MeNativeMessagingHostTest,
       ConnectRespectsCurtainLocalUserSessionParameterOnChromeOsOnly) {
  int next_id = 1;
  base::Value::Dict connect_message = CreateConnectMessage(next_id);
  connect_message.Set(kIsEnterpriseAdminUser, true);
  connect_message.Set(kCurtainLocalUserSession, true);
  WriteMessageToInputPipe(connect_message);
  VerifyConnectResponses(next_id);
#if BUILDFLAG(IS_CHROMEOS_ASH) || !defined(NDEBUG)
  ASSERT_TRUE(get_chrome_os_enterprise_params().has_value());
  ASSERT_TRUE(get_chrome_os_enterprise_params()->curtain_local_user_session);
#else
  ASSERT_FALSE(get_chrome_os_enterprise_params().has_value());
#endif
  ++next_id;
  WriteMessageToInputPipe(CreateDisconnectMessage(next_id));
  VerifyDisconnectResponses(next_id);
}

// Verify requests with no type are rejected.
TEST_F(It2MeNativeMessagingHostTest, MissingType) {
  base::Value::Dict message;
  TestBadRequest(message, true);
}

// Verify rejection if type is unrecognized.
TEST_F(It2MeNativeMessagingHostTest, InvalidType) {
  base::Value::Dict message;
  message.Set(kMessageType, "xxx");
  TestBadRequest(message, true);
}

// Verify rejection if type is unrecognized.
TEST_F(It2MeNativeMessagingHostTest, BadPoliciesBeforeConnect) {
  base::Value::Dict bad_policy;
  bad_policy.Set(policy::key::kRemoteAccessHostFirewallTraversal, 1);
  SetPolicies(std::move(bad_policy));
  WriteMessageToInputPipe(CreateConnectMessage(1));
  VerifyPolicyErrorResponse();
}

// Verify rejection if type is unrecognized.
TEST_F(It2MeNativeMessagingHostTest, BadPoliciesAfterConnect) {
  base::Value::Dict bad_policy;
  bad_policy.Set(policy::key::kRemoteAccessHostFirewallTraversal, 1);
  WriteMessageToInputPipe(CreateConnectMessage(1));
  VerifyConnectResponses(1);
  SetPolicies(std::move(bad_policy));
  VerifyPolicyErrorResponse();
}

}  // namespace remoting
