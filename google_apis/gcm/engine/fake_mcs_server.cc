// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/fake_mcs_server.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/containers/to_value_list.h"
#include "base/containers/to_vector.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "url/url_constants.h"

namespace gcm {

namespace {

constexpr uint8_t kMCSVersion = 41;
constexpr int kReadBufferSize = 4096;
constexpr size_t kMaxVarint32Bytes = 5;

// Encodes an MCS protobuf packet (`[tag: 1 byte][size: varint32][payload]`).
std::string EncodeMCSPacket(
    const google::protobuf::MessageLite& message,
    std::optional<uint8_t> version_prefix = std::nullopt) {
  std::string packet;
  google::protobuf::io::StringOutputStream string_output(&packet);
  {
    google::protobuf::io::CodedOutputStream coded_output(&string_output);
    if (version_prefix.has_value()) {
      const uint8_t version = *version_prefix;
      coded_output.WriteRaw(&version, 1);
    }
    const int tag = GetMCSProtoTag(message);
    CHECK_GE(tag, 0);
    const uint8_t tag_byte = static_cast<uint8_t>(tag);
    coded_output.WriteRaw(&tag_byte, 1);
    coded_output.WriteVarint32(
        base::checked_cast<uint32_t>(message.ByteSizeLong()));
    CHECK(message.SerializeToCodedStream(&coded_output));
  }
  return packet;
}

struct InvalidPacket {};

// Intentionally uses concrete proto types rather than `MCSMessage` /
// `BuildProtobufFromTag()` from `mcs_util.h` to avoid sharing client code under
// test with the fake server.
using ClientMCSPacket = std::variant<InvalidPacket,
                                     mcs_proto::LoginRequest,
                                     mcs_proto::HeartbeatPing,
                                     mcs_proto::IqStanza,
                                     mcs_proto::DataMessageStanza>;

template <typename T>
ClientMCSPacket ParseMCSProto(std::string_view payload) {
  T proto;
  if (!proto.ParseFromString(payload)) {
    return InvalidPacket{};
  }
  return proto;
}

// Parses a binary MCS packet payload according to `tag`.
ClientMCSPacket ParseClientMCSPacket(uint8_t tag, std::string_view payload) {
  switch (tag) {
    case kLoginRequestTag:
      return ParseMCSProto<mcs_proto::LoginRequest>(payload);
    case kHeartbeatPingTag:
      return ParseMCSProto<mcs_proto::HeartbeatPing>(payload);
    case kIqStanzaTag:
      return ParseMCSProto<mcs_proto::IqStanza>(payload);
    case kDataMessageStanzaTag:
      return ParseMCSProto<mcs_proto::DataMessageStanza>(payload);
    default:
      return InvalidPacket{};
  }
}

// Buffers incoming stream bytes and extracts framed MCS packets.
class MCSPacketReader {
 public:
  MCSPacketReader() = default;
  ~MCSPacketReader() = default;

  void AppendBytes(base::span<const uint8_t> bytes) {
    base::Extend(buffer_, bytes);
  }

  // Attempts to read the next complete MCS packet from the buffer. Returns
  // `std::nullopt` if more bytes are needed, or `InvalidPacket` if the stream
  // framing, version, or protobuf payload is malformed.
  std::optional<ClientMCSPacket> ReadNextPacket() {
    google::protobuf::io::CodedInputStream input(
        buffer_.data(), base::checked_cast<int>(buffer_.size()));
    if (!has_read_version_) {
      uint8_t version = 0;
      if (!input.ReadRaw(&version, 1)) {
        return std::nullopt;
      }
      if (version != kMCSVersion) {
        LOG(ERROR) << "Invalid MCS version: " << static_cast<int>(version);
        return InvalidPacket{};
      }
    }

    uint8_t tag = 0;
    if (!input.ReadRaw(&tag, 1)) {
      return std::nullopt;
    }
    if (!has_read_version_ && tag != kLoginRequestTag) {
      LOG(ERROR) << "Invalid first MCS tag: " << static_cast<int>(tag);
      return InvalidPacket{};
    }

    const int pos_before_size = input.CurrentPosition();
    const int remaining_before_size = input.BytesUntilLimit();
    uint32_t msg_size = 0;
    if (!input.ReadVarint32(&msg_size)) {
      return remaining_before_size < static_cast<int>(kMaxVarint32Bytes)
                 ? std::nullopt
                 : std::optional<ClientMCSPacket>(InvalidPacket{});
    }
    if (input.CurrentPosition() - pos_before_size >
        static_cast<int>(kMaxVarint32Bytes)) {
      return InvalidPacket{};
    }
    if (static_cast<size_t>(input.BytesUntilLimit()) < msg_size) {
      return std::nullopt;
    }

    const size_t header_bytes = static_cast<size_t>(input.CurrentPosition());
    ClientMCSPacket packet = ParseClientMCSPacket(
        tag, base::as_string_view(
                 base::span(buffer_).subspan(header_bytes, msg_size)));
    buffer_.erase(buffer_.begin(), buffer_.begin() + header_bytes + msg_size);
    has_read_version_ = true;
    return packet;
  }

