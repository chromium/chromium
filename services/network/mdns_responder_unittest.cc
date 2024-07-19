// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/mdns_responder.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/mock_mdns_socket_factory.h"
#include "net/dns/public/dns_protocol.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ServiceError = MdnsResponderManager::ServiceError;

const net::IPAddress kPublicAddrs[2] = {net::IPAddress(11, 11, 11, 11),
                                        net::IPAddress(22, 22, 22, 22)};
const net::IPAddress kPublicAddrsIpv6[2] = {
    net::IPAddress(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16),
    net::IPAddress(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)};

const base::TimeDelta kDefaultTtl = base::Seconds(120);

const int kNumAnnouncementsPerInterface = 2;
const int kNumMaxRetriesPerResponse = 2;

// Keep in sync with |kMdnsNameGeneratorServiceInstanceName| in
// mdns_responder.cc.
const char kMdnsNameGeneratorServiceInstanceName[] =
    "Generated-Names._mdns_name_generator._udp.local";

std::string CreateMdnsQuery(uint16_t query_id,
                            const std::string& dotted_name,
                            uint16_t qtype = net::dns_protocol::kTypeA) {
  std::optional<std::vector<uint8_t>> qname =
      net::dns_names_util::DottedNameToNetwork(dotted_name);
  CHECK(qname.has_value());
  net::DnsQuery query(query_id, qname.value(), qtype);
  return std::string(query.io_buffer()->data(), query.io_buffer()->size());
}

// Creates an mDNS response as announcement, resolution for queries or goodbye.
std::string CreateResolutionResponse(
    const base::TimeDelta& ttl,
    const std::map<std::string, net::IPAddress>& name_addr_map) {
  auto buf = network::mdns_helper::CreateResolutionResponse(ttl, name_addr_map);
  DCHECK(buf != nullptr);
  return std::string(buf->data(), buf->size());
}

std::string CreateNegativeResponse(
    const std::map<std::string, net::IPAddress>& name_addr_map) {
  auto buf = network::mdns_helper::CreateNegativeResponse(name_addr_map);
  DCHECK(buf != nullptr);
  return std::string(buf->data(), buf->size());
}

std::string CreateResponseToMdnsNameGeneratorServiceQuery(
    const base::TimeDelta& ttl,
    const std::set<std::string>& names) {
  auto buf =
      network::mdns_helper::CreateResponseToMdnsNameGeneratorServiceQuery(
          ttl, names);

  DCHECK(buf != nullptr);
  return std::string(buf->data(), buf->size());
}

std::string CreateResponseToMdnsNameGeneratorServiceQueryWithCacheFlush(
    const std::set<std::string>& names) {
  auto buf =
      network::mdns_helper::CreateResponseToMdnsNameGeneratorServiceQuery(
          kDefaultTtl, names);
  // Deserialize to set the cache-flush bit before serializing again.
  net::DnsResponse response(buf.get(), buf->size());
  bool rv = response.InitParseWithoutQuery(buf->size());
  DCHECK(rv);
  DCHECK_EQ(response.answer_count(), 1u);
  net::DnsResourceRecord txt_record;
  rv = response.Parser().ReadRecord(&txt_record);
  DCHECK(rv);
  DCHECK_EQ(net::dns_protocol::kTypeTXT, txt_record.type);
  txt_record.klass |= net::dns_protocol::kFlagCacheFlush;
  // Parsed record does not own the RDATA. Copy the owned RDATA before
  // constructing a new response.
  const std::string owned_rdata(txt_record.rdata);
  txt_record.SetOwnedRdata(owned_rdata);
  std::vector<net::DnsResourceRecord> answers(1, txt_record);
  net::DnsResponse response_cache_flush(
      /*id=*/0, /*is_authoritative=*/true, answers, /*authority_records=*/{},
      /*additional_records=*/{},
      /*query=*/std::nullopt,
      /*rcode=*/net::dns_protocol::kRcodeNOERROR,
      /*validate_records=*/true,
      /*validate_names_as_internet_hostnames=*/false);
  DCHECK(response_cache_flush.io_buffer() != nullptr);
  buf = base::MakeRefCounted<net::IOBufferWithSize>(
      response_cache_flush.io_buffer_size());
  memcpy(buf->data(), response_cache_flush.io_buffer()->data(),
         response_cache_flush.io_buffer_size());
  return std::string(buf->data(), buf->size());
}

// A mock mDNS socket factory to create sockets that can fail sending or
// receiving packets.
class MockFailingMdnsSocketFactory : public net::MDnsSocketFactory {
 public:
  MockFailingMdnsSocketFactory(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}

  ~MockFailingMdnsSocketFactory() override = default;

  MOCK_METHOD1(CreateSockets,
               void(std::vector<std::unique_ptr<net::DatagramServerSocket>>*));

  MOCK_METHOD1(OnSendTo, void(const std::string&));

  // Emulates the asynchronous contract of invoking |callback| in the SendTo
  // primitive but failed sending;
  int FailToSend(const std::string& packet,
                 const std::string& address,
                 net::CompletionOnceCallback callback) {
    OnSendTo(packet);
    task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), -1));
    return -1;
  }

  // Emulates IO blocking in sending packets if |BlockSend()| is called, in
  // which case the completion callback is not invoked until |ResumeSend()| is
  // called.
  int MaybeBlockSend(const std::string& packet,
                     const std::string& address,
                     net::CompletionOnceCallback callback) {
    OnSendTo(packet);
    if (block_send_) {
      blocked_packet_size_ = packet.size();
      blocked_send_callback_ = std::move(callback);
    } else {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), packet.size()));
    }
    return -1;
  }

  void BlockSend() {
    DCHECK(!block_send_);
    block_send_ = true;
  }

  void ResumeSend() {
    DCHECK(block_send_);
    block_send_ = false;
    std::move(blocked_send_callback_).Run(blocked_packet_size_);
  }

  // Emulates the asynchronous contract of invoking |callback| in the RecvFrom
  // primitive but failed receiving;
  int FailToRecv(net::IOBuffer* buffer,
                 int size,
                 net::IPEndPoint* address,
                 net::CompletionOnceCallback callback) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), net::ERR_FAILED));
    return net::ERR_IO_PENDING;
  }

 private:
  bool block_send_ = false;
  size_t blocked_packet_size_ = 0;
  net::CompletionOnceCallback blocked_send_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace

// Tests of the response creation helpers. For positive responses, we have the
// address records in the Answer section and, if TTL is nonzero, the
// corresponding NSEC records in the Additional section. For negative responses,
// the NSEC records are placed in the Answer section with the address records in
// the Answer section.
TEST(CreateMdnsResponseTest, SingleARecordAnswer) {
  const uint8_t response_data[]{
      0x00, 0x00,  // mDNS response ID mus be zero.
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x00,  // number of questions
      0x00, 0x01,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x01,  // number of additional rr
      0x03, 'w', 'w', 'w', 0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c',
      'o', 'm',
      0x00,                    // null label
      0x00, 0x01,              // type A Record
      0x80, 0x01,              // class IN, cache-flush bit set
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x04,              // rdlength, 32 bits
      0xc0, 0xa8, 0x00, 0x01,  // 192.168.0.1
      // Additional section
      0x03, 'w', 'w', 'w', 0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c',
      'o', 'm',
      0x00,                    // null label
      0x00, 0x2f,              // type NSEC Record
      0x80, 0x01,              // class IN, cache-flush bit set
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x05,              // rdlength, 5 bytes
      0xc0, 0x2b,              // pointer to the previous "www.example.com"
      0x00, 0x01, 0x40,        // type bit map of type A: window block 0, bitmap
                               // length 1, bitmap with bit 1 set
  };

  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response = CreateResolutionResponse(
      kDefaultTtl,
      {{"www.example.com", net::IPAddress(0xc0, 0xa8, 0x00, 0x01)}});
  EXPECT_EQ(expected_response, actual_response);
}

