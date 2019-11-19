// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/display_source/display_source_apitestbase.h"

#include <list>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/udp_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using api::display_source::SinkInfo;
using api::display_source::SinkState;
using api::display_source::AuthenticationMethod;
using api::display_source::SINK_STATE_DISCONNECTED;
using api::display_source::SINK_STATE_CONNECTING;
using api::display_source::SINK_STATE_CONNECTED;
using api::display_source::AUTHENTICATION_METHOD_PBC;
using api::display_source::AUTHENTICATION_METHOD_PIN;
using content::BrowserThread;

class MockDisplaySourceConnectionDelegate
    : public DisplaySourceConnectionDelegate,
      public DisplaySourceConnectionDelegate::Connection {
 public:
  MockDisplaySourceConnectionDelegate();

  const DisplaySourceSinkInfoList& last_found_sinks() const override;

  DisplaySourceConnectionDelegate::Connection* connection()
      override {
    return (active_sink_ && active_sink_->state == SINK_STATE_CONNECTED)
               ? this
               : nullptr;
  }

  void GetAvailableSinks(const SinkInfoListCallback& sinks_callback,
                         const StringCallback& failure_callback) override;

  void RequestAuthentication(int sink_id,
                             const AuthInfoCallback& auth_info_callback,
                             const StringCallback& failure_callback) override;

  void Connect(int sink_id,
               const DisplaySourceAuthInfo& auth_info,
               const StringCallback& failure_callback) override;

  void Disconnect(const StringCallback& failure_callback) override;

  void StartWatchingAvailableSinks() override;

  // DisplaySourceConnectionDelegate::Connection overrides
  const DisplaySourceSinkInfo& GetConnectedSink() const override;

  void StopWatchingAvailableSinks() override;

  net::IPAddress GetLocalAddress() const override;

  net::IPAddress GetSinkAddress() const override;

  void SendMessage(const std::string& message) override;

  void SetMessageReceivedCallback(
      const StringCallback& callback) override;

 private:
  void AddSink(DisplaySourceSinkInfo sink,
               AuthenticationMethod auth_method,
               const std::string& pin_value);

  void OnSinkConnected();

  void NotifySinksUpdated();

  void EnqueueSinkMessage(std::string message);

  void CheckSourceMessageContent(std::string pattern,
                                 const std::string& message);

  void BindToUdpSocket();

  void ReceiveMediaPacket();

  void OnMediaPacketReceived(int net_result);

  DisplaySourceSinkInfoList sinks_;
  DisplaySourceSinkInfo* active_sink_;
  std::map<int, std::pair<AuthenticationMethod, std::string>> auth_infos_;
  StringCallback message_received_cb_;

  struct Message {
    enum Direction {
      SourceToSink,
      SinkToSource
    };
    std::string data;
    Direction direction;

    bool is_from_sink() const { return direction == SinkToSource; }
    Message(const std::string& message_data, Direction direction)
      : data(message_data), direction(direction) {}
  };

  std::list<Message> messages_list_;
  std::string session_id_;

  std::unique_ptr<net::UDPSocket,
      content::BrowserThread::DeleteOnIOThread> socket_;
  scoped_refptr<net::IOBuffer> recvfrom_buffer_;
  net::IPEndPoint end_point_;
  std::string udp_port_;
};

namespace {

const size_t kSessionIdLength = 8;
const size_t kUdpPortLength = 5;
const char kClientPortKey[] = "client_port=";
const char kLocalHost[] = "127.0.0.1";
const char kSessionKey[] = "Session: ";
const char kUnicastKey[] = "unicast ";
const int kPortStart = 10000;
const int kPortEnd = 65535;

DisplaySourceSinkInfo CreateSinkInfo(int id, const std::string& name) {
  DisplaySourceSinkInfo ptr;
  ptr.id = id;
  ptr.name = name;
  ptr.state = SINK_STATE_DISCONNECTED;

  return ptr;
}

std::unique_ptr<KeyedService> CreateMockDelegate(
    content::BrowserContext* profile) {
  return base::WrapUnique<KeyedService>(
      new MockDisplaySourceConnectionDelegate());
}

void AdaptMessagePattern(std::size_t key_pos,
                         const char *key,
                         std::size_t substr_len,
                         const std::string& replace_with,
                         std::string& pattern) {
  const std::size_t position = key_pos +
             std::char_traits<char>::length(key);
  pattern.replace(position, substr_len, replace_with);
}

}  // namespace

