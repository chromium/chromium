// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_native_messaging_host.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringize_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/policy/core/common/fake_async_policy_loader.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/policy_constants.h"
#include "net/base/file_stream.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/native_messaging/log_message_handler.h"
#include "remoting/host/native_messaging/native_messaging_pipe.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/host/setup/test_util.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/ice_config.h"
#include "remoting/signaling/log_to_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using protocol::ErrorCode;

namespace {

const char kTestAccessCode[] = "888888";
constexpr base::TimeDelta kTestAccessCodeLifetime =
    base::TimeDelta::FromSeconds(666);
const char kTestClientUsername[] = "some_user@gmail.com";
const char kTestStunServer[] = "test_relay_server.com";

void VerifyId(std::unique_ptr<base::DictionaryValue> response,
              int expected_value) {
  ASSERT_TRUE(response);

  int value;
  EXPECT_TRUE(response->GetInteger("id", &value));
  EXPECT_EQ(expected_value, value);
}

void VerifyStringProperty(std::unique_ptr<base::DictionaryValue> response,
                          const std::string& name,
                          const std::string& expected_value) {
  ASSERT_TRUE(response);

  std::string value;
  EXPECT_TRUE(response->GetString(name, &value));
  EXPECT_EQ(expected_value, value);
}

// Verity the values of the "type" and "id" properties
void VerifyCommonProperties(std::unique_ptr<base::DictionaryValue> response,
                            const std::string& type,
                            int id) {
  ASSERT_TRUE(response);

  std::string string_value;
  EXPECT_TRUE(response->GetString("type", &string_value));
  EXPECT_EQ(type, string_value);

  int int_value;
  EXPECT_TRUE(response->GetInteger("id", &int_value));
  EXPECT_EQ(id, int_value);
}

base::DictionaryValue CreateConnectMessage(int id) {
  base::DictionaryValue connect_message;
  connect_message.SetInteger("id", id);
  connect_message.SetString("type", "connect");
  connect_message.SetString("xmppServerAddress", "talk.google.com:5222");
  connect_message.SetBoolean("xmppServerUseTls", true);
  connect_message.SetString("userName", kTestClientUsername);
  connect_message.SetString("authServiceWithToken", "oauth2:sometoken");
  connect_message.Set("iceConfig",
                      base::JSONReader::ReadDeprecated(
                          "{ \"iceServers\": [ { \"urls\": [ \"stun:" +
                          std::string(kTestStunServer) + "\" ] } ] }"));

  return connect_message;
}

base::DictionaryValue CreateDisconnectMessage(int id) {
  base::DictionaryValue disconnect_message;
  disconnect_message.SetInteger("id", id);
  disconnect_message.SetString("type", "disconnect");
  return disconnect_message;
}

}  // namespace

class MockIt2MeHost : public It2MeHost {
 public:
  MockIt2MeHost() = default;

  // It2MeHost overrides
  void Connect(std::unique_ptr<ChromotingHostContext> context,
               std::unique_ptr<base::DictionaryValue> policies,
               std::unique_ptr<It2MeConfirmationDialogFactory> dialog_factory,
               std::unique_ptr<RegisterSupportHostRequest> register_request,
               std::unique_ptr<LogToServer> log_to_server,
               base::WeakPtr<It2MeHost::Observer> observer,
               std::unique_ptr<SignalStrategy> signal_strategy,
               const std::string& username,
               const protocol::IceConfig& ice_config) override;
  void Disconnect() override;

 private:
  ~MockIt2MeHost() override = default;

  void RunSetState(It2MeHostState state);

  DISALLOW_COPY_AND_ASSIGN(MockIt2MeHost);
};