TEST(CreateMdnsResponseTest, SingleARecordGoodbye) {
  const uint8_t response_data[]{
      0x00, 0x00,  // mDNS response ID mus be zero.
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x00,  // number of questions
      0x00, 0x01,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
      0x03, 'w',  'w',  'w',  0x07, 'e', 'x', 'a',
      'm',  'p',  'l',  'e',  0x03, 'c', 'o', 'm',
      0x00,                    // null label
      0x00, 0x01,              // type A Record
      0x80, 0x01,              // class IN, cache-flush bit set
      0x00, 0x00, 0x00, 0x00,  // zero TTL
      0x00, 0x04,              // rdlength, 32 bits
      0xc0, 0xa8, 0x00, 0x01,  // 192.168.0.1
  };

  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response = CreateResolutionResponse(
      base::TimeDelta(),
      {{"www.example.com", net::IPAddress(0xc0, 0xa8, 0x00, 0x01)}});
  EXPECT_EQ(expected_response, actual_response);
}

TEST(CreateMdnsResponseTest, SingleQuadARecordAnswer) {
  const uint8_t response_data[] = {
      0x00, 0x00,  // mDNS response ID mus be zero.
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x00,  // number of questions
      0x00, 0x01,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x01,  // number of additional rr
      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'o', 'r', 'g',
      0x00,                    // null label
      0x00, 0x1c,              // type AAAA Record
      0x80, 0x01,              // class IN, cache-flush bit set
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x10,              // rdlength, 128 bits
      0xfd, 0x12, 0x34, 0x56, 0x78, 0x9a, 0x00, 0x01,  // fd12:3456:789a:1::1
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
      // Additional section
      0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'o', 'r', 'g',
      0x00,                    // null label
      0x00, 0x2f,              // type NSEC Record
      0x80, 0x01,              // class IN, cache-flush bit set
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x08,              // rdlength, 8 bytes
      0xc0, 0x33,              // pointer to the previous "example.org"
      0x00, 0x04, 0x00, 0x00, 0x00,
      0x08,  // type bit map of type AAAA: window block 0, bitmap
             // length 4, bitmap with bit 28 set
  };
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response = CreateResolutionResponse(
      kDefaultTtl,
      {{"example.org",
        net::IPAddress(0xfd, 0x12, 0x34, 0x56, 0x78, 0x9a, 0x00, 0x01, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01)}});
  EXPECT_EQ(expected_response, actual_response);
}

TEST(CreateMdnsResponseTest, SingleNsecRecordAnswer) {
  const uint8_t response_data[] = {
      0x00, 0x00,  // mDNS response ID mus be zero.
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x00,  // number of questions
      0x00, 0x01,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x01,  // number of additional rr
      0x03, 'w', 'w', 'w', 0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c',
      'o', 'm',
      0x00,                    // null label
      0x00, 0x2f,              // type NSEC Record
      0x80, 0x01,              // class IN, cache-flush bit set
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x05,              // rdlength, 5 bytes
      0xc0, 0x0c,              // pointer to the previous "www.example.com"
      0x00, 0x01, 0x40,        // type bit map of type A: window block 0, bitmap
                               // length 1, bitmap with bit 1 set
      // Additional section
      0x03, 'w', 'w', 'w', 0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 0x03, 'c',
      'o', 'm',
      0x00,                    // null label
      0x00, 0x01,              // type A Record
      0x80, 0x01,              // class IN, cache-flush bit set
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x04,              // rdlength, 32 bits
      0xc0, 0xa8, 0x00, 0x01,  // 192.168.0.1
  };
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response = CreateNegativeResponse(
      {{"www.example.com", net::IPAddress(0xc0, 0xa8, 0x00, 0x01)}});
  EXPECT_EQ(expected_response, actual_response);
}

TEST(CreateMdnsResponseTest,
     SingleTxtRecordAnswerToMdnsNameGeneratorServiceQuery) {
  const uint8_t response_data[] = {
      0x00, 0x00,  // mDNS response ID mus be zero.
      0x84, 0x00,  // flags, response with authoritative answer
      0x00, 0x00,  // number of questions
      0x00, 0x01,  // number of answer rr
      0x00, 0x00,  // number of name server rr
      0x00, 0x00,  // number of additional rr
      0x0f, 'G',  'e',  'n',  'e',  'r', 'a',  't', 'e', 'd', '-', 'N',
      'a',  'm',  'e',  's',  0x14, '_', 'm',  'd', 'n', 's', '_', 'n',
      'a',  'm',  'e',  '_',  'g',  'e', 'n',  'e', 'r', 'a', 't', 'o',
      'r',  0x04, '_',  'u',  'd',  'p', 0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,                    // null label
      0x00, 0x10,              // type A Record
      0x00, 0x01,              // class IN, cache-flush bit NOT set
      0x00, 0x00, 0x00, 0x78,  // TTL, 120 seconds
      0x00, 0x2e,              // rdlength, 46 bytes for the following kv pairs
      0x0d, 'n',  'a',  'm',  'e',  '0', '=',  '1', '.', 'l', 'o', 'c',
      'a',  'l',  0x15, 'n',  'a',  'm', 'e',  '1', '=', 'w', 'w', 'w',
      '.',  'e',  'x',  'a',  'm',  'p', 'l',  'e', '.', 'c', 'o', 'm',
      0x09, 't',  'x',  't',  'v',  'e', 'r',  's', '=', '1'};
  std::string expected_response(reinterpret_cast<const char*>(response_data),
                                sizeof(response_data));
  std::string actual_response = CreateResponseToMdnsNameGeneratorServiceQuery(
      kDefaultTtl, {"1.local", "www.example.com"});
  EXPECT_EQ(expected_response, actual_response);
}

class SimpleNameGenerator : public MdnsResponderManager::NameGenerator {
 public:
  std::string CreateName() override {
    return base::NumberToString(next_available_id_++);
  }

 private:
  uint32_t next_available_id_ = 0;
};

// Test suite for the mDNS responder utilities provided by the service.
class MdnsResponderTest : public testing::Test {
 public:
  MdnsResponderTest()
      : failing_socket_factory_(task_environment_.GetMainThreadTaskRunner()) {
    feature_list_.InitAndEnableFeature(
        features::kMdnsResponderGeneratedNameListing);
    Reset();
  }

  ~MdnsResponderTest() override {
    // Goodbye messages are scheduled when the responder service |host_manager_|
    // is destroyed and can be synchronously sent if the rate limiting permits.
    // See ResponseScheduler::DispatchPendingPackets().
    EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(AnyNumber());
    EXPECT_CALL(failing_socket_factory_, OnSendTo(_)).Times(AnyNumber());
  }

