// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/mcs_client.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "google_apis/gcm/base/fake_encryptor.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/fake_connection_factory.h"
#include "google_apis/gcm/engine/fake_connection_handler.h"
#include "google_apis/gcm/engine/gcm_store_impl.h"
#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const uint64_t kAndroidId = 54321;
const uint64_t kSecurityToken = 12345;

// Number of messages to send when testing batching.
// Note: must be even for tests that split batches in half.
const int kMessageBatchSize = 6;

// The number of unacked messages the client will receive before sending a
// stream ack.
// TODO(zea): get this (and other constants) directly from the mcs client.
const int kAckLimitSize = 10;

// TTL value for reliable messages.
const int kTTLValue = 5 * 60;  // 5 minutes.

// Specifies whether immediate ACK should be requested.
enum RequestImmediateAck {
  IMMEDIATE_ACK_IGNORE,  // Ignores the field and does not set it.
  IMMEDIATE_ACK_NO,      // Sets the field to false.
  IMMEDIATE_ACK_YES      // Sets the field to true.
};

// Helper for building arbitrary data messages.
MCSMessage BuildDataMessage(const std::string& from,
                            const std::string& category,
                            const std::string& message_id,
                            int last_stream_id_received,
                            const std::string& persistent_id,
                            int ttl,
                            uint64_t sent,
                            int queued,
                            const std::string& token,
                            const uint64_t& user_id,
                            RequestImmediateAck immediate_ack) {
  mcs_proto::DataMessageStanza data_message;
  data_message.set_id(message_id);
  data_message.set_from(from);
  data_message.set_category(category);
  data_message.set_last_stream_id_received(last_stream_id_received);
  if (!persistent_id.empty())
    data_message.set_persistent_id(persistent_id);
  data_message.set_ttl(ttl);
  data_message.set_sent(sent);
  data_message.set_queued(queued);
  data_message.set_token(token);
  data_message.set_device_user_id(user_id);
  if (immediate_ack != IMMEDIATE_ACK_IGNORE) {
    data_message.set_immediate_ack(immediate_ack == IMMEDIATE_ACK_YES);
  }
  return MCSMessage(kDataMessageStanzaTag, data_message);
}

// MCSClient with overriden exposed persistent id logic.
class TestMCSClient : public MCSClient {
 public:
  TestMCSClient(base::Clock* clock,
                ConnectionFactory* connection_factory,
                GCMStore* gcm_store,
                scoped_refptr<base::SequencedTaskRunner> io_task_runner,
                gcm::GCMStatsRecorder* recorder)
      : MCSClient("",
                  clock,
                  connection_factory,
                  gcm_store,
                  io_task_runner,
                  recorder),
        next_id_(0) {}

  std::string GetNextPersistentId() override {
    return base::NumberToString(++next_id_);
  }

 private:
  uint32_t next_id_;
};

class TestConnectionListener : public ConnectionFactory::ConnectionListener {
 public:
  TestConnectionListener() : disconnect_counter_(0) { }
  ~TestConnectionListener() override { }

  void OnConnected(const GURL& current_server,
                   const net::IPEndPoint& ip_endpoint) override { }
  void OnDisconnected() override {
    ++disconnect_counter_;
  }

  int get_disconnect_counter() const { return disconnect_counter_; }
 private:
  int disconnect_counter_;
};

class MCSClientTest : public testing::Test {
 public:
  MCSClientTest();
  ~MCSClientTest() override;

  void SetUp() override;
  void TearDown() override;

  void BuildMCSClient();
  void InitializeClient();
  void StoreCredentials();
  void LoginClient(const std::vector<std::string>& acknowledged_ids);
  void LoginClientWithHeartbeat(
      const std::vector<std::string>& acknowledged_ids,
      int heartbeat_interval_ms);
  void AddExpectedLoginRequest(const std::vector<std::string>& acknowledged_ids,
                               int heartbeat_interval_ms);

  base::SimpleTestClock* clock() { return &clock_; }
  TestMCSClient* mcs_client() const { return mcs_client_.get(); }
  FakeConnectionFactory* connection_factory() {
    return &connection_factory_;
  }
  bool init_success() const { return init_success_; }
  uint64_t restored_android_id() const { return restored_android_id_; }
  uint64_t restored_security_token() const { return restored_security_token_; }
  MCSMessage* received_message() const { return received_message_.get(); }
  std::string sent_message_id() const { return sent_message_id_;}
  MCSClient::MessageSendStatus message_send_status() const {
    return message_send_status_;
  }

  void SetDeviceCredentialsCallback(bool success);

  FakeConnectionHandler* GetFakeHandler() const;

  void WaitForMCSEvent();
  void PumpLoop();

 private:
  void ErrorCallback();
  void MessageReceivedCallback(const MCSMessage& message);
  void MessageSentCallback(int64_t user_serial_number,
                           const std::string& app_id,
                           const std::string& message_id,
                           MCSClient::MessageSendStatus status);

  base::SimpleTestClock clock_;

  base::ScopedTempDir temp_directory_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<GCMStore> gcm_store_;

  FakeConnectionFactory connection_factory_;
  std::unique_ptr<TestMCSClient> mcs_client_;
  bool init_success_;
  uint64_t restored_android_id_;
  uint64_t restored_security_token_;
  std::unique_ptr<MCSMessage> received_message_;
  std::string sent_message_id_;
  MCSClient::MessageSendStatus message_send_status_;

  gcm::FakeGCMStatsRecorder recorder_;
};