 private:
  std::vector<uint8_t> buffer_;
  bool has_read_version_ = false;
};

}  // namespace

// Thread-safe RMQ2 message queue shared between the caller thread and
// `ServerCore`'s IO thread.
class FakeMCSServer::StateCore
    : public base::RefCountedThreadSafe<FakeMCSServer::StateCore> {
 public:
  StateCore() = default;

  void InitializeStorageDir(const base::FilePath& storage_dir) {
    base::AutoLock auto_lock(lock_);
    state_file_path_ =
        storage_dir.empty()
            ? base::FilePath()
            : storage_dir.AppendASCII("fake_mcs_server_state.json");
    LoadStateFromDiskLocked();
  }

  void SetDataMessageReceivedCallback(DataMessageReceivedCallback callback) {
    base::AutoLock auto_lock(lock_);
    data_message_received_callback_ = std::move(callback);
  }

  void OnClientDataMessageReceived(uint64_t android_id,
                                   const mcs_proto::DataMessageStanza& stanza) {
    DataMessageReceivedCallback callback;
    {
      base::AutoLock auto_lock(lock_);
      callback = data_message_received_callback_;
    }
    if (callback) {
      callback.Run(android_id, stanza);
    }
  }

  std::vector<mcs_proto::DataMessageStanza> OnClientLoggedIn(
      uint64_t android_id,
      const std::vector<std::string>& acknowledged_ids) {
    base::AutoLock auto_lock(lock_);
    OnMessagesAcknowledgedLocked(android_id, acknowledged_ids);
    const std::vector<mcs_proto::DataMessageStanza>* pending =
        base::FindOrNull(pending_messages_, android_id);
    return pending ? *pending : std::vector<mcs_proto::DataMessageStanza>();
  }

  void OnMessagesAcknowledged(
      uint64_t android_id,
      const std::vector<std::string>& acknowledged_ids) {
    base::AutoLock auto_lock(lock_);
    OnMessagesAcknowledgedLocked(android_id, acknowledged_ids);
  }

  bool HasUnacknowledgedMessages(uint64_t android_id) const {
    base::AutoLock auto_lock(lock_);
    const std::vector<mcs_proto::DataMessageStanza>* pending =
        base::FindOrNull(pending_messages_, android_id);
    return pending && !pending->empty();
  }

  mcs_proto::DataMessageStanza EnqueueDataMessage(
      uint64_t android_id,
      mcs_proto::DataMessageStanza stanza) {
    base::AutoLock auto_lock(lock_);
    if (stanza.persistent_id().empty()) {
      const std::string persistent_id = base::StrCat(
          {"mcs_msg_", base::NumberToString(++next_persistent_id_)});
      stanza.set_id(persistent_id);
      stanza.set_persistent_id(persistent_id);
    }

    pending_messages_[android_id].push_back(stanza);
    WriteStateToDiskLocked();
    return stanza;
  }

 private:
  friend class base::RefCountedThreadSafe<StateCore>;

  ~StateCore() = default;

  void OnMessagesAcknowledgedLocked(
      uint64_t android_id,
      const std::vector<std::string>& acknowledged_ids)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    std::vector<mcs_proto::DataMessageStanza>* pending =
        base::FindOrNull(pending_messages_, android_id);
    if (!pending) {
      return;
    }

    if (std::erase_if(*pending,
                      [&](const mcs_proto::DataMessageStanza& stanza) {
                        return std::ranges::contains(acknowledged_ids,
                                                     stanza.persistent_id());
                      }) > 0) {
      WriteStateToDiskLocked();
    }
  }