  void Reset(bool use_failing_socket_factory = false) {
    client_[0].reset();
    client_[1].reset();
    if (use_failing_socket_factory)
      host_manager_ =
          std::make_unique<MdnsResponderManager>(&failing_socket_factory_);
    else
      host_manager_ = std::make_unique<MdnsResponderManager>(&socket_factory_);

    host_manager_->SetNameGeneratorForTesting(
        std::make_unique<SimpleNameGenerator>());
    host_manager_->SetTickClockForTesting(task_environment_.GetMockTickClock());
    CreateMdnsResponders();
  }

  void CreateMdnsResponders() {
    host_manager_->CreateMdnsResponder(client_[0].BindNewPipeAndPassReceiver());
    client_[0].set_disconnect_handler(base::BindOnce(
        &MdnsResponderTest::OnMojoConnectionError, base::Unretained(this), 0));
    host_manager_->CreateMdnsResponder(client_[1].BindNewPipeAndPassReceiver());
    client_[1].set_disconnect_handler(base::BindOnce(
        &MdnsResponderTest::OnMojoConnectionError, base::Unretained(this), 1));
  }

  // The following method is synchronous for testing by waiting on running the
  // task runner.
  std::string CreateNameForAddress(int client_id, const net::IPAddress& addr) {
    client_[client_id]->CreateNameForAddress(
        addr, base::BindOnce(&MdnsResponderTest::OnNameCreatedForAddress,
                             base::Unretained(this), addr));
    RunUntilNoTasksRemain();
    return last_name_created_;
  }

  void RemoveNameForAddress(int client_id,
                            const net::IPAddress& addr,
                            bool expected_removed,
                            bool expected_goodbye_sched) {
    client_[client_id]->RemoveNameForAddress(
        addr, base::BindOnce(&MdnsResponderTest::OnNameRemovedForAddress,
                             base::Unretained(this), expected_removed,
                             expected_goodbye_sched));
  }

  void RemoveNameForAddressAndExpectDone(int client_id,
                                         const net::IPAddress& addr) {
    RemoveNameForAddress(client_id, addr, true, true);
  }

  void CloseConnectionToResponderHost(int client_id) {
    client_[client_id].reset();
  }

  void OnMojoConnectionError(int client_id) { client_[client_id].reset(); }

 protected:
  void OnNameCreatedForAddress(const net::IPAddress& addr,
                               const std::string& name,
                               bool announcement_scheduled) {
    last_name_created_ = name;
  }

  void OnNameRemovedForAddress(bool expected_removed,
                               bool expected_goodbye_scheduled,
                               bool actual_removed,
                               bool actual_goodbye_scheduled) {
    EXPECT_EQ(expected_removed, actual_removed);
    EXPECT_EQ(expected_goodbye_scheduled, actual_goodbye_scheduled);
  }

  void RunUntilNoTasksRemain() {
    task_environment_.FastForwardUntilNoTasksRemain();
  }
  void RunFor(base::TimeDelta duration) {
    task_environment_.FastForwardBy(duration);
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  // Overrides the current thread task runner, so we can simulate the passage
  // of time and avoid any actual sleeps.
  NiceMock<net::MockMDnsSocketFactory> socket_factory_;
  NiceMock<MockFailingMdnsSocketFactory> failing_socket_factory_;
  mojo::Remote<mojom::MdnsResponder> client_[2];
  std::unique_ptr<MdnsResponderManager> host_manager_;
  std::string last_name_created_;
};

// Test that a name-to-address map does not change for the same client after
// it is created.
TEST_F(MdnsResponderTest, PersistentNameAddressMapForTheSameClient) {
  const auto& addr1 = kPublicAddrs[0];
  const auto& addr2 = kPublicAddrs[1];
  const auto name1 = CreateNameForAddress(0, addr1);
  const auto name2 = CreateNameForAddress(0, addr2);
  EXPECT_NE(name1, name2);
  EXPECT_EQ(name1, CreateNameForAddress(0, addr1));
}

// Test that a name-to-address map can be removed when reaching zero refcount
// and can be updated afterwards.
TEST_F(MdnsResponderTest, NameAddressMapCanBeRemovedByOwningClient) {
  const auto& addr = kPublicAddrs[0];
  const auto prev_name = CreateNameForAddress(0, addr);
  RemoveNameForAddressAndExpectDone(0, addr);
  RunUntilNoTasksRemain();
  EXPECT_NE(prev_name, CreateNameForAddress(0, addr));
}

// Test that a name-to-address map is not removed with a positive refcount.
TEST_F(MdnsResponderTest,
       NameAddressMapCanOnlyBeRemovedWhenReachingZeroRefcount) {
  const auto& addr = kPublicAddrs[0];
  const auto prev_name = CreateNameForAddress(0, addr);
  EXPECT_EQ(prev_name, CreateNameForAddress(0, addr));
  RemoveNameForAddress(0, addr, false /* expected removed */,
                       false /* expected goodbye scheduled */);
  RunUntilNoTasksRemain();
  EXPECT_EQ(prev_name, CreateNameForAddress(0, addr));
}

// Test that different clients have isolated space of name-to-address maps.
TEST_F(MdnsResponderTest, ClientsHaveIsolatedNameSpaceForAddresses) {
  const net::IPAddress& addr = kPublicAddrs[0];
  // The same address is mapped to different names for different clients.
  const auto name_client1 = CreateNameForAddress(0, addr);
  const auto name_client2 = CreateNameForAddress(1, addr);
  EXPECT_NE(name_client1, name_client2);
  // Removing a name-address association by client 2 does not change the
  // mapping for client 1.
  RemoveNameForAddressAndExpectDone(1, addr);
  EXPECT_EQ(name_client1, CreateNameForAddress(0, addr));
}

// Test that the mDNS responder sends an mDNS response to announce the
// ownership of an address and its newly mapped name, but not for a previously
// announced name-to-address map.
TEST_F(MdnsResponderTest,
       CreatingNameForAddressOnlySendsAnnouncementForNewName) {
  const net::IPAddress& addr = kPublicAddrs[0];

  std::string expected_announcement =
      CreateResolutionResponse(kDefaultTtl, {{"0.local", addr}});

  // MockMdnsSocketFactory binds sockets to two interfaces.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement))
      .Times(2 * kNumAnnouncementsPerInterface);
  EXPECT_EQ("0.local", CreateNameForAddress(0, addr));  // SimpleNameGenerator.
  // Sends no announcement for a name that is already mapped to an address.
  CreateNameForAddress(0, addr);
  RunUntilNoTasksRemain();
}

// Test that the announcements are sent for isolated spaces of name-to-address
// maps owned by different clients.
TEST_F(MdnsResponderTest,
       CreatingNamesForSameAddressButTwoClientsSendsDistinctAnnouncements) {
  const auto& addr = kPublicAddrs[0];

  std::string expected_announcement1 =
      CreateResolutionResponse(kDefaultTtl, {{"0.local", addr}});
  std::string expected_announcement2 =
      CreateResolutionResponse(kDefaultTtl, {{"1.local", addr}});

  // MockMdnsSocketFactory binds sockets to two interfaces.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement1))
      .Times(2 * kNumAnnouncementsPerInterface);
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement2))
      .Times(2 * kNumAnnouncementsPerInterface);
  CreateNameForAddress(0, addr);
  CreateNameForAddress(1, addr);
  RunUntilNoTasksRemain();
}