MCSClientTest::MCSClientTest()
    : run_loop_(new base::RunLoop()),
      init_success_(true),
      restored_android_id_(0),
      restored_security_token_(0),
      message_send_status_(MCSClient::SENT) {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
  run_loop_ = std::make_unique<base::RunLoop>();

  // Advance the clock to a non-zero time.
  clock_.Advance(base::Seconds(1));
}

MCSClientTest::~MCSClientTest() {}

void MCSClientTest::SetUp() {
  testing::Test::SetUp();
}

void MCSClientTest::TearDown() {
  gcm_store_.reset();
  task_environment_.RunUntilIdle();
  testing::Test::TearDown();
}

void MCSClientTest::BuildMCSClient() {
  gcm_store_ = std::make_unique<GCMStoreImpl>(
      temp_directory_.GetPath(), task_environment_.GetMainThreadTaskRunner(),
      base::WrapUnique<Encryptor>(new FakeEncryptor));
  mcs_client_ = std::make_unique<TestMCSClient>(
      &clock_, &connection_factory_, gcm_store_.get(),
      base::SingleThreadTaskRunner::GetCurrentDefault(), &recorder_);
}

void MCSClientTest::InitializeClient() {
  gcm_store_->Load(
      GCMStore::CREATE_IF_MISSING,
      base::BindOnce(
          &MCSClient::Initialize, base::Unretained(mcs_client_.get()),
          base::BindRepeating(&MCSClientTest::ErrorCallback,
                              base::Unretained(this)),
          base::BindRepeating(&MCSClientTest::MessageReceivedCallback,
                              base::Unretained(this)),
          base::BindRepeating(&MCSClientTest::MessageSentCallback,
                              base::Unretained(this))));
  run_loop_->RunUntilIdle();
  run_loop_ = std::make_unique<base::RunLoop>();
}

void MCSClientTest::LoginClient(
    const std::vector<std::string>& acknowledged_ids) {
  LoginClientWithHeartbeat(acknowledged_ids, 0);
}

void MCSClientTest::LoginClientWithHeartbeat(
    const std::vector<std::string>& acknowledged_ids,
    int heartbeat_interval_ms) {
  AddExpectedLoginRequest(acknowledged_ids, heartbeat_interval_ms);
  mcs_client_->Login(kAndroidId, kSecurityToken);
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
}

void MCSClientTest::AddExpectedLoginRequest(
    const std::vector<std::string>& acknowledged_ids,
    int heartbeat_interval_ms) {
  std::unique_ptr<mcs_proto::LoginRequest> login_request =
      BuildLoginRequest(kAndroidId, kSecurityToken, "");
  for (size_t i = 0; i < acknowledged_ids.size(); ++i)
    login_request->add_received_persistent_id(acknowledged_ids[i]);
  if (heartbeat_interval_ms) {
    mcs_proto::Setting* setting = login_request->add_setting();
    setting->set_name("hbping");
    setting->set_value(base::NumberToString(heartbeat_interval_ms));
  }
  GetFakeHandler()->ExpectOutgoingMessage(
      MCSMessage(kLoginRequestTag, std::move(login_request)));
}

void MCSClientTest::StoreCredentials() {
  gcm_store_->SetDeviceCredentials(
      kAndroidId, kSecurityToken,
      base::BindOnce(&MCSClientTest::SetDeviceCredentialsCallback,
                     base::Unretained(this)));
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
}

FakeConnectionHandler* MCSClientTest::GetFakeHandler() const {
  return reinterpret_cast<FakeConnectionHandler*>(
      connection_factory_.GetConnectionHandler());
}

void MCSClientTest::WaitForMCSEvent() {
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
}

void MCSClientTest::PumpLoop() {
  run_loop_->RunUntilIdle();
  run_loop_ = std::make_unique<base::RunLoop>();
}

void MCSClientTest::ErrorCallback() {
  init_success_ = false;
  DVLOG(1) << "Error callback invoked, killing loop.";
  run_loop_->Quit();
}

void MCSClientTest::MessageReceivedCallback(const MCSMessage& message) {
  received_message_ = std::make_unique<MCSMessage>(message);
  DVLOG(1) << "Message received callback invoked, killing loop.";
  run_loop_->Quit();
}

void MCSClientTest::MessageSentCallback(int64_t user_serial_number,
                                        const std::string& app_id,
                                        const std::string& message_id,
                                        MCSClient::MessageSendStatus status) {
  DVLOG(1) << "Message sent callback invoked, killing loop.";
  sent_message_id_ = message_id;
  message_send_status_ = status;
  run_loop_->Quit();
}

void MCSClientTest::SetDeviceCredentialsCallback(bool success) {
  ASSERT_TRUE(success);
  run_loop_->Quit();
}

// Initialize a new client.
TEST_F(MCSClientTest, InitializeNew) {
  BuildMCSClient();
  InitializeClient();
  EXPECT_TRUE(init_success());
}

// Initialize a new client, shut it down, then restart the client. Should
// reload the existing device credentials.
TEST_F(MCSClientTest, InitializeExisting) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Rebuild the client, to reload from the GCM store.
  StoreCredentials();
  BuildMCSClient();
  InitializeClient();
  EXPECT_TRUE(init_success());
}

// Log in successfully to the MCS endpoint.
TEST_F(MCSClientTest, LoginSuccess) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  EXPECT_TRUE(connection_factory()->IsEndpointReachable());
  EXPECT_TRUE(init_success());
  ASSERT_TRUE(received_message());
  EXPECT_EQ(kLoginResponseTag, received_message()->tag());
}