  void LoadStateFromDiskLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    if (state_file_path_.empty()) {
      return;
    }

    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string json_string;
    if (!base::ReadFileToString(state_file_path_, &json_string)) {
      return;
    }

    std::optional<base::DictValue> root =
        base::JSONReader::ReadDict(json_string, base::JSON_PARSE_RFC);
    CHECK(root.has_value());

    if (std::optional<int> next_pid = root->FindInt("next_persistent_id")) {
      next_persistent_id_ = static_cast<uint64_t>(*next_pid);
    }

    if (const base::DictValue* pending_dict =
            root->FindDict("pending_messages")) {
      for (const auto [aid_str, list_val] : *pending_dict) {
        uint64_t aid = 0;
        if (!base::StringToUint64(aid_str, &aid) || !list_val.is_list()) {
          continue;
        }
        for (const base::Value& item_val : list_val.GetList()) {
          const std::string* encoded_stanza = item_val.GetIfString();
          if (!encoded_stanza) {
            continue;
          }
          std::string serialized_stanza;
          CHECK(base::Base64Decode(*encoded_stanza, &serialized_stanza));
          mcs_proto::DataMessageStanza stanza;
          CHECK(stanza.ParseFromString(serialized_stanza));
          pending_messages_[aid].push_back(std::move(stanza));
        }
      }
    }
  }

  void WriteStateToDiskLocked() const EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    if (state_file_path_.empty()) {
      return;
    }

    base::ScopedAllowBlockingForTesting allow_blocking;
    base::DictValue root;
    root.Set("next_persistent_id", static_cast<int>(next_persistent_id_));

    base::DictValue pending_dict;
    for (const auto& [aid, stanzas] : pending_messages_) {
      pending_dict.Set(
          base::NumberToString(aid),
          base::ToValueList(
              stanzas, [](const mcs_proto::DataMessageStanza& stanza) {
                return base::Base64Encode(stanza.SerializeAsString());
              }));
    }
    root.Set("pending_messages", std::move(pending_dict));

    std::string json_string;
    CHECK(base::JSONWriter::Write(root, &json_string));
    CHECK(base::CreateDirectory(state_file_path_.DirName()));
    CHECK(base::WriteFile(state_file_path_, json_string));
  }

  mutable base::Lock lock_;
  base::FilePath state_file_path_ GUARDED_BY(lock_);
  uint64_t next_persistent_id_ GUARDED_BY(lock_) = 0;
  base::flat_map<uint64_t, std::vector<mcs_proto::DataMessageStanza>>
      pending_messages_ GUARDED_BY(lock_);
  DataMessageReceivedCallback data_message_received_callback_ GUARDED_BY(lock_);
};

// Runs on `io_thread_` to accept TCP connections and speak the binary MCS
// protocol with `gcm::MCSClient`.
class FakeMCSServer::ServerCore {
 public:
  explicit ServerCore(scoped_refptr<StateCore> state_core)
      : state_core_(std::move(state_core)) {}

  ServerCore(const ServerCore&) = delete;
  ServerCore& operator=(const ServerCore&) = delete;
  ~ServerCore() = default;