// Test that the goodbye message with zero TTL for a name is sent only
// when we remove a name in an existing name-to-address map.
TEST_F(MdnsResponderTest,
       RemovingNameForAddressOnlySendsResponseWithZeroTtlForExistingName) {
  const auto& addr = kPublicAddrs[0];

  std::string expected_announcement =
      CreateResolutionResponse(kDefaultTtl, {{"0.local", addr}});
  std::string expected_goodbye =
      CreateResolutionResponse(base::TimeDelta(), {{"0.local", addr}});

  // MockMdnsSocketFactory binds sockets to two interfaces.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement))
      .Times(2 * kNumAnnouncementsPerInterface);
  EXPECT_CALL(socket_factory_, OnSendTo(expected_goodbye)).Times(2);

  CreateNameForAddress(0, addr);
  RemoveNameForAddressAndExpectDone(0, addr);
  // Sends no goodbye message for a name that is already removed.
  RemoveNameForAddress(0, addr, false /* expected removed */,
                       false /* expected goodbye scheduled */);
  RunUntilNoTasksRemain();
}

// Test that the responder can reply to an incoming query about a name it
// knows.
TEST_F(MdnsResponderTest, SendResponseToQueryForOwnedName) {
  const auto& addr = kPublicAddrs[0];
  const auto name = CreateNameForAddress(0, addr);

  std::string query1 = CreateMdnsQuery(0, name);
  std::string query2 = CreateMdnsQuery(0, "unknown_name");

  std::string expected_response =
      CreateResolutionResponse(kDefaultTtl, {{name, addr}});

  // SimulateReceive only lets the last created socket receive.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response)).Times(1);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query1.data()), query1.size());
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query2.data()), query2.size());
  RunUntilNoTasksRemain();
}

// Test that the responder does not respond to any query about a name that is
// unknown to it.
TEST_F(MdnsResponderTest, SendNoResponseToQueryForRemovedName) {
  const auto& addr = kPublicAddrs[0];
  const auto name = CreateNameForAddress(0, addr);
  RemoveNameForAddressAndExpectDone(0, addr);
  RunUntilNoTasksRemain();

  std::string query = CreateMdnsQuery(0, {name});

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(0);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  RunUntilNoTasksRemain();
}

// Test that the responder sends a negative response to any query that is not
// of type A, AAAA, or ANY.
TEST_F(MdnsResponderTest, SendNegativeResponseToQueryForNonAddressRecord) {
  const auto& addr = kPublicAddrs[0];
  const auto name = CreateNameForAddress(0, addr);
  const std::set<uint16_t> non_address_qtypes = {
      net::dns_protocol::kTypeCNAME, net::dns_protocol::kTypeSOA,
      net::dns_protocol::kTypePTR,   net::dns_protocol::kTypeTXT,
      net::dns_protocol::kTypeSRV,   net::dns_protocol::kTypeOPT,
      net::dns_protocol::kTypeNSEC,
  };

  std::string expected_negative_response =
      CreateNegativeResponse({{name, addr}});
  for (auto qtype : non_address_qtypes) {
    std::string query = CreateMdnsQuery(0, {name}, qtype);
    EXPECT_CALL(socket_factory_, OnSendTo(expected_negative_response)).Times(1);
    socket_factory_.SimulateReceive(
        reinterpret_cast<const uint8_t*>(query.data()), query.size());
    RunUntilNoTasksRemain();
  }
}

// Test that the mDNS responder service can respond to an mDNS name generator
// service query with all existing names.
TEST_F(MdnsResponderTest,
       SendResponseToMdnsNameGeneratorServiceQueryWithAllExistingNames) {
  const auto& addr1 = kPublicAddrs[0];
  const auto& addr2 = kPublicAddrs[1];
  // Let two names be created by different clients.
  const std::string name1 = CreateNameForAddress(0, addr1);
  const std::string name2 = CreateNameForAddress(1, addr2);

  const std::string query = CreateMdnsQuery(
      0, kMdnsNameGeneratorServiceInstanceName, net::dns_protocol::kTypeTXT);
  // The response should contain both names.
  const std::string expected_response1 =
      CreateResponseToMdnsNameGeneratorServiceQuery(kDefaultTtl,
                                                    {name1, name2});

  EXPECT_CALL(socket_factory_, OnSendTo(expected_response1)).Times(1);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  RunUntilNoTasksRemain();

  // Remove |name2|.
  std::string expected_goodbye =
      CreateResolutionResponse(base::TimeDelta(), {{name2, addr2}});
  // Goodbye on both interfaces.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_goodbye)).Times(2);
  RemoveNameForAddressAndExpectDone(1 /* client_id */, addr2);
  RunUntilNoTasksRemain();

  // The response should contain only |name1|.
  const std::string expected_response2 =
      CreateResponseToMdnsNameGeneratorServiceQuery(kDefaultTtl, {name1});
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response2)).Times(1);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  RunUntilNoTasksRemain();
}

// Test that the responder manager closes the connection after
// an invalid IP address is given to create a name for.
TEST_F(MdnsResponderTest,
       HostClosesMojoConnectionWhenCreatingNameForInvalidAddress) {
  base::HistogramTester tester;
  const net::IPAddress addr;
  ASSERT_TRUE(!addr.IsValid());
  EXPECT_TRUE(client_[0].is_bound());
  // No packet should be sent out from interfaces.
  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(0);
  CreateNameForAddress(0, addr);
  EXPECT_FALSE(client_[0].is_bound());
}

// Test that the responder manager closes the connection after observing
// conflicting name resolution in the network.
TEST_F(MdnsResponderTest,
       HostClosesMojoConnectionAfterObservingAddressNameConflict) {
  const auto& addr1 = kPublicAddrs[0];
  const auto& addr2 = kPublicAddrs[1];
  const auto name1 = CreateNameForAddress(0, addr1);
  const auto name2 = CreateNameForAddress(0, addr2);

  std::string query1 = CreateMdnsQuery(0, {name1});
  std::string query2 = CreateMdnsQuery(0, {name2});

  std::string conflicting_response =
      CreateResolutionResponse(kDefaultTtl, {{name1, addr2}});

  std::string expected_goodbye = CreateResolutionResponse(
      base::TimeDelta(), {{name1, addr1}, {name2, addr2}});

  EXPECT_TRUE(client_[0].is_bound());
  // MockMdnsSocketFactory binds sockets to two interfaces.
  // We should send only two goodbyes before closing the connection and no
  // packet should be sent out from interfaces after the connection is closed.
  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(0);
  EXPECT_CALL(socket_factory_, OnSendTo(expected_goodbye)).Times(2);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(conflicting_response.data()),
      conflicting_response.size());
  RunUntilNoTasksRemain();
  // The responder should have observed the conflict and the responder manager
  // should have closed the Mojo connection and sent out the goodbye messages
  // for owned names.
  EXPECT_FALSE(client_[0].is_bound());
  // Also, as a result, we should have stopped responding to the following
  // queries.
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query1.data()), query1.size());
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query2.data()), query2.size());
  RunUntilNoTasksRemain();
}

// Test that we stop sending response to the mDNS name generator service queries
// when there is an external response carrying a TXT record with the same
// service instance name and the cache-flush bit set.
TEST_F(MdnsResponderTest,
       StopRespondingToGeneratorServiceQueryAfterObservingTxtNameConflict) {
  const auto& addr = kPublicAddrs[0];
  const std::string name = CreateNameForAddress(0, addr);

  const std::string query = CreateMdnsQuery(
      0, kMdnsNameGeneratorServiceInstanceName, net::dns_protocol::kTypeTXT);
  // Verify that we can respond to the service query.
  const std::string expected_response =
      CreateResponseToMdnsNameGeneratorServiceQuery(kDefaultTtl, {name});
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response)).Times(1);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  RunUntilNoTasksRemain();

  // Receive a conflicting response.
  const std::string conflicting_response =
      CreateResponseToMdnsNameGeneratorServiceQueryWithCacheFlush(
          {"dummy.local"});
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(conflicting_response.data()),
      conflicting_response.size());
  RunUntilNoTasksRemain();

  // We should have stopped responding to service queries.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response)).Times(0);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  RunUntilNoTasksRemain();
}