// Encounter a server error during the login attempt. Should trigger a
// reconnect.
TEST_F(MCSClientTest, FailLogin) {
  BuildMCSClient();
  InitializeClient();
  GetFakeHandler()->set_fail_login(true);
  connection_factory()->set_delay_reconnect(true);
  LoginClient(std::vector<std::string>());
  EXPECT_FALSE(connection_factory()->IsEndpointReachable());
  EXPECT_FALSE(init_success());
  EXPECT_FALSE(received_message());
  EXPECT_TRUE(connection_factory()->reconnect_pending());
}

// Send a message without RMQ support.
TEST_F(MCSClientTest, SendMessageNoRMQ) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  MCSMessage message(BuildDataMessage("from", "category", "X", 1, "", 0, 1, 0,
                                      "", 0, IMMEDIATE_ACK_NO));
  GetFakeHandler()->ExpectOutgoingMessage(message);
  mcs_client()->SendMessage(message);
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

// Send a message without RMQ support while disconnected. Message send should
// fail immediately, invoking callback.
TEST_F(MCSClientTest, SendMessageNoRMQWhileDisconnected) {
  BuildMCSClient();
  InitializeClient();

  EXPECT_TRUE(sent_message_id().empty());
  MCSMessage message(BuildDataMessage("from", "category", "X", 1, "", 0, 1, 0,
                                      "", 0, IMMEDIATE_ACK_NO));
  mcs_client()->SendMessage(message);

  // Message sent callback should be invoked, but no message should actually
  // be sent.
  EXPECT_EQ("X", sent_message_id());
  EXPECT_EQ(MCSClient::NO_CONNECTION_ON_ZERO_TTL, message_send_status());
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

// Send a message with RMQ support.
TEST_F(MCSClientTest, SendMessageRMQ) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  MCSMessage message(BuildDataMessage("from", "category", "X", 1, "1",
                                      kTTLValue, 1, 0, "", 0,
                                      IMMEDIATE_ACK_NO));
  GetFakeHandler()->ExpectOutgoingMessage(message);
  mcs_client()->SendMessage(message);
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

// Send a message with RMQ support while disconnected. On reconnect, the message
// should be resent.
TEST_F(MCSClientTest, SendMessageRMQWhileDisconnected) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  GetFakeHandler()->set_fail_send(true);
  MCSMessage message(BuildDataMessage("from", "category", "X", 1, "1",
                                      kTTLValue, 1, 0, "", 0,
                                      IMMEDIATE_ACK_NO));

  // The initial (failed) send.
  GetFakeHandler()->ExpectOutgoingMessage(message);
  // The login request.
  GetFakeHandler()->ExpectOutgoingMessage(MCSMessage(
      kLoginRequestTag, BuildLoginRequest(kAndroidId, kSecurityToken, "")));
  // The second (re)send.
  MCSMessage message2(BuildDataMessage("from", "category", "X", 1, "1",
                                       kTTLValue, 1, kTTLValue - 1, "", 0,
                                       IMMEDIATE_ACK_NO));
  GetFakeHandler()->ExpectOutgoingMessage(message2);
  mcs_client()->SendMessage(message);
  PumpLoop();         // Wait for the queuing to happen.
  EXPECT_EQ(MCSClient::QUEUED, message_send_status());
  EXPECT_EQ("X", sent_message_id());
  EXPECT_FALSE(GetFakeHandler()->AllOutgoingMessagesReceived());
  GetFakeHandler()->set_fail_send(false);
  clock()->Advance(base::Seconds(kTTLValue - 1));
  connection_factory()->Connect();
  WaitForMCSEvent();  // Wait for the login to finish.
  PumpLoop();         // Wait for the send to happen.

  // Receive the ack.
  std::unique_ptr<mcs_proto::IqStanza> ack = BuildStreamAck();
  ack->set_last_stream_id_received(2);
  GetFakeHandler()->ReceiveMessage(MCSMessage(kIqStanzaTag, std::move(ack)));
  WaitForMCSEvent();

  EXPECT_EQ(MCSClient::SENT, message_send_status());
  EXPECT_EQ("X", sent_message_id());
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

// Send a message with RMQ support without receiving an acknowledgement. On
// restart the message should be resent.
TEST_F(MCSClientTest, SendMessageRMQOnRestart) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  GetFakeHandler()->set_fail_send(true);
  MCSMessage message(BuildDataMessage("from", "category", "X", 1, "1",
                                      kTTLValue, 1, 0, "", 0,
                                      IMMEDIATE_ACK_NO));

  // The initial (failed) send.
  GetFakeHandler()->ExpectOutgoingMessage(message);
  GetFakeHandler()->set_fail_send(false);
  mcs_client()->SendMessage(message);
  PumpLoop();
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());

  // Rebuild the client, which should resend the old message.
  StoreCredentials();
  BuildMCSClient();
  InitializeClient();

  clock()->Advance(base::Seconds(kTTLValue - 1));
  MCSMessage message2(BuildDataMessage("from", "category", "X", 1, "1",
                                       kTTLValue, 1, kTTLValue - 1, "", 0,
                                       IMMEDIATE_ACK_NO));
  LoginClient(std::vector<std::string>());
  GetFakeHandler()->ExpectOutgoingMessage(message2);
  PumpLoop();
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