  net::IPEndPoint InitializeAndListen() {
    constexpr int kMaxTries = 5;
    net::IPEndPoint endpoint;
    for (int num_tries = 1; num_tries <= kMaxTries; ++num_tries) {
      listen_socket_ =
          std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
      int rv = listen_socket_->Listen(
          net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0),
          /*backlog=*/10, /*ipv6_only=*/std::nullopt);
      CHECK_EQ(rv, net::OK) << "Listen failed: " << net::ErrorToString(rv);
      rv = listen_socket_->GetLocalAddress(&endpoint);
      CHECK_EQ(rv, net::OK)
          << "GetLocalAddress failed: " << net::ErrorToString(rv);
      if (net::IsPortAllowedForScheme(endpoint.port(), url::kHttpScheme)) {
        listen_socket_->DetachFromThread();
        return endpoint;
      }
    }
    LOG(FATAL) << "Failed to listen on a valid port after " << kMaxTries
               << " attempts.";
  }

  void StartAcceptingConnections() { DoAcceptLoop(); }

  void DeliverMessage(uint64_t android_id,
                      const mcs_proto::DataMessageStanza& stanza) {
    for (const auto& conn : connections_) {
      if (conn->android_id() == android_id && conn->logged_in()) {
        conn->SendStanza(stanza);
      }
    }
  }

  void DisconnectAllClients() { connections_.clear(); }

 private:
  class Connection {
   public:
    Connection(ServerCore* owner, std::unique_ptr<net::StreamSocket> socket)
        : owner_(owner),
          socket_(std::move(socket)),
          read_io_buf_(
              base::MakeRefCounted<net::IOBufferWithSize>(kReadBufferSize)) {}

    ~Connection() = default;

    uint64_t android_id() const { return android_id_; }
    bool logged_in() const { return logged_in_; }

    void Start() { DoReadLoop(); }

    void SendStanza(const mcs_proto::DataMessageStanza& stanza) {
      if (!stanza.persistent_id().empty()) {
        if (std::ranges::contains(sent_stream_id_to_persistent_id_,
                                  stanza.persistent_id(),
                                  &decltype(sent_stream_id_to_persistent_id_)::
                                      value_type::second)) {
          return;
        }
        sent_stream_id_to_persistent_id_[out_stream_id_ + 1] =
            stanza.persistent_id();
      }
      SendPacket(stanza);
    }

   private:
    template <typename Proto>
    void SendPacket(Proto message,
                    std::optional<uint8_t> version_prefix = std::nullopt) {
      if (!socket_) {
        return;
      }
      ++out_stream_id_;
      message.set_last_stream_id_received(in_stream_id_);
      EnqueueBytes(EncodeMCSPacket(message, version_prefix));
    }

    void DoReadLoop() {
      while (socket_) {
        int rv = socket_->Read(read_io_buf_.get(), kReadBufferSize,
                               base::BindOnce(&Connection::OnReadCompleted,
                                              weak_ptr_factory_.GetWeakPtr()));
        if (rv == net::ERR_IO_PENDING) {
          return;
        }
        if (!HandleReadResult(rv)) {
          return;
        }
      }
    }

    void OnReadCompleted(int rv) {
      if (HandleReadResult(rv)) {
        DoReadLoop();
      }
    }

    bool HandleReadResult(int rv) {
      if (rv <= 0) {
        CloseAndDestroy();
        return false;
      }

      packet_reader_.AppendBytes(
          read_io_buf_->span().first(static_cast<size_t>(rv)));
      while (socket_) {
        std::optional<ClientMCSPacket> packet = packet_reader_.ReadNextPacket();
        if (!packet.has_value()) {
          break;
        }
        ++in_stream_id_;
        if (!std::visit([this](const auto& msg) { return HandlePacket(msg); },
                        *packet)) {
          return false;
        }
      }
      return socket_ != nullptr;
    }

    bool HandlePacket(const InvalidPacket&) {
      LOG(ERROR) << "Failed to parse MCS packet";
      CloseAndDestroy();
      return false;
    }

    bool HandlePacket(const mcs_proto::LoginRequest& login_req) {
      if (!base::StringToUint64(login_req.user(), &android_id_)) {
        CloseAndDestroy();
        return false;
      }

      logged_in_ = true;
      std::vector<mcs_proto::DataMessageStanza> pending_stanzas =
          owner_->state_core_->OnClientLoggedIn(
              android_id_, base::ToVector(login_req.received_persistent_id()));

      mcs_proto::LoginResponse login_resp;
      login_resp.set_id("server");
      login_resp.set_server_timestamp(
          (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds());
      SendPacket(std::move(login_resp), kMCSVersion);

      for (const mcs_proto::DataMessageStanza& stanza : pending_stanzas) {
        SendStanza(stanza);
      }
      return true;
    }

    bool HandlePacket(const mcs_proto::HeartbeatPing& ping) {
      AcknowledgeUpToStreamId(ping.last_stream_id_received());
      SendPacket(mcs_proto::HeartbeatAck());
      return true;
    }

    bool HandlePacket(const mcs_proto::IqStanza& iq) {
      AcknowledgeUpToStreamId(iq.last_stream_id_received());
      return true;
    }

    bool HandlePacket(const mcs_proto::DataMessageStanza& data_stanza) {
      AcknowledgeUpToStreamId(data_stanza.last_stream_id_received());
      owner_->state_core_->OnClientDataMessageReceived(android_id_,
                                                       data_stanza);
      SendPacket(*BuildStreamAck());
      return true;
    }

    void AcknowledgeUpToStreamId(uint32_t stream_id) {
      if (stream_id == 0 || android_id_ == 0) {
        return;
      }

      std::vector<std::string> acked_ids;
      base::EraseIf(sent_stream_id_to_persistent_id_, [&](const auto& entry) {
        if (entry.first <= stream_id) {
          acked_ids.push_back(entry.second);
          return true;
        }
        return false;
      });
      if (!acked_ids.empty()) {
        owner_->state_core_->OnMessagesAcknowledged(android_id_, acked_ids);
        // Confirm receipt of the client's StreamAck so MCSClient removes the
        // acknowledged incoming IDs from its persistent GCMStore.
        SendPacket(*BuildStreamAck());
      }
    }

    void EnqueueBytes(std::string bytes) {
      const size_t size = bytes.size();
      write_queue_.push_back(base::MakeRefCounted<net::DrainableIOBuffer>(
          base::MakeRefCounted<net::StringIOBuffer>(std::move(bytes)), size));
      if (!is_writing_) {
        DoWriteLoop();
      }
    }

    void DoWriteLoop() {
      is_writing_ = true;
      while (!write_queue_.empty() && socket_) {
        scoped_refptr<net::DrainableIOBuffer> buf = write_queue_.front();
        int rv = socket_->Write(buf.get(), buf->BytesRemaining(),
                                base::BindOnce(&Connection::OnWriteCompleted,
                                               weak_ptr_factory_.GetWeakPtr()),
                                TRAFFIC_ANNOTATION_FOR_TESTS);
        if (rv == net::ERR_IO_PENDING) {
          return;
        }
        if (!HandleWriteResult(rv)) {
          return;
        }
      }
      is_writing_ = false;
    }

    void OnWriteCompleted(int rv) {
      if (HandleWriteResult(rv)) {
        DoWriteLoop();
      }
    }

    bool HandleWriteResult(int rv) {
      if (rv <= 0) {
        is_writing_ = false;
        CloseAndDestroy();
        return false;
      }

      CHECK(!write_queue_.empty());
      write_queue_.front()->DidConsume(rv);
      if (write_queue_.front()->BytesRemaining() == 0) {
        write_queue_.pop_front();
      }
      return true;
    }

    void CloseAndDestroy() {
      if (!socket_) {
        return;
      }
      logged_in_ = false;
      socket_.reset();
      write_queue_.clear();
      weak_ptr_factory_.InvalidateWeakPtrs();
      owner_->ScheduleRemoveConnection(weak_ptr_factory_.GetWeakPtr());
    }

    const raw_ptr<ServerCore> owner_;
    std::unique_ptr<net::StreamSocket> socket_;
    scoped_refptr<net::IOBufferWithSize> read_io_buf_;
    MCSPacketReader packet_reader_;
    bool logged_in_ = false;
    uint64_t android_id_ = 0;
    uint32_t in_stream_id_ = 0;
    uint32_t out_stream_id_ = 0;
    base::flat_map<uint32_t, std::string> sent_stream_id_to_persistent_id_;
    std::deque<scoped_refptr<net::DrainableIOBuffer>> write_queue_;
    bool is_writing_ = false;
    base::WeakPtrFactory<Connection> weak_ptr_factory_{this};
  };

  void ScheduleRemoveConnection(base::WeakPtr<Connection> conn) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ServerCore::RemoveConnection,
                       weak_ptr_factory_.GetWeakPtr(), std::move(conn)));
  }

  void RemoveConnection(base::WeakPtr<Connection> conn) {
    if (!conn) {
      return;
    }
    std::erase_if(connections_, base::MatchesUniquePtr(conn.get()));
  }

  void DoAcceptLoop() {
    while (listen_socket_) {
      int rv = listen_socket_->Accept(
          &accepted_socket_, base::BindOnce(&ServerCore::OnAcceptCompleted,
                                            weak_ptr_factory_.GetWeakPtr()));
      if (rv == net::ERR_IO_PENDING) {
        return;
      }
      HandleAcceptResult(rv);
    }
  }

  void OnAcceptCompleted(int rv) {
    HandleAcceptResult(rv);
    DoAcceptLoop();
  }

  void HandleAcceptResult(int rv) {
    if (rv != net::OK || !accepted_socket_) {
      return;
    }

    auto conn = std::make_unique<Connection>(this, std::move(accepted_socket_));
    Connection* conn_ptr = conn.get();
    connections_.push_back(std::move(conn));
    conn_ptr->Start();
  }

  const scoped_refptr<StateCore> state_core_;
  std::unique_ptr<net::TCPServerSocket> listen_socket_;
  std::unique_ptr<net::StreamSocket> accepted_socket_;
  std::vector<std::unique_ptr<Connection>> connections_;
  base::WeakPtrFactory<ServerCore> weak_ptr_factory_{this};
};