void InitMockDisplaySourceConnectionDelegate(content::BrowserContext* profile) {
  DisplaySourceConnectionDelegateFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&CreateMockDelegate));
}
namespace {

// WiFi Display session RTSP messages patterns.

const char kM1Message[] = "OPTIONS * RTSP/1.0\r\n"
                          "CSeq: 1\r\n"
                          "Require: org.wfa.wfd1.0\r\n\r\n";

const char kM1MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq:1\r\n"
                               "Public: org.wfa.wfd1.0, "
                               "GET_PARAMETER, SET_PARAMETER\r\n\r\n";

const char kM2Message[] = "OPTIONS * RTSP/1.0\r\n"
                          "CSeq: 2\r\n"
                          "Require: org.wfa.wfd1.0\r\n\r\n";

const char kM2MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 2\r\n"
                               "Public: org.wfa.wfd1.0, "
                               "GET_PARAMETER, SET_PARAMETER, PLAY, PAUSE, "
                               "SETUP, TEARDOWN\r\n\r\n";

const char kM3Message[] = "GET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
                          "CSeq: 2\r\n"
                          "Content-Type: text/parameters\r\n"
                          "Content-Length: 41\r\n\r\n"
                          "wfd_video_formats\r\n"
                          "wfd_client_rtp_ports\r\n";

const char kM3MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 2\r\n"
                               "Content-Type: text/parameters\r\n"
                               "Content-Length: 145\r\n\r\n"
                               "wfd_video_formats: "
                               "00 00 01 01 0001FFFF 1FFFFFFF 00000FFF 00 0000 "
                               "0000 00 none none\r\n"
                               "wfd_client_rtp_ports: RTP/AVP/UDP;"
                               "unicast 00000 0 mode=play\r\n";

const char kM4Message[] = "SET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
                          "CSeq: 3\r\n"
                          "Content-Type: text/parameters\r\n"
                          "Content-Length: 209\r\n\r\n"
                          "wfd_client_rtp_ports: "
                          "RTP/AVP/UDP;unicast 00000 0 mode=play\r\n"
                          "wfd_presentation_URL: "
                          "rtsp://127.0.0.1/wfd1.0/streamid=0 none\r\n"
                          "wfd_video_formats: "
                          "00 00 01 01 00000001 00000000 00000000 00 0000 0000 "
                          "00 none none\r\n";

const char kM4MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 3\r\n\r\n";

const char kM5Message[] = "SET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
                          "CSeq: 4\r\n"
                          "Content-Type: text/parameters\r\n"
                          "Content-Length: 27\r\n\r\n"
                          "wfd_trigger_method: SETUP\r\n";

const char kM5MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 4\r\n\r\n";

const char kM6Message[] = "SETUP rtsp://localhost/wfd1.0/streamid=0 "
                          "RTSP/1.0\r\n"
                          "CSeq: 3\r\n"
                          "Transport: RTP/AVP/UDP;unicast;"
                          "client_port=00000\r\n\r\n";

const char kM6MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 3\r\n"
                               "Session: 00000000;timeout=60\r\n"
                               "Transport: RTP/AVP/UDP;unicast;"
                               "client_port=00000\r\n\r\n";

const char kM7Message[] = "PLAY rtsp://localhost/wfd1.0/streamid=0 RTSP/1.0\r\n"
                          "CSeq: 4\r\n"
                          "Session: 00000000\r\n\r\n";

const char kM7MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 4\r\n\r\n";

const char kM8Message[] = "GET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
                          "CSeq: 5\r\n\r\n";

const char kM8MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 5\r\n\r\n";

const char kM9Message[] = "GET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
                          "CSeq: 6\r\n\r\n";

const char kM9MessageReply[] = "RTSP/1.0 200 OK\r\n"
                               "CSeq: 6\r\n\r\n";

const char kM10Message[] = "GET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n"
                           "CSeq: 7\r\n\r\n";

const char kM10MessageReply[] = "RTSP/1.0 200 OK\r\n"
                                "CSeq: 7\r\n\r\n";

} // namespace
MockDisplaySourceConnectionDelegate::MockDisplaySourceConnectionDelegate()
    : active_sink_(nullptr) {
  messages_list_.push_back(Message(kM1Message, Message::SourceToSink));
  messages_list_.push_back(Message(kM1MessageReply, Message::SinkToSource));
  messages_list_.push_back(Message(kM2Message, Message::SinkToSource));
  messages_list_.push_back(Message(kM2MessageReply, Message::SourceToSink));
  messages_list_.push_back(Message(kM3Message, Message::SourceToSink));
  messages_list_.push_back(Message(kM3MessageReply, Message::SinkToSource));
  messages_list_.push_back(Message(kM4Message, Message::SourceToSink));
  messages_list_.push_back(Message(kM4MessageReply, Message::SinkToSource));
  messages_list_.push_back(Message(kM5Message, Message::SourceToSink));
  messages_list_.push_back(Message(kM5MessageReply, Message::SinkToSource));
  messages_list_.push_back(Message(kM6Message, Message::SinkToSource));
  messages_list_.push_back(Message(kM6MessageReply, Message::SourceToSink));
  messages_list_.push_back(Message(kM7Message, Message::SinkToSource));
  messages_list_.push_back(Message(kM7MessageReply, Message::SourceToSink));
  messages_list_.push_back(Message(kM8Message, Message::SourceToSink));
  messages_list_.push_back(Message(kM8MessageReply, Message::SinkToSource));
  messages_list_.push_back(Message(kM9Message, Message::SourceToSink));
  messages_list_.push_back(Message(kM9MessageReply, Message::SinkToSource));
  messages_list_.push_back(Message(kM10Message, Message::SourceToSink));
  messages_list_.push_back(Message(kM10MessageReply, Message::SinkToSource));

  AddSink(CreateSinkInfo(1, "sink 1"), AUTHENTICATION_METHOD_PIN, "1234");
}