// Test that an external response with the same service instance name but
// without the cache-flush bit set is not considered a conflict.
TEST_F(MdnsResponderTest,
       NoConflictResolutionIfCacheFlushBitSetInExternalResponse) {
  const auto& addr = kPublicAddrs[0];
  const std::string name = CreateNameForAddress(0, addr);

  // Receive an external response to the same instance name but without the
  // cache-flush bit set in the TXT record.
  const std::string nonconflict_response =
      CreateResponseToMdnsNameGeneratorServiceQuery(kDefaultTtl,
                                                    {"dummy.local"});
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(nonconflict_response.data()),
      nonconflict_response.size());
  RunUntilNoTasksRemain();

  const std::string query = CreateMdnsQuery(
      0, kMdnsNameGeneratorServiceInstanceName, net::dns_protocol::kTypeTXT);
  // We can still respond to the service query.
  const std::string expected_response =
      CreateResponseToMdnsNameGeneratorServiceQuery(kDefaultTtl, {name});
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response)).Times(1);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  RunUntilNoTasksRemain();
}

// Test that scheduled responses to mDNS name generator service queries can be
// cancelled after observing conflict with external records.
TEST_F(MdnsResponderTest,
       CancelResponseToGeneratorServiceQueryAfterObservingTxtNameConflict) {
  const auto& addr = kPublicAddrs[0];
  const std::string name = CreateNameForAddress(0, addr);

  // Receive a series of queries so that we have delayed responses scheduled
  // because of rate limiting. We will also receive a conflicting response after
  // the first response sent to cancel the subsequent ones.
  const std::string query = CreateMdnsQuery(
      0, kMdnsNameGeneratorServiceInstanceName, net::dns_protocol::kTypeTXT);
  const std::string expected_response =
      CreateResponseToMdnsNameGeneratorServiceQuery(kDefaultTtl, {name});
  // We should have only the first response sent and the rest cancelled after
  // encountering the conflicting.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response)).Times(1);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  RunFor(base::Milliseconds(900));

  // Receive a conflicting response.
  const std::string conflicting_response =
      CreateResponseToMdnsNameGeneratorServiceQueryWithCacheFlush(
          {"dummy.local"});
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(conflicting_response.data()),
      conflicting_response.size());

  RunUntilNoTasksRemain();
}

// Test that if we ever send any response to mDNS name generator service
// queries, a goodbye packet is sent when the responder manager is destroyed.
TEST_F(MdnsResponderTest,
       SendGoodbyeForMdnsNameGeneratorServiceAfterManagerDestroyed) {
  const auto& addr = kPublicAddrs[0];
  const std::string name = CreateNameForAddress(0, addr);

  const std::string query = CreateMdnsQuery(
      0, kMdnsNameGeneratorServiceInstanceName, net::dns_protocol::kTypeTXT);
  const std::string expected_response =
      CreateResponseToMdnsNameGeneratorServiceQuery(kDefaultTtl, {name});
  // Respond to a generator service query once.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response)).Times(1);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  RunFor(base::Milliseconds(1000));

  // Goodbye on both interfaces.
  const std::string expected_goodbye =
      CreateResponseToMdnsNameGeneratorServiceQuery(base::TimeDelta(), {name});
  EXPECT_CALL(socket_factory_, OnSendTo(expected_goodbye)).Times(2);
  host_manager_ = nullptr;
  RunUntilNoTasksRemain();
}

// Test that we do not send any goodbye packet to flush TXT records of
// owned names when the responder manager is destroyed, if we have not sent any
// response to mDNS name generator service queries.
TEST_F(MdnsResponderTest,
       NoGoodbyeForMdnsNameGeneratorServiceIfNoPreviousServiceResponseSent) {
  const auto& addr = kPublicAddrs[0];
  const std::string name = CreateNameForAddress(0, addr);

  const std::string goodbye =
      CreateResponseToMdnsNameGeneratorServiceQuery(base::TimeDelta(), {name});
  EXPECT_CALL(socket_factory_, OnSendTo(goodbye)).Times(0);
  host_manager_ = nullptr;
  RunUntilNoTasksRemain();
}

// Test that the responder host clears all name-address maps in one goodbye
// message with zero TTL for a client after the Mojo connection between them is
// lost.
TEST_F(MdnsResponderTest, ResponderHostDoesCleanUpAfterMojoConnectionError) {
  const auto& addr1 = kPublicAddrs[0];
  const auto& addr2 = kPublicAddrs[1];
  const auto name1 = CreateNameForAddress(0, addr1);
  const auto name2 = CreateNameForAddress(0, addr2);

  std::string expected_goodbye = CreateResolutionResponse(
      base::TimeDelta(), {{name1, addr1}, {name2, addr2}});
  // MockMdnsSocketFactory binds sockets to two interfaces.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_goodbye)).Times(2);

  CloseConnectionToResponderHost(0);
  RunUntilNoTasksRemain();
}

// Test that the host generates a Mojo connection error when no socket handler
// is successfully started, and subsequent retry attempts are throttled.
TEST_F(MdnsResponderTest, ClosesBindingWhenNoSocketHanlderStarted) {
  // Expect only one attempt to create sockets before start throttling prevents
  // further attempts.
  EXPECT_CALL(failing_socket_factory_, CreateSockets(_)).WillOnce(Return());
  Reset(true /* use_failing_socket_factory */);
  RunUntilNoTasksRemain();
  // MdnsResponderTest::OnMojoConnectionError.
  EXPECT_FALSE(client_[0].is_bound());
  EXPECT_FALSE(client_[1].is_bound());

  // Little extra fudge around throttle delays as it is not essential for it to
  // be precise, and don't need the test to be too restrictive.
  const base::TimeDelta kThrottleFudge = base::Milliseconds(2);

  // Expect socket creation to not be attempted again too soon.
  EXPECT_CALL(failing_socket_factory_, CreateSockets(_)).Times(0);
  RunFor(MdnsResponderManager::kManagerStartThrottleDelay - kThrottleFudge);
  CreateMdnsResponders();
  RunUntilNoTasksRemain();
  EXPECT_FALSE(client_[0].is_bound());
  EXPECT_FALSE(client_[1].is_bound());

  // Expect no change for subsequent responder creation attempts if socket
  // creation still fails.
  EXPECT_CALL(failing_socket_factory_, CreateSockets(_)).WillOnce(Return());
  RunFor(2 * kThrottleFudge);
  CreateMdnsResponders();
  RunUntilNoTasksRemain();
  EXPECT_FALSE(client_[0].is_bound());
  EXPECT_FALSE(client_[1].is_bound());

  // Expect socket creation to not be attempted again too soon.
  EXPECT_CALL(failing_socket_factory_, CreateSockets(_)).Times(0);
  RunFor(MdnsResponderManager::kManagerStartThrottleDelay - kThrottleFudge);
  CreateMdnsResponders();
  RunUntilNoTasksRemain();
  EXPECT_FALSE(client_[0].is_bound());
  EXPECT_FALSE(client_[1].is_bound());

  // Simulate socket creation fixing itself, and expect responder creation
  // should be able to succeed through retry.
  EXPECT_CALL(failing_socket_factory_, CreateSockets(_))
      .WillOnce(
          Invoke(&socket_factory_, &net::MockMDnsSocketFactory::CreateSockets));
  RunFor(2 * kThrottleFudge);
  CreateMdnsResponders();
  RunUntilNoTasksRemain();
  EXPECT_TRUE(client_[0].is_bound());
  EXPECT_TRUE(client_[1].is_bound());

  // After success, new responders can be created without repeating socket
  // creation.
  EXPECT_CALL(failing_socket_factory_, CreateSockets(_)).Times(0);
  RunFor(MdnsResponderManager::kManagerStartThrottleDelay + kThrottleFudge);
  mojo::Remote<mojom::MdnsResponder> responder;
  host_manager_->CreateMdnsResponder(responder.BindNewPipeAndPassReceiver());
  RunUntilNoTasksRemain();
  EXPECT_TRUE(responder.is_bound());
}