void MockIt2MeHost::Connect(
    std::unique_ptr<ChromotingHostContext> context,
    std::unique_ptr<base::DictionaryValue> policies,
    std::unique_ptr<It2MeConfirmationDialogFactory> dialog_factory,
    std::unique_ptr<RegisterSupportHostRequest> register_request,
    std::unique_ptr<LogToServer> log_to_server,
    base::WeakPtr<It2MeHost::Observer> observer,
    std::unique_ptr<SignalStrategy> signal_strategy,
    const std::string& username,
    const protocol::IceConfig& ice_config) {
  DCHECK(context->ui_task_runner()->BelongsToCurrentThread());

  // Verify that parameters are passed correctly.
  EXPECT_EQ(username, kTestClientUsername);
  EXPECT_EQ(ice_config.stun_servers[0].hostname(), kTestStunServer);

  host_context_ = std::move(context);
  observer_ = std::move(observer);
  signal_strategy_ = std::move(signal_strategy);
  register_request_ = std::move(register_request);

  OnPolicyUpdate(std::move(policies));

  RunSetState(kStarting);
  RunSetState(kRequestedAccessCode);

  host_context()->ui_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&It2MeHost::Observer::OnStoreAccessCode, observer_,
                     kTestAccessCode, kTestAccessCodeLifetime));

  RunSetState(kReceivedAccessCode);
  RunSetState(kConnecting);

  host_context()->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&It2MeHost::Observer::OnClientAuthenticated,
                                observer_, kTestClientUsername));

  RunSetState(kConnected);
}

void MockIt2MeHost::Disconnect() {
  if (!host_context()->network_task_runner()->BelongsToCurrentThread()) {
    DCHECK(host_context()->ui_task_runner()->BelongsToCurrentThread());
    host_context()->network_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&MockIt2MeHost::Disconnect, this));
    return;
  }

  RunSetState(kDisconnected);
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
  ~MockIt2MeHostFactory() override = default;

  scoped_refptr<It2MeHost> CreateIt2MeHost() override {
    return host;
  }

  scoped_refptr<MockIt2MeHost> host;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIt2MeHostFactory);
};

class It2MeNativeMessagingHostTest : public testing::Test {
 public:
  It2MeNativeMessagingHostTest() = default;
  ~It2MeNativeMessagingHostTest() override = default;

  void SetUp() override;
  void TearDown() override;

 protected:
  void SetPolicies(const base::DictionaryValue& dict);
  std::unique_ptr<base::DictionaryValue> ReadMessageFromOutputPipe();
  void WriteMessageToInputPipe(const base::Value& message);

  void VerifyHelloResponse(int request_id);
  void VerifyErrorResponse();
  void VerifyConnectResponses(int request_id);
  void VerifyDisconnectResponses(int request_id);
  void VerifyPolicyErrorResponse();

  // The Host process should shut down when it receives a malformed request.
  // This is tested by sending a known-good request, followed by |message|,
  // followed by the known-good request again. The response file should only
  // contain a single response from the first good request.
  void TestBadRequest(const base::Value& message, bool expect_error_response);
  void TestConnect();

  // Raw pointer to host factory (owned by It2MeNativeMessagingHost).
  MockIt2MeHostFactory* factory_raw_ptr_ = nullptr;

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

  // Message loop of the test thread.
  std::unique_ptr<base::MessageLoop> test_message_loop_;
  std::unique_ptr<base::RunLoop> test_run_loop_;

  std::unique_ptr<base::Thread> host_thread_;
  std::unique_ptr<base::RunLoop> host_run_loop_;

  std::unique_ptr<base::RunLoop> policy_run_loop_;

  // Retain a raw pointer to |policy_loader_| in order to control the policy
  // contents.
  policy::FakeAsyncPolicyLoader* policy_loader_ = nullptr;

  // Task runner of the host thread.
  scoped_refptr<AutoThreadTaskRunner> host_task_runner_;
  std::unique_ptr<remoting::NativeMessagingPipe> pipe_;

  DISALLOW_COPY_AND_ASSIGN(It2MeNativeMessagingHostTest);
};

void It2MeNativeMessagingHostTest::SetUp() {
  test_message_loop_.reset(new base::MessageLoop());
  test_run_loop_.reset(new base::RunLoop());

  // Run the host on a dedicated thread.
  host_thread_.reset(new base::Thread("host_thread"));
  host_thread_->Start();

  host_task_runner_ = new AutoThreadTaskRunner(
      host_thread_->task_runner(),
      base::Bind(&It2MeNativeMessagingHostTest::ExitTest,
                 base::Unretained(this)));

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
  test_run_loop_.reset(new base::RunLoop());
  test_run_loop_->Run();

  // Verify there are no more message in the output pipe.
  std::unique_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);

  // The It2MeNativeMessagingHost dtor closes the handles that are passed to it.
  // So the only handle left to close is |output_read_file_|.
  output_read_file_.Close();
}