const DisplaySourceSinkInfoList&
MockDisplaySourceConnectionDelegate::last_found_sinks() const {
  return sinks_;
}

void MockDisplaySourceConnectionDelegate::GetAvailableSinks(
    const SinkInfoListCallback& sinks_callback,
    const StringCallback& failure_callback) {
  sinks_callback.Run(sinks_);
}

void MockDisplaySourceConnectionDelegate::RequestAuthentication(
    int sink_id,
    const AuthInfoCallback& auth_info_callback,
    const StringCallback& failure_callback) {
  DisplaySourceAuthInfo info;
  auto it = auth_infos_.find(sink_id);
  ASSERT_NE(it, auth_infos_.end());

  info.method = it->second.first;
  auth_info_callback.Run(info);
}

void MockDisplaySourceConnectionDelegate::Connect(
    int sink_id,
    const DisplaySourceAuthInfo& auth_info,
    const StringCallback& failure_callback) {
  auto it = auth_infos_.find(sink_id);
  ASSERT_NE(it, auth_infos_.end());
  ASSERT_EQ(it->second.first, auth_info.method);
  ASSERT_STREQ(it->second.second.c_str(), auth_info.data->c_str());

  auto found = std::find_if(sinks_.begin(), sinks_.end(),
                            [sink_id](const DisplaySourceSinkInfo& sink) {
                              return sink.id == sink_id;
                            });

  ASSERT_NE(found, sinks_.end());
  active_sink_ = sinks_.data() + (found - sinks_.begin());
  active_sink_->state = SINK_STATE_CONNECTING;
  NotifySinksUpdated();

  // Bind sink to udp socket at this stage
  // And store udp port to udp_port_ string in order to be used
  // In a message exchange. Then make a base::PostTask
  // on UI thread and call OnSinkConnected() to proceed with the test
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&MockDisplaySourceConnectionDelegate::BindToUdpSocket,
                     base::Unretained(this)));
}

void MockDisplaySourceConnectionDelegate::Disconnect(
    const StringCallback& failure_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(active_sink_);
  ASSERT_EQ(active_sink_->state, SINK_STATE_CONNECTED);
  active_sink_->state = SINK_STATE_DISCONNECTED;
  active_sink_ = nullptr;
  NotifySinksUpdated();
}

void MockDisplaySourceConnectionDelegate::StartWatchingAvailableSinks() {
  AddSink(CreateSinkInfo(2, "sink 2"), AUTHENTICATION_METHOD_PBC, "");
}

const DisplaySourceSinkInfo&
MockDisplaySourceConnectionDelegate::GetConnectedSink() const {
  CHECK(active_sink_);
  return *active_sink_;
}

void MockDisplaySourceConnectionDelegate::StopWatchingAvailableSinks() {}

net::IPAddress MockDisplaySourceConnectionDelegate::GetLocalAddress() const {
  return net::IPAddress::IPv4Localhost();
}

net::IPAddress MockDisplaySourceConnectionDelegate::GetSinkAddress() const {
  return net::IPAddress::IPv4Localhost();
}

void MockDisplaySourceConnectionDelegate::SendMessage(
    const std::string& message) {
  ASSERT_FALSE(messages_list_.empty());
  ASSERT_FALSE(messages_list_.front().is_from_sink());

  CheckSourceMessageContent(messages_list_.front().data, message);
  messages_list_.pop_front();

  while (!messages_list_.empty() && messages_list_.front().is_from_sink()) {
    EnqueueSinkMessage(messages_list_.front().data);
    messages_list_.pop_front();
  }
}

void MockDisplaySourceConnectionDelegate::SetMessageReceivedCallback(
    const StringCallback& callback) {
  message_received_cb_ = callback;
}

void MockDisplaySourceConnectionDelegate::AddSink(
    DisplaySourceSinkInfo sink,
    AuthenticationMethod auth_method,
    const std::string& pin_value) {
  auth_infos_[sink.id] = {auth_method, pin_value};
  sinks_.push_back(std::move(sink));
  NotifySinksUpdated();
}