// Test that an announcement is retried after send failure.
TEST_F(MdnsResponderTest, AnnouncementRetriedAfterSendFailure) {
  auto create_send_failing_socket =
      [this](std::vector<std::unique_ptr<net::DatagramServerSocket>>* sockets) {
        auto socket =
            std::make_unique<NiceMock<net::MockMDnsDatagramServerSocket>>(
                net::ADDRESS_FAMILY_IPV4);

        ON_CALL(*socket, SendToInternal(_, _, _))
            .WillByDefault(Invoke(&failing_socket_factory_,
                                  &MockFailingMdnsSocketFactory::FailToSend));
        ON_CALL(*socket, RecvFrom(_, _, _, _)).WillByDefault(Return(-1));

        sockets->push_back(std::move(socket));
      };
  EXPECT_CALL(failing_socket_factory_, CreateSockets(_))
      .WillOnce(Invoke(create_send_failing_socket));
  Reset(true /* use_failing_socket_factory */);
  const auto& addr = kPublicAddrs[0];
  std::string expected_announcement =
      CreateResolutionResponse(kDefaultTtl, {{"0.local", addr}});
  // Mocked CreateSockets above only creates one socket.
  EXPECT_CALL(failing_socket_factory_, OnSendTo(expected_announcement))
      .Times(kNumAnnouncementsPerInterface + kNumMaxRetriesPerResponse);
  const auto name = CreateNameForAddress(0, addr);
  RunUntilNoTasksRemain();
}

// Test that responses as announcements are sent following the per-response rate
// limit.
TEST_F(MdnsResponderTest, AnnouncementsAreRateLimitedPerResponse) {
  const auto& addr1 = kPublicAddrs[0];
  const auto& addr2 = kPublicAddrs[1];
  std::string expected_announcement1 =
      CreateResolutionResponse(kDefaultTtl, {{"0.local", addr1}});
  std::string expected_announcement2 =
      CreateResolutionResponse(kDefaultTtl, {{"1.local", addr2}});
  // First announcement for 0.local.
  // MockMdnsSocketFactory binds sockets to two interfaces.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement1)).Times(2);
  // We need the async call from the client.
  client_[0]->CreateNameForAddress(addr1, base::DoNothing());
  client_[0]->CreateNameForAddress(addr2, base::DoNothing());

  RunFor(base::Milliseconds(900));
  // Second announcement for 0.local.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement1)).Times(2);
  RunFor(base::Seconds(1));
  // First announcement for 1.local.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement2)).Times(2);
  RunFor(base::Seconds(1));
  // Second announcement for 1.local.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement2)).Times(2);
  RunUntilNoTasksRemain();
}

// Test that responses as goodbyes are sent following the per-response rate
// limit.
TEST_F(MdnsResponderTest, GoodbyesAreRateLimitedPerResponse) {
  const auto& addr1 = kPublicAddrs[0];
  const auto& addr2 = kPublicAddrs[1];
  // Note that the wrapper method calls below are sync and announcements are
  // sent after they return.
  auto name1 = CreateNameForAddress(0, addr1);
  auto name2 = CreateNameForAddress(0, addr2);
  std::string expected_goodbye1 =
      CreateResolutionResponse(base::TimeDelta(), {{name1, addr1}});
  std::string expected_goodbye2 =
      CreateResolutionResponse(base::TimeDelta(), {{name2, addr2}});

  // Goodbye for 0.local.
  // MockMdnsSocketFactory binds sockets to two interfaces.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_goodbye1)).Times(2);
  // Note that the wrapper method calls below are async.
  RemoveNameForAddressAndExpectDone(0, addr1);
  RemoveNameForAddressAndExpectDone(0, addr2);

  RunFor(base::Milliseconds(900));
  // Goodbye for 1.local.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_goodbye2)).Times(2);
  RunUntilNoTasksRemain();
}

// Test that the mixture of announcements and goodbyes are sent following the
// per-response rate limit.
TEST_F(MdnsResponderTest, AnnouncementsAndGoodbyesAreRateLimitedPerResponse) {
  const auto& addr1 = kPublicAddrs[0];
  const auto& addr2 = kPublicAddrs[1];
  std::string expected_announcement1 =
      CreateResolutionResponse(kDefaultTtl, {{"0.local", addr1}});
  std::string expected_announcement2 =
      CreateResolutionResponse(kDefaultTtl, {{"1.local", addr2}});
  std::string expected_goodbye1 =
      CreateResolutionResponse(base::TimeDelta(), {{"0.local", addr1}});
  std::string expected_goodbye2 =
      CreateResolutionResponse(base::TimeDelta(), {{"1.local", addr2}});

  // First announcement for 0.local.
  // MockMdnsSocketFactory binds sockets to two interfaces.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement1)).Times(2);
  // We need the async call from the client.
  client_[0]->CreateNameForAddress(addr1, base::DoNothing());
  RemoveNameForAddressAndExpectDone(0, addr1);

  client_[0]->CreateNameForAddress(addr2, base::DoNothing());
  RemoveNameForAddressAndExpectDone(0, addr2);

  RunFor(base::Milliseconds(900));
  // Second announcement for 0.local.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement1)).Times(2);
  RunFor(base::Seconds(1));
  // Goodbye for 0.local.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_goodbye1)).Times(2);
  RunFor(base::Seconds(1));
  // First announcement for 1.local.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement2)).Times(2);
  RunFor(base::Seconds(1));
  // Second announcement for 1.local.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_announcement2)).Times(2);
  RunFor(base::Seconds(1));
  // Goodbye for 1.local.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_goodbye2)).Times(2);
  RunUntilNoTasksRemain();
}

// Test that responses to the name generator service queries are sent following
// the per-record rate limit, so that each message is separated by at least one
// second.
TEST_F(MdnsResponderTest,
       MdnsNameGeneratorServiceResponsesAreRateLimitedPerRecord) {
  const auto& addr = kPublicAddrs[0];
  const std::string name = CreateNameForAddress(0, addr);
  // After a name has been created for |addr|, let the responder receive
  // queries. Their responses should be scheduled sequentially, each separated
  // by at least one second. Note that responses to the mDNS name generator
  // service queries have an extra delay of 20-120ms as the service instance
  // name is shared among Chrome instances.
  const std::string query = CreateMdnsQuery(
      0, kMdnsNameGeneratorServiceInstanceName, net::dns_protocol::kTypeTXT);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());

  const std::string expected_response =
      CreateResponseToMdnsNameGeneratorServiceQuery(kDefaultTtl, {"0.local"});
  // Response to the first query is sent right after the query is received.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response)).Times(1);

  // Response to the second received query will be delayed for another one
  // second plus an extra delay of 20-120ms.
  RunFor(base::Milliseconds(1015));
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response)).Times(1);

  RunUntilNoTasksRemain();
}