void It2MeNativeMessagingHostTest::SetPolicies(
    const base::DictionaryValue& dict) {
  DCHECK(test_message_loop_->task_runner()->RunsTasksInCurrentSequence());
  // Copy |dict| into |policy_bundle|.
  policy::PolicyNamespace policy_namespace =
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
  policy::PolicyBundle policy_bundle;
  policy::PolicyMap& policy_map = policy_bundle.Get(policy_namespace);
  policy_map.LoadFrom(&dict, policy::POLICY_LEVEL_MANDATORY,
                      policy::POLICY_SCOPE_MACHINE,
                      policy::POLICY_SOURCE_CLOUD);

  // Simulate a policy update and wait for it to complete.
  policy_run_loop_.reset(new base::RunLoop);
  policy_loader_->SetPolicies(policy_bundle);
  policy_loader_->PostReloadOnBackgroundThread(true /* force reload asap */);
  policy_run_loop_->Run();
  policy_run_loop_.reset(nullptr);
}

std::unique_ptr<base::DictionaryValue>
It2MeNativeMessagingHostTest::ReadMessageFromOutputPipe() {
  while (true) {
    uint32_t length;
    int read_result = output_read_file_.ReadAtCurrentPos(
        reinterpret_cast<char*>(&length), sizeof(length));
    if (read_result != sizeof(length)) {
      // The output pipe has been closed, return an empty message.
      return nullptr;
    }

    std::string message_json(length, '\0');
    read_result =
        output_read_file_.ReadAtCurrentPos(base::data(message_json), length);
    if (read_result != static_cast<int>(length)) {
      LOG(ERROR) << "Message size (" << read_result
                 << ") doesn't match the header (" << length << ").";
      return nullptr;
    }

    std::unique_ptr<base::Value> message =
        base::JSONReader::ReadDeprecated(message_json);
    if (!message || !message->is_dict()) {
      LOG(ERROR) << "Malformed message:" << message_json;
      return nullptr;
    }

    std::unique_ptr<base::DictionaryValue> result = base::WrapUnique(
        static_cast<base::DictionaryValue*>(message.release()));
    std::string type;
    // If this is a debug message log, ignore it, otherwise return it.
    if (!result->GetString("type", &type) ||
        type != LogMessageHandler::kDebugMessageTypeName) {
      return result;
    }
  }
}

void It2MeNativeMessagingHostTest::WriteMessageToInputPipe(
    const base::Value& message) {
  std::string message_json;
  base::JSONWriter::Write(message, &message_json);

  uint32_t length = message_json.length();
  input_write_file_.WriteAtCurrentPos(reinterpret_cast<char*>(&length),
                                      sizeof(length));
  input_write_file_.WriteAtCurrentPos(message_json.data(), length);
}

void It2MeNativeMessagingHostTest::VerifyHelloResponse(int request_id) {
  std::unique_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  VerifyCommonProperties(std::move(response), "helloResponse", request_id);
}