void MockDisplaySourceConnectionDelegate::OnSinkConnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(active_sink_);
  active_sink_->state = SINK_STATE_CONNECTED;
  NotifySinksUpdated();
}

void MockDisplaySourceConnectionDelegate::NotifySinksUpdated() {
  for (auto& observer : observers_)
    observer.OnSinksUpdated(sinks_);
}

void MockDisplaySourceConnectionDelegate::
EnqueueSinkMessage(std::string message) {
  const std::size_t found_session_key = message.find(kSessionKey);
  if (found_session_key != std::string::npos)
    AdaptMessagePattern(found_session_key, kSessionKey, kSessionIdLength,
                        session_id_, message);

  const std::size_t found_unicast_key = message.find(kUnicastKey);
  if (found_unicast_key != std::string::npos)
    AdaptMessagePattern(found_unicast_key, kUnicastKey, kUdpPortLength,
                        udp_port_, message);

  const std::size_t found_clientport_key = message.find(kClientPortKey);
  if (found_clientport_key != std::string::npos)
    AdaptMessagePattern(found_clientport_key, kClientPortKey, kUdpPortLength,
                        udp_port_, message);

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(message_received_cb_, message));
}

void MockDisplaySourceConnectionDelegate::
CheckSourceMessageContent(std::string pattern,
                          const std::string& message) {
  // Message M6_reply from Source to Sink has a unique and random session id
  // generated by Source. The id cannot be predicted and the session id should
  // be extracted and added to the message pattern for assertion.
  // The following code checks if messages include "Session" string.
  // If not, assert the message normally.
  // If yes, find the session id, add it to the pattern and to the sink message
  // that has Session: substring inside.
  const std::size_t found_session_key = message.find(kSessionKey);
  if (found_session_key != std::string::npos) {
    const std::size_t session_id_pos = found_session_key +
        std::char_traits<char>::length(kSessionKey);
    session_id_ = message.substr(session_id_pos, kSessionIdLength);
    AdaptMessagePattern(found_session_key, kSessionKey, kSessionIdLength,
                        session_id_, pattern);
  }

  const std::size_t found_unicast_key = message.find(kUnicastKey);
  if (found_unicast_key != std::string::npos)
    AdaptMessagePattern(found_unicast_key, kUnicastKey, kUdpPortLength,
                        udp_port_, pattern);

  const std::size_t found_clientport_key = message.find(kClientPortKey);
  if (found_clientport_key != std::string::npos)
    AdaptMessagePattern(found_clientport_key, kClientPortKey, kUdpPortLength,
                        udp_port_, pattern);

  ASSERT_EQ(pattern, message);
}

void MockDisplaySourceConnectionDelegate::BindToUdpSocket() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  socket_.reset(new net::UDPSocket(net::DatagramSocket::DEFAULT_BIND, nullptr,
                                   net::NetLogSource()));

  net::IPAddress address;
  ASSERT_TRUE(address.AssignFromIPLiteral(kLocalHost));

  int net_result;
  net_result = socket_->Open(net::ADDRESS_FAMILY_IPV4);
  ASSERT_EQ(net_result, net::OK);

  for (uint16_t port = kPortStart; port < kPortEnd; ++port) {
    net::IPEndPoint local_point(address, port);
    net_result = socket_->Bind(local_point);
    if (net_result == net::OK) {
      udp_port_ = std::to_string(port);
      // When we got an udp socket established and udp port is known
      // Change sink's status to connected and proceed with the test.
      base::PostTask(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&MockDisplaySourceConnectionDelegate::OnSinkConnected,
                         base::Unretained(this)));
      break;
    }
  }

  ReceiveMediaPacket();
}

void MockDisplaySourceConnectionDelegate::ReceiveMediaPacket() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(socket_.get());
  const int kBufferSize = 512;

  recvfrom_buffer_ = base::MakeRefCounted<net::IOBuffer>(kBufferSize);

  int net_result = socket_->RecvFrom(
      recvfrom_buffer_.get(), kBufferSize, &end_point_,
      base::Bind(&MockDisplaySourceConnectionDelegate::OnMediaPacketReceived,
                 base::Unretained(this)));

  if (net_result != net::ERR_IO_PENDING)
    OnMediaPacketReceived(net_result);
}

void MockDisplaySourceConnectionDelegate::OnMediaPacketReceived(
    int net_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(recvfrom_buffer_.get());
  recvfrom_buffer_.reset();

  if (net_result > 0) {
    // We received at least one media packet.
    // Test is completed.
    socket_->Close();
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&MockDisplaySourceConnectionDelegate::Disconnect,
                       base::Unretained(this), StringCallback()));
    return;
   }

  DCHECK(socket_.get());
  ReceiveMediaPacket();
}

}  // namespace extensions