FakeMCSServer::FakeMCSServer()
    : state_core_(base::MakeRefCounted<StateCore>()),
      io_thread_("FakeMCSServer IO Thread"),
      server_core_(std::make_unique<ServerCore>(state_core_)) {}

FakeMCSServer::~FakeMCSServer() {
  if (io_thread_.IsRunning()) {
    io_thread_.task_runner()->DeleteSoon(FROM_HERE, std::move(server_core_));
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    io_thread_.Stop();
  }
}

void FakeMCSServer::InitializeAndListen() {
  CHECK(!endpoint_url_.is_valid());
  const net::IPEndPoint listen_endpoint = server_core_->InitializeAndListen();
  endpoint_url_ = GURL(base::StrCat({"http://", listen_endpoint.ToString()}));
}

void FakeMCSServer::Start(const base::FilePath& storage_dir) {
  CHECK(endpoint_url_.is_valid());
  CHECK(!io_thread_.IsRunning());
  state_core_->InitializeStorageDir(storage_dir);

  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  CHECK(io_thread_.StartWithOptions(std::move(options)));
  io_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServerCore::StartAcceptingConnections,
                                base::Unretained(server_core_.get())));
}

GURL FakeMCSServer::GetEndpointURL() const {
  CHECK(endpoint_url_.is_valid());
  return endpoint_url_;
}