// Test that responses with resource records for name resolution are sent based
// on a per-record rate limit.
TEST_F(MdnsResponderTest, ResolutionResponsesAreRateLimitedPerRecord) {
  const auto& addr1 = kPublicAddrs[0];
  const auto& addr2 = kPublicAddrs[1];
  auto name1 = CreateNameForAddress(0, addr1);
  auto name2 = CreateNameForAddress(0, addr2);
  // kPublicAddrs are IPv4.
  std::string query1 = CreateMdnsQuery(0, {name1}, net::dns_protocol::kTypeA);
  std::string query2 = CreateMdnsQuery(0, {name2}, net::dns_protocol::kTypeA);
  std::string expected_response1 =
      CreateResolutionResponse(kDefaultTtl, {{name1, addr1}});
  std::string expected_response2 =
      CreateResolutionResponse(kDefaultTtl, {{name2, addr2}});

  // Resolution for name1.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response1)).Times(1);
  // Resolution for name2.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response2)).Times(1);
  // SimulateReceive only lets the last created socket receive.
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query1.data()), query1.size());
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query2.data()), query2.size());
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query1.data()), query1.size());
  RunFor(base::Milliseconds(900));
  // Resolution for name1 for the second query about it.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response1)).Times(1);
  RunUntilNoTasksRemain();
}

// Test that negative responses to queries for non-existing records are sent
// based on a per-record rate limit.
TEST_F(MdnsResponderTest, NegativeResponsesAreRateLimitedPerRecord) {
  const auto& addr1 = kPublicAddrs[0];
  const auto& addr2 = kPublicAddrs[1];
  auto name1 = CreateNameForAddress(0, addr1);
  auto name2 = CreateNameForAddress(0, addr2);
  // kPublicAddrs are IPv4 and type AAAA records do not exist.
  std::string query1 =
      CreateMdnsQuery(0, {name1}, net::dns_protocol::kTypeAAAA);
  std::string query2 =
      CreateMdnsQuery(0, {name2}, net::dns_protocol::kTypeAAAA);
  std::string expected_response1 = CreateNegativeResponse({{name1, addr1}});
  std::string expected_response2 = CreateNegativeResponse({{name2, addr2}});

  // Negative response for name1.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response1)).Times(1);
  // Negative response for name2.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response2)).Times(1);
  // SimulateReceive only lets the last created socket receive.
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query1.data()), query1.size());
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query2.data()), query2.size());
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query1.data()), query1.size());
  RunFor(base::Milliseconds(900));
  // Negative response for name1 for the second query about it.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_response1)).Times(1);
  RunUntilNoTasksRemain();
}

// Test that the mixture of resolution and negative responses are sent following
// the per-record rate limit.
TEST_F(MdnsResponderTest,
       ResolutionAndNegativeResponsesAreRateLimitedPerRecord) {
  const auto& addr = kPublicAddrs[0];
  auto name = CreateNameForAddress(0, addr);
  // kPublicAddrs are IPv4 and type AAAA records do not exist.
  std::string query_a = CreateMdnsQuery(0, {name}, net::dns_protocol::kTypeA);
  std::string query_aaaa =
      CreateMdnsQuery(0, {name}, net::dns_protocol::kTypeAAAA);
  std::string expected_resolution =
      CreateResolutionResponse(kDefaultTtl, {{name, addr}});
  std::string expected_negative_resp = CreateNegativeResponse({{name, addr}});

  EXPECT_CALL(socket_factory_, OnSendTo(expected_resolution)).Times(1);
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query_a.data()), query_a.size());
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query_aaaa.data()), query_aaaa.size());
  RunFor(base::Milliseconds(900));

  EXPECT_CALL(socket_factory_, OnSendTo(expected_negative_resp)).Times(1);
  RunUntilNoTasksRemain();
}

// Test that we send responses immediately to probing queries with qtype ANY.
TEST_F(MdnsResponderTest, ResponsesToProbesAreNotRateLimited) {
  const auto& addr = kPublicAddrs[0];
  auto name = CreateNameForAddress(0, addr);
  // Type ANY for probing queries.
  //
  // TODO(qingsi): Setting the type to kTypeAny is not sufficient to construct a
  // proper probe query. We also need to include a record in the Authority
  // section. See the comment inside IsProbeQuery in mdns_responder.cc. After we
  // support parsing the Authority section of a query in DnsQuery, we should
  // create the following probe query by the standard definition.
  std::string query = CreateMdnsQuery(0, {name}, net::dns_protocol::kTypeANY);
  std::string expected_response =
      CreateResolutionResponse(kDefaultTtl, {{name, addr}});

  EXPECT_CALL(socket_factory_, OnSendTo(expected_response)).Times(2);
  // SimulateReceive only lets the last created socket receive.
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  socket_factory_.SimulateReceive(
      reinterpret_cast<const uint8_t*>(query.data()), query.size());
  RunFor(base::Milliseconds(500));
}