void It2MeNativeMessagingHostTest::VerifyErrorResponse() {
  std::unique_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  VerifyStringProperty(std::move(response), "type", "error");
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
    std::unique_ptr<base::DictionaryValue> response =
        ReadMessageFromOutputPipe();
    ASSERT_TRUE(response);

    std::string type;
    ASSERT_TRUE(response->GetString("type", &type));

    if (type == "connectResponse") {
      EXPECT_FALSE(connect_response_received);
      connect_response_received = true;
      VerifyId(std::move(response), request_id);
    } else if (type == "natPolicyChanged") {
      EXPECT_FALSE(nat_policy_received);
      nat_policy_received = true;
    } else if (type == "hostStateChanged") {
      std::string state;
      ASSERT_TRUE(response->GetString("state", &state));

      std::string value;
      if (state == It2MeNativeMessagingHost::HostStateToString(kStarting)) {
        EXPECT_FALSE(starting_received);
        starting_received = true;
      } else if (state == It2MeNativeMessagingHost::HostStateToString(
                              kRequestedAccessCode)) {
        EXPECT_FALSE(requestedAccessCode_received);
        requestedAccessCode_received = true;
      } else if (state == It2MeNativeMessagingHost::HostStateToString(
                              kReceivedAccessCode)) {
        EXPECT_FALSE(receivedAccessCode_received);
        receivedAccessCode_received = true;

        EXPECT_TRUE(response->GetString("accessCode", &value));
        EXPECT_EQ(kTestAccessCode, value);

        int access_code_lifetime;
        EXPECT_TRUE(
            response->GetInteger("accessCodeLifetime", &access_code_lifetime));
        EXPECT_EQ(kTestAccessCodeLifetime.InSeconds(), access_code_lifetime);
      } else if (state ==
                 It2MeNativeMessagingHost::HostStateToString(kConnecting)) {
        EXPECT_FALSE(connecting_received);
        connecting_received = true;
      } else if (state ==
                 It2MeNativeMessagingHost::HostStateToString(kConnected)) {
        EXPECT_FALSE(connected_received);
        connected_received = true;

        EXPECT_TRUE(response->GetString("client", &value));
        EXPECT_EQ(kTestClientUsername, value);
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

  // We expect a total of 3 messages: 1 connectResponse and 1 hostStateChanged.
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<base::DictionaryValue> response =
        ReadMessageFromOutputPipe();
    ASSERT_TRUE(response);

    std::string type;
    ASSERT_TRUE(response->GetString("type", &type));

    if (type == "disconnectResponse") {
      EXPECT_FALSE(disconnect_response_received);
      disconnect_response_received = true;
      VerifyId(std::move(response), request_id);
    } else if (type == "hostStateChanged") {
      std::string state;
      ASSERT_TRUE(response->GetString("state", &state));
      if (state == It2MeNativeMessagingHost::HostStateToString(kDisconnected)) {
        EXPECT_FALSE(disconnected_received);
        disconnected_received = true;
      } else {
        ADD_FAILURE() << "Unexpected host state: " << state;
      }
    } else {
      ADD_FAILURE() << "Unexpected message type: " << type;
    }
  }
}

void It2MeNativeMessagingHostTest::VerifyPolicyErrorResponse() {
  std::unique_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  ASSERT_TRUE(response);
  std::string type;
  ASSERT_TRUE(response->GetString("type", &type));
  ASSERT_EQ("policyError", type);
}

void It2MeNativeMessagingHostTest::TestBadRequest(const base::Value& message,
                                                  bool expect_error_response) {
  base::DictionaryValue good_message;
  good_message.SetString("type", "hello");
  good_message.SetInteger("id", 1);

  WriteMessageToInputPipe(good_message);
  WriteMessageToInputPipe(message);
  WriteMessageToInputPipe(good_message);

  VerifyHelloResponse(1);

  if (expect_error_response) {
    VerifyErrorResponse();
  }

  std::unique_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  EXPECT_FALSE(response);
}

void It2MeNativeMessagingHostTest::StartHost() {
  DCHECK(host_task_runner_->RunsTasksInCurrentSequence());

  base::File input_read_file;
  base::File output_write_file;

  ASSERT_TRUE(MakePipe(&input_read_file, &input_write_file_));
  ASSERT_TRUE(MakePipe(&output_read_file_, &output_write_file));

  pipe_.reset(new NativeMessagingPipe());

  std::unique_ptr<extensions::NativeMessagingChannel> channel(
      new PipeMessagingChannel(std::move(input_read_file),
                               std::move(output_write_file)));

  // Creating a native messaging host with a mock It2MeHostFactory and policy
  // loader.
  std::unique_ptr<ChromotingHostContext> context =
      ChromotingHostContext::Create(host_task_runner_);
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
  it2me_host->SetPolicyErrorClosureForTesting(
      base::Bind(base::IgnoreResult(&base::TaskRunner::PostTask),
                 test_message_loop_->task_runner(), FROM_HERE,
                 base::Bind(&It2MeNativeMessagingHostTest::ExitPolicyRunLoop,
                            base::Unretained(this))));
  it2me_host->Start(pipe_.get());

  pipe_->Start(std::move(it2me_host), std::move(channel));

  // Notify the test that the host has finished starting up.
  test_run_loop_->Quit();
}

void It2MeNativeMessagingHostTest::ExitTest() {
  if (!test_message_loop_->task_runner()->RunsTasksInCurrentSequence()) {
    test_message_loop_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&It2MeNativeMessagingHostTest::ExitTest,
                                  base::Unretained(this)));
    return;
  }
  test_run_loop_->Quit();
}