// Send messages with RMQ support, followed by receiving a stream ack. On
// restart nothing should be recent.
TEST_F(MCSClientTest, SendMessageRMQWithStreamAck) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Send some messages.
  for (int i = 1; i <= kMessageBatchSize; ++i) {
    MCSMessage message(BuildDataMessage("from", "category", "X", 1,
                                        base::NumberToString(i), kTTLValue, 1,
                                        0, "", 0, IMMEDIATE_ACK_NO));
    GetFakeHandler()->ExpectOutgoingMessage(message);
    mcs_client()->SendMessage(message);
    PumpLoop();
  }
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());

  // Receive the ack.
  std::unique_ptr<mcs_proto::IqStanza> ack = BuildStreamAck();
  ack->set_last_stream_id_received(kMessageBatchSize + 1);
  GetFakeHandler()->ReceiveMessage(MCSMessage(kIqStanzaTag, std::move(ack)));
  WaitForMCSEvent();

  // Reconnect and ensure no messages are resent.
  StoreCredentials();
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  PumpLoop();
}

// Send messages with RMQ support. On restart, receive a SelectiveAck with
// the login response. No messages should be resent.
TEST_F(MCSClientTest, SendMessageRMQAckOnReconnect) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Send some messages.
  std::vector<std::string> id_list;
  for (int i = 1; i <= kMessageBatchSize; ++i) {
    id_list.push_back(base::NumberToString(i));
    MCSMessage message(BuildDataMessage("from", "category", id_list.back(), 1,
                                        id_list.back(), kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
    GetFakeHandler()->ExpectOutgoingMessage(message);
    mcs_client()->SendMessage(message);
    PumpLoop();
  }
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());

  // Rebuild the client, and receive an acknowledgment for the messages as
  // part of the login response.
  StoreCredentials();
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  std::unique_ptr<mcs_proto::IqStanza> ack(BuildSelectiveAck(id_list));
  GetFakeHandler()->ReceiveMessage(MCSMessage(kIqStanzaTag, std::move(ack)));
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

// Send messages with RMQ support. On restart, receive a SelectiveAck with
// the login response that only acks some messages. The unacked messages should
// be resent.
TEST_F(MCSClientTest, SendMessageRMQPartialAckOnReconnect) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Send some messages.
  std::vector<std::string> id_list;
  for (int i = 1; i <= kMessageBatchSize; ++i) {
    id_list.push_back(base::NumberToString(i));
    MCSMessage message(BuildDataMessage("from", "category", id_list.back(), 1,
                                        id_list.back(), kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
    GetFakeHandler()->ExpectOutgoingMessage(message);
    mcs_client()->SendMessage(message);
    PumpLoop();
  }
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());

  // Rebuild the client, and receive an acknowledgment for the messages as
  // part of the login response.
  StoreCredentials();
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  std::vector<std::string> acked_ids, remaining_ids;
  acked_ids.insert(acked_ids.end(),
                   id_list.begin(),
                   id_list.begin() + kMessageBatchSize / 2);
  remaining_ids.insert(remaining_ids.end(),
                       id_list.begin() + kMessageBatchSize / 2,
                       id_list.end());
  for (int i = 1; i <= kMessageBatchSize / 2; ++i) {
    MCSMessage message(BuildDataMessage(
        "from", "category", remaining_ids[i - 1], 2, remaining_ids[i - 1],
        kTTLValue, 1, 0, "", 0, IMMEDIATE_ACK_NO));
    GetFakeHandler()->ExpectOutgoingMessage(message);
  }
  std::unique_ptr<mcs_proto::IqStanza> ack(BuildSelectiveAck(acked_ids));
  GetFakeHandler()->ReceiveMessage(MCSMessage(kIqStanzaTag, std::move(ack)));
  WaitForMCSEvent();
  PumpLoop();
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

// Handle a selective ack that only acks some messages. The remaining unacked
// messages should be resent. On restart, those same unacked messages should be
// resent, and any pending acks for incoming messages should also be resent.
TEST_F(MCSClientTest, SelectiveAckMidStream) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Server stream id 2 ("s1").
  // Acks client stream id 0 (login).
  MCSMessage sMessage1(BuildDataMessage("from", "category", "X", 0, "s1",
                                        kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
  GetFakeHandler()->ReceiveMessage(sMessage1);
  WaitForMCSEvent();
  PumpLoop();

  // Client stream id 1 ("1").
  // Acks server stream id 2 ("s1").
  MCSMessage cMessage1(BuildDataMessage("from", "category", "Y", 2, "1",
                                        kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
  GetFakeHandler()->ExpectOutgoingMessage(cMessage1);
  mcs_client()->SendMessage(cMessage1);
  PumpLoop();
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());

  // Server stream id 3 ("s2").
  // Acks client stream id 1 ("1").
  // Confirms ack of server stream id 2 ("s1").
  MCSMessage sMessage2(BuildDataMessage("from", "category", "X", 1, "s2",
                                        kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
  GetFakeHandler()->ReceiveMessage(sMessage2);
  WaitForMCSEvent();
  PumpLoop();

  // Client Stream id 2 ("2").
  // Acks server stream id 3 ("s2").
  MCSMessage cMessage2(BuildDataMessage("from", "category", "Y", 3, "2",
                                        kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
  GetFakeHandler()->ExpectOutgoingMessage(cMessage2);
  mcs_client()->SendMessage(cMessage2);
  PumpLoop();
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());

  // Simulate the last message being dropped by having the server selectively
  // ack client message "1".
  // Client message "2" should be resent, acking server stream id 4 (selective
  // ack).
  MCSMessage cMessage3(BuildDataMessage("from", "category", "Y", 4, "2",
                                        kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
  GetFakeHandler()->ExpectOutgoingMessage(cMessage3);
  std::vector<std::string> acked_ids(1, "1");
  std::unique_ptr<mcs_proto::IqStanza> ack(BuildSelectiveAck(acked_ids));
  GetFakeHandler()->ReceiveMessage(MCSMessage(kIqStanzaTag, std::move(ack)));
  WaitForMCSEvent();
  PumpLoop();
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());

  // Rebuild the client without any further acks from server. Note that this
  // resets the stream ids.
  // Sever message "s2" should be acked as part of login.
  // Client message "2" should be resent.
  StoreCredentials();
  BuildMCSClient();
  InitializeClient();

  acked_ids[0] = "s2";
  LoginClient(acked_ids);

  MCSMessage cMessage4(BuildDataMessage("from", "category", "Y", 1, "2",
                                        kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
  GetFakeHandler()->ExpectOutgoingMessage(cMessage4);
  PumpLoop();
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

// Receive some messages. On restart, the login request should contain the
// appropriate acknowledged ids.
TEST_F(MCSClientTest, AckOnLogin) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Receive some messages.
  std::vector<std::string> id_list;
  for (int i = 1; i <= kMessageBatchSize; ++i) {
    id_list.push_back(base::NumberToString(i));
    MCSMessage message(BuildDataMessage("from", "category", "X", 1,
                                        id_list.back(), kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
    GetFakeHandler()->ReceiveMessage(message);
    WaitForMCSEvent();
    PumpLoop();
  }

  // Restart the client.
  StoreCredentials();
  BuildMCSClient();
  InitializeClient();
  LoginClient(id_list);
}

// Receive some messages. On the next send, the outgoing message should contain
// the appropriate last stream id received field to ack the received messages.
TEST_F(MCSClientTest, AckOnSend) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // Receive some messages.
  std::vector<std::string> id_list;
  for (int i = 1; i <= kMessageBatchSize; ++i) {
    id_list.push_back(base::NumberToString(i));
    MCSMessage message(BuildDataMessage("from", "category", id_list.back(), 1,
                                        id_list.back(), kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
    GetFakeHandler()->ReceiveMessage(message);
    PumpLoop();
  }

  // Trigger a message send, which should acknowledge via stream ack.
  MCSMessage message(BuildDataMessage("from", "category", "X",
                                      kMessageBatchSize + 1, "1", kTTLValue, 1,
                                      0, "", 0, IMMEDIATE_ACK_NO));
  GetFakeHandler()->ExpectOutgoingMessage(message);
  mcs_client()->SendMessage(message);
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

// Receive the ack limit in messages, which should trigger an automatic
// stream ack. Receive a heartbeat to confirm the ack.
TEST_F(MCSClientTest, AckWhenLimitReachedWithHeartbeat) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // The stream ack.
  std::unique_ptr<mcs_proto::IqStanza> ack = BuildStreamAck();
  ack->set_last_stream_id_received(kAckLimitSize + 1);
  GetFakeHandler()->ExpectOutgoingMessage(
      MCSMessage(kIqStanzaTag, std::move(ack)));

  // Receive some messages.
  std::vector<std::string> id_list;
  for (int i = 1; i <= kAckLimitSize; ++i) {
    id_list.push_back(base::NumberToString(i));
    MCSMessage message(BuildDataMessage("from", "category", id_list.back(), 1,
                                        id_list.back(), kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
    GetFakeHandler()->ReceiveMessage(message);
    WaitForMCSEvent();
    PumpLoop();
  }
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());

  // Receive a heartbeat confirming the ack (and receive the heartbeat ack).
  std::unique_ptr<mcs_proto::HeartbeatPing> heartbeat(
      new mcs_proto::HeartbeatPing());
  heartbeat->set_last_stream_id_received(2);

  std::unique_ptr<mcs_proto::HeartbeatAck> heartbeat_ack(
      new mcs_proto::HeartbeatAck());
  heartbeat_ack->set_last_stream_id_received(kAckLimitSize + 2);
  GetFakeHandler()->ExpectOutgoingMessage(
      MCSMessage(kHeartbeatAckTag, std::move(heartbeat_ack)));

  GetFakeHandler()->ReceiveMessage(
      MCSMessage(kHeartbeatPingTag, std::move(heartbeat)));
  PumpLoop();
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());

  // Rebuild the client. Nothing should be sent on login.
  StoreCredentials();
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

// If a message's TTL has expired by the time it reaches the front of the send
// queue, it should be dropped.
TEST_F(MCSClientTest, ExpiredTTLOnSend) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  MCSMessage message(BuildDataMessage("from", "category", "X", 1, "1",
                                      kTTLValue, 1, 0, "", 0,
                                      IMMEDIATE_ACK_NO));

  // Advance time to after the TTL.
  clock()->Advance(base::Seconds(kTTLValue + 2));
  EXPECT_TRUE(sent_message_id().empty());
  mcs_client()->SendMessage(message);

  // No messages should be sent, but the callback should still be invoked.
  EXPECT_EQ("X", sent_message_id());
  EXPECT_EQ(MCSClient::TTL_EXCEEDED, message_send_status());
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

TEST_F(MCSClientTest, ExpiredTTLOnRestart) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  GetFakeHandler()->set_fail_send(true);
  MCSMessage message(BuildDataMessage("from", "category", "X", 1, "1",
                                      kTTLValue, 1, 0, "", 0,
                                      IMMEDIATE_ACK_NO));

  // The initial (failed) send.
  GetFakeHandler()->ExpectOutgoingMessage(message);
  GetFakeHandler()->set_fail_send(false);
  mcs_client()->SendMessage(message);
  PumpLoop();
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());

  // Move the clock forward and rebuild the client, which should fail the
  // message send on restart.
  clock()->Advance(base::Seconds(kTTLValue + 2));
  StoreCredentials();
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  PumpLoop();
  EXPECT_EQ("X", sent_message_id());
  EXPECT_EQ(MCSClient::TTL_EXCEEDED, message_send_status());
  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

// Sending two messages with the same collapse key and same app id while
// disconnected should only send the latter of the two on reconnection.
TEST_F(MCSClientTest, CollapseKeysSameApp) {
  BuildMCSClient();
  InitializeClient();
  MCSMessage message(BuildDataMessage("from", "app", "message id 1", 1, "1",
                                      kTTLValue, 1, 0, "token", 0,
                                      IMMEDIATE_ACK_NO));
  mcs_client()->SendMessage(message);

  MCSMessage message2(BuildDataMessage("from", "app", "message id 2", 1, "1",
                                       kTTLValue, 1, 0, "token", 0,
                                       IMMEDIATE_ACK_NO));
  mcs_client()->SendMessage(message2);

  LoginClient(std::vector<std::string>());
  GetFakeHandler()->ExpectOutgoingMessage(message2);
  PumpLoop();
}

// Sending two messages with the same collapse key and different app id while
// disconnected should not perform any collapsing.
TEST_F(MCSClientTest, CollapseKeysDifferentApp) {
  BuildMCSClient();
  InitializeClient();
  MCSMessage message(BuildDataMessage("from", "app", "message id 1", 1, "1",
                                      kTTLValue, 1, 0, "token", 0,
                                      IMMEDIATE_ACK_NO));
  mcs_client()->SendMessage(message);

  MCSMessage message2(BuildDataMessage("from", "app 2", "message id 2", 1, "2",
                                       kTTLValue, 1, 0, "token", 0,
                                       IMMEDIATE_ACK_NO));
  mcs_client()->SendMessage(message2);

  LoginClient(std::vector<std::string>());
  GetFakeHandler()->ExpectOutgoingMessage(message);
  GetFakeHandler()->ExpectOutgoingMessage(message2);
  PumpLoop();
}

// Sending two messages with the same collapse key and app id, but different
// user, while disconnected, should not perform any collapsing.
TEST_F(MCSClientTest, CollapseKeysDifferentUser) {
  BuildMCSClient();
  InitializeClient();
  MCSMessage message(BuildDataMessage("from", "app", "message id 1", 1, "1",
                                      kTTLValue, 1, 0, "token", 0,
                                      IMMEDIATE_ACK_NO));
  mcs_client()->SendMessage(message);

  MCSMessage message2(BuildDataMessage("from", "app", "message id 2", 1, "2",
                                       kTTLValue, 1, 0, "token", 1,
                                       IMMEDIATE_ACK_NO));
  mcs_client()->SendMessage(message2);

  LoginClient(std::vector<std::string>());
  GetFakeHandler()->ExpectOutgoingMessage(message);
  GetFakeHandler()->ExpectOutgoingMessage(message2);
  PumpLoop();
}

// Test case for setting a custom heartbeat interval, when it is too short.
// Covers both connection restart and storing of custom intervals.
TEST_F(MCSClientTest, CustomHeartbeatIntervalTooShort) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  PumpLoop();
  StoreCredentials();

  HeartbeatManager* hb_manager = mcs_client()->GetHeartbeatManagerForTesting();
  // By default custom client interval is not set.
  EXPECT_FALSE(hb_manager->HasClientHeartbeatInterval());

  const std::string component_1 = "component1";
  int interval_ms = 15 * 1000;  // 15 seconds, too low.
  mcs_client()->AddHeartbeatInterval(component_1, interval_ms);
  // Setting was too low so it was ignored.
  EXPECT_FALSE(hb_manager->HasClientHeartbeatInterval());

  // Restore and check again to make sure that nothing was set in store.
  BuildMCSClient();
  InitializeClient();
  LoginClientWithHeartbeat(std::vector<std::string>(), 0);
  PumpLoop();

  hb_manager = mcs_client()->GetHeartbeatManagerForTesting();
  EXPECT_FALSE(hb_manager->HasClientHeartbeatInterval());
}

// Test case for setting a custom heartbeat interval, when it is too long.
// Covers both connection restart and storing of custom intervals.
TEST_F(MCSClientTest, CustomHeartbeatIntervalTooLong) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  PumpLoop();
  StoreCredentials();

  HeartbeatManager* hb_manager = mcs_client()->GetHeartbeatManagerForTesting();

  const std::string component_1 = "component1";
  int interval_ms = 60 * 60 * 1000;  // 1 hour, too high.
  mcs_client()->AddHeartbeatInterval(component_1, interval_ms);
  // Setting was too high, again it was ignored.
  EXPECT_FALSE(hb_manager->HasClientHeartbeatInterval());

  // Restore and check again to make sure that nothing was set in store.
  BuildMCSClient();
  InitializeClient();
  LoginClientWithHeartbeat(std::vector<std::string>(), 0);
  PumpLoop();

  // Setting was too high, again it was ignored.
  hb_manager = mcs_client()->GetHeartbeatManagerForTesting();
  EXPECT_FALSE(hb_manager->HasClientHeartbeatInterval());
}

// Tests adding and removing custom heartbeat interval.
// Covers both connection restart and storing of custom intervals.
TEST_F(MCSClientTest, CustomHeartbeatIntervalSingleInterval) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  PumpLoop();
  StoreCredentials();

  TestConnectionListener test_connection_listener;
  connection_factory()->SetConnectionListener(&test_connection_listener);

  HeartbeatManager* hb_manager = mcs_client()->GetHeartbeatManagerForTesting();

  const std::string component_1 = "component1";
  int interval_ms = 5 * 60 * 1000;  // 5 minutes. A valid setting.

  AddExpectedLoginRequest(std::vector<std::string>(), interval_ms);
  mcs_client()->AddHeartbeatInterval(component_1, interval_ms);
  PumpLoop();

  // Interval was OK. HearbeatManager should get that setting now.
  EXPECT_TRUE(hb_manager->HasClientHeartbeatInterval());
  EXPECT_EQ(interval_ms, hb_manager->GetClientHeartbeatIntervalMs());
  EXPECT_EQ(1, test_connection_listener.get_disconnect_counter());

  // Check that setting was persisted and will take effect upon restart.
  BuildMCSClient();
  InitializeClient();
  LoginClientWithHeartbeat(std::vector<std::string>(), interval_ms);
  PumpLoop();

  // HB manger uses the shortest persisted interval after restart.
  hb_manager = mcs_client()->GetHeartbeatManagerForTesting();
  EXPECT_TRUE(hb_manager->HasClientHeartbeatInterval());
  EXPECT_EQ(interval_ms, hb_manager->GetClientHeartbeatIntervalMs());

  mcs_client()->RemoveHeartbeatInterval(component_1);
  PumpLoop();

  // Check that setting was persisted and will take effect upon restart.
  BuildMCSClient();
  InitializeClient();
  LoginClientWithHeartbeat(std::vector<std::string>(), 0);
  PumpLoop();

  hb_manager = mcs_client()->GetHeartbeatManagerForTesting();
  EXPECT_FALSE(hb_manager->HasClientHeartbeatInterval());
}

// Tests adding custom heartbeat interval before connection is initialized.
TEST_F(MCSClientTest, CustomHeartbeatIntervalSetBeforeInitialize) {
  BuildMCSClient();

  const std::string component_1 = "component1";
  int interval_ms = 5 * 60 * 1000;  // 5 minutes. A valid setting.
  mcs_client()->AddHeartbeatInterval(component_1, interval_ms);
  InitializeClient();
  LoginClientWithHeartbeat(std::vector<std::string>(), interval_ms);
  HeartbeatManager* hb_manager = mcs_client()->GetHeartbeatManagerForTesting();
  EXPECT_TRUE(hb_manager->HasClientHeartbeatInterval());
}

// Tests adding custom heartbeat interval after connection is initialized, but
// but before login is sent.
TEST_F(MCSClientTest, CustomHeartbeatIntervalSetBeforeLogin) {
  BuildMCSClient();

  const std::string component_1 = "component1";
  int interval_ms = 5 * 60 * 1000;  // 5 minutes. A valid setting.
  InitializeClient();
  mcs_client()->AddHeartbeatInterval(component_1, interval_ms);
  LoginClientWithHeartbeat(std::vector<std::string>(), interval_ms);
  HeartbeatManager* hb_manager = mcs_client()->GetHeartbeatManagerForTesting();
  EXPECT_TRUE(hb_manager->HasClientHeartbeatInterval());
}

// Tests situation when two heartbeat intervals are set and second is longer.
// Covers both connection restart and storing of custom intervals.
TEST_F(MCSClientTest, CustomHeartbeatIntervalSecondIntervalLonger) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  PumpLoop();
  StoreCredentials();

  TestConnectionListener test_connection_listener;
  connection_factory()->SetConnectionListener(&test_connection_listener);

  HeartbeatManager* hb_manager = mcs_client()->GetHeartbeatManagerForTesting();

  const std::string component_1 = "component1";
  int interval_ms = 5 * 60 * 1000;  // 5 minutes. A valid setting.

  AddExpectedLoginRequest(std::vector<std::string>(), interval_ms);
  mcs_client()->AddHeartbeatInterval(component_1, interval_ms);
  PumpLoop();

  const std::string component_2 = "component2";
  int other_interval_ms = 10 * 60 * 1000;  // 10 minutes. A valid setting.
  mcs_client()->AddHeartbeatInterval(component_2, other_interval_ms);
  PumpLoop();

  // Interval was OK, but longer. HearbeatManager will use the first one.
  EXPECT_TRUE(hb_manager->HasClientHeartbeatInterval());
  EXPECT_EQ(interval_ms, hb_manager->GetClientHeartbeatIntervalMs());
  EXPECT_EQ(1, test_connection_listener.get_disconnect_counter());

  // Check that setting was persisted and will take effect upon restart.
  BuildMCSClient();
  InitializeClient();
  LoginClientWithHeartbeat(std::vector<std::string>(), interval_ms);
  PumpLoop();

  // HB manger uses the shortest persisted interval after restart.
  hb_manager = mcs_client()->GetHeartbeatManagerForTesting();
  EXPECT_TRUE(hb_manager->HasClientHeartbeatInterval());
  EXPECT_EQ(interval_ms, hb_manager->GetClientHeartbeatIntervalMs());
}

// Tests situation when two heartbeat intervals are set and second is shorter.
// Covers both connection restart and storing of custom intervals.
TEST_F(MCSClientTest, CustomHeartbeatIntervalSecondIntervalShorter) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  PumpLoop();
  StoreCredentials();

  TestConnectionListener test_connection_listener;
  connection_factory()->SetConnectionListener(&test_connection_listener);

  HeartbeatManager* hb_manager = mcs_client()->GetHeartbeatManagerForTesting();

  const std::string component_1 = "component1";
  int interval_ms = 5 * 60 * 1000;  // 5 minutes. A valid setting.

  AddExpectedLoginRequest(std::vector<std::string>(), interval_ms);
  mcs_client()->AddHeartbeatInterval(component_1, interval_ms);
  PumpLoop();

  const std::string component_2 = "component2";
  int other_interval_ms = 3 * 60 * 1000;  // 3 minutes. A valid setting.
  AddExpectedLoginRequest(std::vector<std::string>(), other_interval_ms);
  mcs_client()->AddHeartbeatInterval(component_2, other_interval_ms);
  PumpLoop();
  // Interval was OK. HearbeatManager should get that setting now.
  EXPECT_TRUE(hb_manager->HasClientHeartbeatInterval());
  EXPECT_EQ(other_interval_ms, hb_manager->GetClientHeartbeatIntervalMs());
  EXPECT_EQ(2, test_connection_listener.get_disconnect_counter());

  // Check that setting was persisted and will take effect upon restart.
  BuildMCSClient();
  InitializeClient();
  LoginClientWithHeartbeat(std::vector<std::string>(), other_interval_ms);
  PumpLoop();

  // HB manger uses the shortest persisted interval after restart.
  hb_manager = mcs_client()->GetHeartbeatManagerForTesting();
  EXPECT_TRUE(hb_manager->HasClientHeartbeatInterval());
  EXPECT_EQ(other_interval_ms, hb_manager->GetClientHeartbeatIntervalMs());
}

// Tests situation shorter of two intervals is removed.
// Covers both connection restart and storing of custom intervals.
TEST_F(MCSClientTest, CustomHeartbeatIntervalRemoveShorterInterval) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());
  PumpLoop();
  StoreCredentials();

  TestConnectionListener test_connection_listener;
  connection_factory()->SetConnectionListener(&test_connection_listener);

  HeartbeatManager* hb_manager = mcs_client()->GetHeartbeatManagerForTesting();

  const std::string component_1 = "component1";
  int interval_ms = 5 * 60 * 1000;  // 5 minutes. A valid setting.

  AddExpectedLoginRequest(std::vector<std::string>(), interval_ms);
  mcs_client()->AddHeartbeatInterval(component_1, interval_ms);
  PumpLoop();

  const std::string component_2 = "component2";
  int other_interval_ms = 3 * 60 * 1000;  // 3 minutes. A valid setting.
  AddExpectedLoginRequest(std::vector<std::string>(), other_interval_ms);
  mcs_client()->AddHeartbeatInterval(component_2, other_interval_ms);
  PumpLoop();

  mcs_client()->RemoveHeartbeatInterval(component_2);
  PumpLoop();

  // Removing the lowest setting reverts to second lowest.
  EXPECT_TRUE(hb_manager->HasClientHeartbeatInterval());
  EXPECT_EQ(interval_ms, hb_manager->GetClientHeartbeatIntervalMs());
  // No connection reset expected.
  EXPECT_EQ(2, test_connection_listener.get_disconnect_counter());

  // Check that setting was persisted and will take effect upon restart.
  BuildMCSClient();
  InitializeClient();
  LoginClientWithHeartbeat(std::vector<std::string>(), interval_ms);
  PumpLoop();

  // HB manger uses the shortest persisted interval after restart.
  hb_manager = mcs_client()->GetHeartbeatManagerForTesting();
  EXPECT_TRUE(hb_manager->HasClientHeartbeatInterval());
  EXPECT_EQ(interval_ms, hb_manager->GetClientHeartbeatIntervalMs());
}

// Receive a message with immediate ack request, which should trigger an
// automatic stream ack.
TEST_F(MCSClientTest, AckWhenImmediateAckRequested) {
  BuildMCSClient();
  InitializeClient();
  LoginClient(std::vector<std::string>());

  // The stream ack.
  std::unique_ptr<mcs_proto::IqStanza> ack = BuildStreamAck();
  ack->set_last_stream_id_received(kAckLimitSize - 1);
  GetFakeHandler()->ExpectOutgoingMessage(
      MCSMessage(kIqStanzaTag, std::move(ack)));

  // Receive some messages.
  for (int i = 1; i < kAckLimitSize - 2; ++i) {
    std::string id(base::NumberToString(i));
    MCSMessage message(BuildDataMessage("from", "category", id, 1, id,
                                        kTTLValue, 1, 0, "", 0,
                                        IMMEDIATE_ACK_NO));
    GetFakeHandler()->ReceiveMessage(message);
    WaitForMCSEvent();
    PumpLoop();
  }
  // This message expects immediate ACK, which means it will happen before the
  // ACK limit size is reached. All of the preceding messages will be acked at
  // the same time.
  std::string ack_id(base::NumberToString(kAckLimitSize - 1));
  MCSMessage message(BuildDataMessage("from", "category", ack_id, 1, ack_id,
                                      kTTLValue, 1, 0, "", 0,
                                      IMMEDIATE_ACK_YES));
  GetFakeHandler()->ReceiveMessage(message);
  WaitForMCSEvent();
  PumpLoop();

  EXPECT_TRUE(GetFakeHandler()->AllOutgoingMessagesReceived());
}

} // namespace

}  // namespace gcm