void FakeMCSServer::SendDataMessage(uint64_t android_id,
                                    mcs_proto::DataMessageStanza stanza) {
  CHECK(io_thread_.IsRunning());
  mcs_proto::DataMessageStanza queued_stanza =
      state_core_->EnqueueDataMessage(android_id, std::move(stanza));
  io_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServerCore::DeliverMessage,
                                base::Unretained(server_core_.get()),
                                android_id, std::move(queued_stanza)));
}

void FakeMCSServer::SetDataMessageReceivedCallback(
    DataMessageReceivedCallback callback) {
  state_core_->SetDataMessageReceivedCallback(
      callback ? base::BindPostTaskToCurrentDefault(std::move(callback))
               : DataMessageReceivedCallback());
}

void FakeMCSServer::DisconnectAllClients() {
  CHECK(io_thread_.IsRunning());
  base::WaitableEvent done_event;
  io_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServerCore::DisconnectAllClients,
                                base::Unretained(server_core_.get()))
                     .Then(base::BindOnce(&base::WaitableEvent::Signal,
                                          base::Unretained(&done_event))));
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
  done_event.Wait();
}

bool FakeMCSServer::HasUnacknowledgedMessages(uint64_t android_id) const {
  return state_core_->HasUnacknowledgedMessages(android_id);
}

}  // namespace gcm