// Test that different rate limit schemes effectively form different queues of
// responses that do not interfere with each other.
TEST_F(MdnsResponderTest, RateLimitSchemesDoNotInterfere) {
  const auto& addr1 = kPublicAddrsIpv6[0];
  const auto& addr2 = kPublicAddrsIpv6[1];
  // SimpleNameGenerator.
  std::string name1 = "0.local";
  std::string name2 = "1.local";
  std::string query1_a = CreateMdnsQuery(0, {name1}, net::dns_protocol::kTypeA);
  std::string query1_aaaa =
      CreateMdnsQuery(0, {name1}, net::dns_protocol::kTypeAAAA);
  std::string query1_any =
      CreateMdnsQuery(0, {name1}, net::dns_protocol::kTypeANY);
  std::string query2_a = CreateMdnsQuery(0, {name2}, net::dns_protocol::kTypeA);
  std::string query2_aaaa =
      CreateMdnsQuery(0, {name2}, net::dns_protocol::kTypeAAAA);
  std::string query2_any =
      CreateMdnsQuery(0, {name2}, net::dns_protocol::kTypeANY);
  std::string expected_resolution1 =
      CreateResolutionResponse(kDefaultTtl, {{name1, addr1}});
  std::string expected_resolution2 =
      CreateResolutionResponse(kDefaultTtl, {{name2, addr2}});
  std::string expected_negative_resp1 =
      CreateNegativeResponse({{name1, addr1}});
  std::string expected_negative_resp2 =
      CreateNegativeResponse({{name2, addr2}});

  auto do_sequence_after_name_created =
      [](net::MockMDnsSocketFactory* socket_factory, const std::string& query_a,
         const std::string& query_aaaa, const std::string& query_any,
         const std::string& /* name */, bool /* announcement_scheduled */) {
        socket_factory->SimulateReceive(
            reinterpret_cast<const uint8_t*>(query_a.data()), query_a.size());
        socket_factory->SimulateReceive(
            reinterpret_cast<const uint8_t*>(query_aaaa.data()),
            query_aaaa.size());
        socket_factory->SimulateReceive(
            reinterpret_cast<const uint8_t*>(query_any.data()),
            query_any.size());
      };
  // 2 first announcements for name1 from 2 interfaces (per-response limit) and
  // 1 response to the probing query1_any (no limit).
  EXPECT_CALL(socket_factory_, OnSendTo(expected_resolution1)).Times(3);
  // 1 negative response to query1_a (per-record limit).
  EXPECT_CALL(socket_factory_, OnSendTo(expected_negative_resp1)).Times(1);
  // 1 response to the probing query2_any (no limit).
  EXPECT_CALL(socket_factory_, OnSendTo(expected_resolution2)).Times(1);
  // 1 negative response to query2_a (per-record limit).
  EXPECT_CALL(socket_factory_, OnSendTo(expected_negative_resp2)).Times(1);
  client_[0]->CreateNameForAddress(
      addr1, base::BindOnce(do_sequence_after_name_created, &socket_factory_,
                            query1_a, query1_aaaa, query1_any));
  client_[0]->CreateNameForAddress(
      addr2, base::BindOnce(do_sequence_after_name_created, &socket_factory_,
                            query2_a, query2_aaaa, query2_any));
  RunFor(base::Milliseconds(900));

  // 2 second announcements for name1 from 2 interfaces, and 1 response to
  // query1_aaaa (per-record limit).
  EXPECT_CALL(socket_factory_, OnSendTo(expected_resolution1)).Times(3);
  // 1 response to query2_aaaa (per-record limit).
  EXPECT_CALL(socket_factory_, OnSendTo(expected_resolution2)).Times(1);
  RunFor(base::Seconds(1));

  // 2 first announcements for name2 from 2 interfaces.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_resolution2)).Times(2);
  RunFor(base::Seconds(1));

  // 2 second announcements for name2 from 2 interfaces.
  EXPECT_CALL(socket_factory_, OnSendTo(expected_resolution2)).Times(2);
  RunUntilNoTasksRemain();
}

// Test that the responder does not send an announcement if the current
// scheduled delay exceeds the maximum delay allowed, and the client side gets
// notified of the result.
TEST_F(MdnsResponderTest, LongDelayedAnnouncementIsNotScheduled) {
  const auto& addr = kPublicAddrs[0];
  // Enqueue announcements and delays so that we reach the maximum delay
  // allowed of the per-response rate limit. Our current implementation defines
  // a 10-second maximum scheduled delay (see kMaxScheduledDelay in
  // mdns_responder.cc).
  for (int i = 0; i < 5; ++i) {
    // Use the async call from the client.
    client_[0]->CreateNameForAddress(addr, base::DoNothing());
    client_[0]->RemoveNameForAddress(addr, base::DoNothing());
  }
  client_[0]->CreateNameForAddress(
      addr, base::BindOnce([](const std::string&, bool announcement_scheduled) {
        EXPECT_FALSE(announcement_scheduled);
      }));
  RunUntilNoTasksRemain();
}

// Test that all pending sends scheduled are cancelled if the responder manager
// is destroyed.
TEST_F(MdnsResponderTest, ScheduledSendsAreCancelledAfterManagerDestroyed) {
  const auto& addr = kPublicAddrs[0];
  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(0);
  // Use the async call from the client.
  client_[0]->CreateNameForAddress(addr, base::DoNothing());
  client_[0]->RemoveNameForAddress(addr, base::DoNothing());
  host_manager_.reset();
  RunUntilNoTasksRemain();
}

// Test that if all socket handlers fail to read, the manager restarts itself.
TEST_F(MdnsResponderTest, ManagerCanRestartAfterAllSocketHandlersFailToRead) {
  base::HistogramTester tester;
  auto create_read_failing_socket =
      [this](std::vector<std::unique_ptr<net::DatagramServerSocket>>* sockets) {
        auto socket =
            std::make_unique<NiceMock<net::MockMDnsDatagramServerSocket>>(
                net::ADDRESS_FAMILY_IPV4);

        ON_CALL(*socket, SendToInternal(_, _, _)).WillByDefault(Return(0));
        ON_CALL(*socket, RecvFrom(_, _, _, _))
            .WillByDefault(Invoke(&failing_socket_factory_,
                                  &MockFailingMdnsSocketFactory::FailToRecv));

        sockets->push_back(std::move(socket));
      };
  EXPECT_CALL(failing_socket_factory_, CreateSockets(_))
      .WillOnce(Invoke(create_read_failing_socket));
  Reset(true /* use_failing_socket_factory */);
  // Called when the manager restarts. The mocked CreateSockets() by default
  // returns an empty vector of sockets, thus failing the restart again.
  EXPECT_CALL(failing_socket_factory_, CreateSockets(_)).Times(1);
  RunUntilNoTasksRemain();
}

// Test that sending packets on an interface can be blocked by an incomplete
// send on the same interface. Blocked packets are later flushed when sending is
// unblocked.
TEST_F(MdnsResponderTest, IncompleteSendBlocksFollowingSends) {
  auto create_send_blocking_socket =
      [this](std::vector<std::unique_ptr<net::DatagramServerSocket>>* sockets) {
        auto socket =
            std::make_unique<NiceMock<net::MockMDnsDatagramServerSocket>>(
                net::ADDRESS_FAMILY_IPV4);

        ON_CALL(*socket, SendToInternal(_, _, _))
            .WillByDefault(
                Invoke(&failing_socket_factory_,
                       &MockFailingMdnsSocketFactory::MaybeBlockSend));
        ON_CALL(*socket, RecvFrom(_, _, _, _)).WillByDefault(Return(-1));

        sockets->push_back(std::move(socket));
      };
  EXPECT_CALL(failing_socket_factory_, CreateSockets(_))
      .WillOnce(Invoke(create_send_blocking_socket));
  Reset(true /* use_failing_socket_factory */);

  const auto& addr1 = kPublicAddrs[0];
  std::string expected_announcement1 =
      CreateResolutionResponse(kDefaultTtl, {{"0.local", addr1}});
  // Mocked CreateSockets above only creates one socket.
  // We schedule to send the announcement for |kNumAnnouncementsPerInterface|
  // times but the second announcement is blocked by the first one in this case.
  EXPECT_CALL(failing_socket_factory_, OnSendTo(expected_announcement1))
      .Times(1);
  failing_socket_factory_.BlockSend();
  const auto name1 = CreateNameForAddress(0, addr1);
  RunUntilNoTasksRemain();

  const auto& addr2 = kPublicAddrs[1];
  std::string expected_announcement2 =
      CreateResolutionResponse(kDefaultTtl, {{"1.local", addr2}});
  // The announcement for the following name should also be blocked.
  const auto name2 = CreateNameForAddress(0, addr2);
  EXPECT_CALL(failing_socket_factory_, OnSendTo(expected_announcement2))
      .Times(0);
  RunUntilNoTasksRemain();

  // We later unblock sending packets. Previously scheduled announcements should
  // be flushed.
  EXPECT_CALL(failing_socket_factory_, OnSendTo(expected_announcement1))
      .Times(kNumAnnouncementsPerInterface - 1);
  EXPECT_CALL(failing_socket_factory_, OnSendTo(expected_announcement2))
      .Times(kNumAnnouncementsPerInterface);
  failing_socket_factory_.ResumeSend();
  RunUntilNoTasksRemain();
}

}  // namespace network