void It2MeNativeMessagingHostTest::ExitPolicyRunLoop() {
  DCHECK(test_message_loop_->task_runner()->RunsTasksInCurrentSequence());
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
  base::DictionaryValue message;
  message.SetInteger("id", ++next_id);
  message.SetString("type", "hello");
  WriteMessageToInputPipe(message);

  VerifyHelloResponse(next_id);
}

// Verify that response ID matches request ID.
TEST_F(It2MeNativeMessagingHostTest, Id) {
  base::DictionaryValue message;
  message.SetString("type", "hello");
  WriteMessageToInputPipe(message);
  message.SetString("id", "42");
  WriteMessageToInputPipe(message);

  std::unique_ptr<base::DictionaryValue> response = ReadMessageFromOutputPipe();
  EXPECT_TRUE(response);
  std::string value;
  EXPECT_FALSE(response->GetString("id", &value));

  response = ReadMessageFromOutputPipe();
  EXPECT_TRUE(response);
  EXPECT_TRUE(response->GetString("id", &value));
  EXPECT_EQ("42", value);
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
       ConnectRespectsNoDialogsParameterOnChromeOsOnly) {
  int next_id = 1;
  base::DictionaryValue connect_message = CreateConnectMessage(next_id);
  connect_message.SetBoolean("noDialogs", true);
  WriteMessageToInputPipe(connect_message);
  VerifyConnectResponses(next_id);
#if defined(OS_CHROMEOS) || !defined(NDEBUG)
  EXPECT_FALSE(factory_raw_ptr_->host->enable_dialogs());
#else
  EXPECT_TRUE(factory_raw_ptr_->host->enable_dialogs());
#endif
  ++next_id;
  WriteMessageToInputPipe(CreateDisconnectMessage(next_id));
  VerifyDisconnectResponses(next_id);
}


// Verify non-Dictionary requests are rejected.
TEST_F(It2MeNativeMessagingHostTest, WrongFormat) {
  base::ListValue message;
  // No "error" response will be sent for non-Dictionary messages.
  TestBadRequest(message, false);
}

// Verify requests with no type are rejected.
TEST_F(It2MeNativeMessagingHostTest, MissingType) {
  base::DictionaryValue message;
  TestBadRequest(message, true);
}

// Verify rejection if type is unrecognized.
TEST_F(It2MeNativeMessagingHostTest, InvalidType) {
  base::DictionaryValue message;
  message.SetString("type", "xxx");
  TestBadRequest(message, true);
}

// Verify rejection if type is unrecognized.
TEST_F(It2MeNativeMessagingHostTest, BadPoliciesBeforeConnect) {
  base::DictionaryValue bad_policy;
  bad_policy.SetInteger(policy::key::kRemoteAccessHostFirewallTraversal, 1);
  SetPolicies(bad_policy);
  WriteMessageToInputPipe(CreateConnectMessage(1));
  VerifyPolicyErrorResponse();
}

// Verify rejection if type is unrecognized.
TEST_F(It2MeNativeMessagingHostTest, BadPoliciesAfterConnect) {
  base::DictionaryValue bad_policy;
  bad_policy.SetInteger(policy::key::kRemoteAccessHostFirewallTraversal, 1);
  WriteMessageToInputPipe(CreateConnectMessage(1));
  VerifyConnectResponses(1);
  SetPolicies(bad_policy);
  VerifyPolicyErrorResponse();
}

}  // namespace remoting
