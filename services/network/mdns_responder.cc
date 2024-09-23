// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/mdns_responder.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <queue>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "net/base/address_family.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_util.h"
#include "net/dns/mdns_client.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/util.h"
#include "net/dns/record_parsed.h"
#include "net/dns/record_rdata.h"
#include "net/socket/datagram_server_socket.h"
#include "net/socket/udp_server_socket.h"
#include "services/network/public/cpp/features.h"

// TODO(qingsi): Several features to implement:
//
// 1) Support parsing a query with multiple questions in the wire format to a
// DnsQuery, and bundle answers to questions in a single DnsResponse with proper
// rate limiting.
//
// 2) Support detecting queries for the same record within the minimal interval
// between responses and allow at most one response queued by the scheduler at a
// time for each name.
//
// 3) Support parsing the authority section of a query in the wire format to
// correctly implement the detection of probe queries.
namespace network {

namespace {

using MdnsResponderServiceError = MdnsResponderManager::ServiceError;

// RFC 6762, Section 6.
//
// The multicast of responses of the same record on an interface must be at
// least one second apart on that particular interface.
const base::TimeDelta kMinIntervalBetweenSameRecord = base::Seconds(1);

const base::TimeDelta kMinIntervalBetweenMdnsResponses = base::Seconds(1);

// RFC 6762, Section 10.
const base::TimeDelta kDefaultTtlForRecordWithHostname = base::Seconds(120);

// RFC 6762, Section 8.3.
const int kMinNumAnnouncementsToSend = 2;

// Maximum number of retries for the same response due to send failure.
const uint8_t kMaxMdnsResponseRetries = 2;
// The capacity of the send queue for packets blocked by an incomplete send.
const uint8_t kSendQueueCapacity = 100;
// Maximum delay allowed for per-response rate-limited responses.
const base::TimeDelta kMaxScheduledDelay = base::Seconds(10);

// The query name of the mDNS name generator service.
const char kMdnsNameGeneratorServiceInstanceName[] =
    "Generated-Names._mdns_name_generator._udp.local";

// RFC 6763, the TXT record is recommended to be under 1300 bytes to fit in a
// single 1500-byte Ethernet packet.
//
// Currently we only construct a TXT record in the response to an mDNS name
// generator service query. The record consists of a list of owned names, and
// this list is truncated as necessary to stay within the size limit. See
// |CreateTxtRdataWithNames| below for the detail.
const uint16_t kMaxTxtRecordSizeInBytes = 1300;
// RFC 6763, Section 6.4, the key in a kv pair in a DNS-SD TXT record should be
// no more than 9 characters long.
const int kMaxKeySizeInTxtRecord = 9;
// The prefix of the key used in the TXT record to list mDNS names.
const char kKeyPrefixInTxtRecord[] = "name";
// Version tag in the TXT record.
const char kTxtversLine[] = "\x9txtvers=1";

// RFC 6762, Section 6, a response that may contain an answer as a member of a
// shared resource record set, should be delayed uniformly and randomly in the
// range of 20-120 ms. This delay is applied in addition to the scheduled delay
// by rate limiting.
constexpr auto kMinRandDelayForSharedResult = base::Milliseconds(20);
constexpr auto kMaxRandDelayForSharedResult = base::Milliseconds(120);

class RandomUuidNameGenerator
    : public network::MdnsResponderManager::NameGenerator {
 public:
  std::string CreateName() override {
    return base::Uuid::GenerateRandomV4().AsLowercaseString();
  }
};

bool QueryTypeAndAddressFamilyAreCompatible(uint16_t qtype,
                                            net::AddressFamily af) {
  switch (qtype) {
    case net::dns_protocol::kTypeA:
      return af == net::ADDRESS_FAMILY_IPV4;
    case net::dns_protocol::kTypeAAAA:
      return af == net::ADDRESS_FAMILY_IPV6;
    case net::dns_protocol::kTypeANY:
      return af == net::ADDRESS_FAMILY_IPV4 || af == net::ADDRESS_FAMILY_IPV6;
    default:
      return false;
  }
}

// Creates a vector of A or AAAA records, where the name field of each record is
// given by the name in |name_addr_map|, and its mapped address is used to
// construct the RDATA stored in |DnsResourceRecord::owned_rdata|. |ttl|
// specifies the TTL of each record. With the owned RDATA, the returned records
// can be later used to construct a DnsResponse.
std::vector<net::DnsResourceRecord> CreateAddressResourceRecords(
    const std::map<std::string, net::IPAddress>& name_addr_map,
    const base::TimeDelta& ttl) {
  std::vector<net::DnsResourceRecord> address_records;
  for (const auto& name_addr_pair : name_addr_map) {
    const auto& ip = name_addr_pair.second;
    DCHECK(ip.IsIPv4() || ip.IsIPv6());
    net::DnsResourceRecord record;
    record.name = name_addr_pair.first;
    record.type = (ip.IsIPv4() ? net::dns_protocol::kTypeA
                               : net::dns_protocol::kTypeAAAA);
    // Set the cache-flush bit to assert that this information is the truth and
    // the whole truth.
    record.klass =
        net::dns_protocol::kClassIN | net::dns_protocol::kFlagCacheFlush;
    // TTL in a resource record is 32-bit.
    record.ttl = base::checked_cast<uint32_t>(ttl.InSeconds());
    record.SetOwnedRdata(net::IPAddressToPackedString(ip));
    address_records.push_back(std::move(record));
  }
  return address_records;
}

// Creates an NSEC record RDATA in the wire format for the resource record type
// that corresponds to the address family of |addr|. The type bit map in the
// RDATA asserts the existence of only the address record that matches |addr|.
// Per RFC 3845 Section 2.1 and RFC 6762 Section 6, each RDATA has its Next
// Domain Name as a two-octet pointer to the name field of the NSEC resource
// record. |containing_nsec_rr_offset| defines the offset in the message of the
// NSEC resource record that would contain the returned RDATA, and its value is
// used to generate the correct pointer for Next Domain Name.
std::string CreateNsecRdata(const net::IPAddress& addr,
                            uint16_t containing_nsec_rr_offset) {
  DCHECK(addr.IsIPv4() || addr.IsIPv6());
  // Each NSEC rdata in our negative response is given by 5 octets and 8
  // octets for type A and type AAAA records, respectively:
  //
  // 2 octets for Next Domain Name as a pointer to the name field
  // (DnsResourceRecord::name) of the NSEC record that will contain this RDATA;
  // 1 octet for Window Block, which is always 0;
  // 1 octet for Bitmap Length with value X, where X=1 for type A and X=4 for
  // type AAAA;
  // X octet(s) for Bitmap, 0x40 for type A and 0x00000008 for type AAAA.
  std::string next_domain_name =
      net::CreateNamePointer(containing_nsec_rr_offset);
  DCHECK_EQ(2u, next_domain_name.size());
  if (addr.IsIPv4())
    return next_domain_name + std::string("\x00\x01\x40", 3);

  return next_domain_name + std::string("\x00\x04\x00\x00\x00\x08", 6);
}

// Creates a vector of NSEC records, where the name field of each record is
// given by the name in |name_addr_map|, and its mapped address is used to
// construct the RDATA stored in |DnsResourceRecord::owned_rdata| via
// CreateNsecRdata above. With the owned RDATA, the returned records can be
// later used to construct a DnsResponse.
std::vector<net::DnsResourceRecord> CreateNsecResourceRecords(
    const std::map<std::string, net::IPAddress>& name_addr_map,
    uint16_t first_nsec_rr_offset) {
  std::vector<net::DnsResourceRecord> nsec_records;
  uint16_t cur_rr_offset = first_nsec_rr_offset;
  for (const auto& name_addr_pair : name_addr_map) {
    net::DnsResourceRecord record;
    record.name = name_addr_pair.first;
    record.type = net::dns_protocol::kTypeNSEC;
    // Set the cache-flush bit to assert that this information is the truth and
    // the whole truth.
    record.klass =
        net::dns_protocol::kClassIN | net::dns_protocol::kFlagCacheFlush;
    // RFC 6762, Section 6.1. TTL should be the same as that of what the record
    // would have.
    record.ttl = kDefaultTtlForRecordWithHostname.InSeconds();
    record.SetOwnedRdata(CreateNsecRdata(name_addr_pair.second, cur_rr_offset));
    cur_rr_offset += record.CalculateRecordSize();
    nsec_records.push_back(std::move(record));
  }
  return nsec_records;
}

// Creates TXT RDATA as a list of key-value pairs subject to a size limit. The
// key is in the format "name0", "name1" and so on, and the value is the name.
std::string CreateTxtRdataWithNames(const std::set<std::string>& names,
                                    uint16_t txt_rdata_size_limit) {
  DCHECK(!names.empty());
  DCHECK_GT(txt_rdata_size_limit, sizeof(kTxtversLine));
  int remaining_budget =
      txt_rdata_size_limit - sizeof(kTxtversLine) + 1 /* null terminator */;
  std::string txt_rdata;
  size_t prev_txt_rdata_size = 0;
  uint16_t idx = 0;
  for (const std::string& name : names) {
    const int key_size =
        sizeof(kKeyPrefixInTxtRecord) - 1 /* null terminator */ +
        (idx > 0 ? static_cast<int>(log10(static_cast<double>(idx))) + 1 : 1);
    // RFC 6763, Section 6.4, the key should be no more than nine characters
    // long.
    DCHECK_LE(key_size, kMaxKeySizeInTxtRecord);
    // Each TXT line consists of a length octet followed by as many characters,
    // and as a result each line cannot exceed 256 characters.
    const int line_size =
        2 /* length octet and "=" sign */ + key_size + name.size();
    // Each name should be guaranteed to have no more than 245 characters to
    // meet the line length limit. See the comment before |NameGenerator|.
    DCHECK_LE(line_size - 1, std::numeric_limits<uint8_t>::max());
    remaining_budget -= line_size;
    if (remaining_budget <= 0) {
      VLOG(1) << "TXT RDATA size limit exceeded. Stopped appending lines in "
                 "the response.";
      break;
    }

    // Note that c_str() is null terminated.
    //
    // E.g. \x13name0=example.local
    base::StringAppendF(&txt_rdata, "%c%s%d=%s", line_size - 1,
                        kKeyPrefixInTxtRecord, idx, name.c_str());
    DCHECK_EQ(txt_rdata.size(), prev_txt_rdata_size + line_size);
    prev_txt_rdata_size = txt_rdata.size();
    ++idx;
  }

  DCHECK(!txt_rdata.empty());
  // Note that the size of the version tag line has been deducted from the
  // budget before we add lines of names.
  txt_rdata += kTxtversLine;

  return txt_rdata;
}

net::DnsResourceRecord CreateTxtRecordWithNames(
    const base::TimeDelta& ttl,
    const std::string& service_instance_name,
    const std::set<std::string>& names) {
  net::DnsResourceRecord txt;
  txt.name = service_instance_name;
  txt.type = net::dns_protocol::kTypeTXT;
  // The cache-flush bit is not set so that the responses from other Chrome
  // instances are not considered conflicts. See the conflict detection in
  // |SocketHandler::HandlePacket|.
  txt.klass = net::dns_protocol::kClassIN;
  // TTL in a resource record is 32-bit.
  txt.ttl = base::checked_cast<uint32_t>(ttl.InSeconds());
  uint16_t txt_rdata_size_limit =
      kMaxTxtRecordSizeInBytes - service_instance_name.size() -
      net::dns_protocol::kResourceRecordSizeInBytesWithoutNameAndRData;
  txt.SetOwnedRdata(CreateTxtRdataWithNames(names, txt_rdata_size_limit));
  return txt;
}

bool IsProbeQuery(const net::DnsQuery& query) {
  // TODO(qingsi): RFC 6762, the proper way to detect a probe query is
  // to check if
  //
  // 1) its qtype is ANY (Section 8.1) and
  // 2) it "contains a proposed record in the Authority Section that
  // answers the question in the Question Section" (Section 6).
  //
  // Currently DnsQuery does not support the Authority section. Fix it.
  return query.qtype() == net::dns_protocol::kTypeANY;
}

struct PendingPacket {
  PendingPacket(scoped_refptr<net::IOBufferWithSize> buf,
                scoped_refptr<MdnsResponseSendOption> option,
                const base::TimeTicks& send_ready_time)
      : buf(std::move(buf)),
        option(std::move(option)),
        send_ready_time(send_ready_time) {}

  bool operator<(const PendingPacket& other) const {
    return send_ready_time > other.send_ready_time;
  }

  scoped_refptr<net::IOBufferWithSize> buf;
  scoped_refptr<MdnsResponseSendOption> option;
  base::TimeTicks send_ready_time;
};

}  // namespace


namespace mdns_helper {

scoped_refptr<net::IOBufferWithSize> CreateResolutionResponse(
    const base::TimeDelta& ttl,
    const std::map<std::string, net::IPAddress>& name_addr_map) {
  DCHECK(!name_addr_map.empty());
  std::vector<net::DnsResourceRecord> answers =
      CreateAddressResourceRecords(name_addr_map, ttl);
  std::vector<net::DnsResourceRecord> additional_records;
  if (!ttl.is_zero()) {
    uint16_t cur_size = std::accumulate(
        answers.begin(), answers.end(), sizeof(net::dns_protocol::Header),
        [](size_t cur_size, const net::DnsResourceRecord& answer) {
          return cur_size + answer.CalculateRecordSize();
        });
    additional_records = CreateNsecResourceRecords(name_addr_map, cur_size);
  }

  // RFC 6762.
  //
  // Section 6. mDNS responses MUST NOT contain any questions.
  // Section 18.1. In mDNS responses, ID MUST be set to zero.
  net::DnsResponse response(0 /* id */, true /* is_authoritative */, answers,
                            {} /* authority_records */, additional_records,
                            std::nullopt /* query */);
  DCHECK(response.io_buffer() != nullptr);
  auto buf =
      base::MakeRefCounted<net::IOBufferWithSize>(response.io_buffer_size());
  memcpy(buf->data(), response.io_buffer()->data(), response.io_buffer_size());
  return buf;
}

scoped_refptr<net::IOBufferWithSize> CreateNegativeResponse(
    const std::map<std::string, net::IPAddress>& name_addr_map) {
  DCHECK(!name_addr_map.empty());
  std::vector<net::DnsResourceRecord> nsec_records = CreateNsecResourceRecords(
      name_addr_map, sizeof(net::dns_protocol::Header));
  std::vector<net::DnsResourceRecord> additional_records =
      CreateAddressResourceRecords(name_addr_map,
                                   kDefaultTtlForRecordWithHostname);
  net::DnsResponse response(0 /* id */, true /* is_authoritative */,
                            nsec_records, {} /* authority_records */,
                            additional_records, std::nullopt /* query */);
  DCHECK(response.io_buffer() != nullptr);
  auto buf =
      base::MakeRefCounted<net::IOBufferWithSize>(response.io_buffer_size());
  memcpy(buf->data(), response.io_buffer()->data(), response.io_buffer_size());
  return buf;
}

scoped_refptr<net::IOBufferWithSize>
CreateResponseToMdnsNameGeneratorServiceQuery(
    const base::TimeDelta& ttl,
    const std::set<std::string>& mdns_names) {
  std::vector<net::DnsResourceRecord> answers(
      1, CreateTxtRecordWithNames(ttl, kMdnsNameGeneratorServiceInstanceName,
                                  mdns_names));

  net::DnsResponse response(/*id=*/0, /*is_authoritative=*/true, answers,
                            /*authority_records=*/{},
                            /*additional_records=*/{},
                            /*query=*/std::nullopt,
                            /*rcode=*/net::dns_protocol::kRcodeNOERROR,
                            /*validate_records=*/true,
                            /*validate_names_as_internet_hostnames=*/false);
  DCHECK(response.io_buffer() != nullptr);
  auto buf =
      base::MakeRefCounted<net::IOBufferWithSize>(response.io_buffer_size());
  memcpy(buf->data(), response.io_buffer()->data(), response.io_buffer_size());
  return buf;
}

}  // namespace mdns_helper

class MdnsResponderManager::SocketHandler {
 public:
  SocketHandler(uint16_t id,
                std::unique_ptr<net::DatagramServerSocket> socket,
                MdnsResponderManager* responder_manager)
      : id_(id),
        scheduler_(std::make_unique<ResponseScheduler>(this)),
        socket_(std::move(socket)),
        responder_manager_(responder_manager),
        io_buffer_(base::MakeRefCounted<net::IOBufferWithSize>(
            net::dns_protocol::kMaxMulticastSize + 1)) {}

  SocketHandler(const SocketHandler&) = delete;
  SocketHandler& operator=(const SocketHandler&) = delete;

  ~SocketHandler() = default;

  int Start() {
    net::IPEndPoint end_point;
    int rv = socket_->GetLocalAddress(&end_point);
    if (rv != net::OK) {
      return rv;
    }
    DCHECK(end_point.GetFamily() == net::ADDRESS_FAMILY_IPV4 ||
           end_point.GetFamily() == net::ADDRESS_FAMILY_IPV6);
    multicast_addr_ =
        net::dns_util::GetMdnsGroupEndPoint(end_point.GetFamily());
    int result = DoReadLoop();
    if (result == net::ERR_IO_PENDING) {
      // An in-progress read loop is considered a completed start.
      return net::OK;
    }
    return result;
  }

  // Returns true if the send is successfully scheduled after rate limiting on
  // the underlying interface, and false otherwise.
  bool Send(scoped_refptr<net::IOBufferWithSize> buf,
            scoped_refptr<MdnsResponseSendOption> option);

  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  int DoSend(PendingPacket pending_packet);

  uint16_t id() const { return id_; }

  void SetTickClockForTesting(const base::TickClock* tick_clock);

 private:
  class ResponseScheduler;

  // Returns the effective result after handling. In particular, if |result|
  // represents a non-fatal error that is not ERR_IO_PENDING, it will be
  // converted to net::OK and returned.
  int HandlePacket(int result);

  int DoReadLoop() {
    int result;
    do {
      // Using base::Unretained(this) is safe because the CompletionOnceCallback
      // is automatically cancelled when |socket_| is destroyed, and the latter
      // is owned by |this|.
      result = socket_->RecvFrom(
          io_buffer_.get(), io_buffer_->size(), &recv_addr_,
          base::BindOnce(&MdnsResponderManager::SocketHandler::OnRead,
                         base::Unretained(this)));
      // Process synchronous return from RecvFrom.
      result = HandlePacket(result);
    } while (result >= 0);

    // Note that since |HandlePacket| converts a non-fatal error that is not
    // ERR_IO_PENDING to OK, |result| returned is either ERR_IO_PENDING or a
    // fatal error.
    return result;
  }

  // For the methods below, |result| indicates the number of bytes read if
  // positive, or a network stack error code if negative. Zero indicates either
  // net::OK or zero bytes read.
  void OnRead(int result) {
    result = HandlePacket(result);
    DCHECK_NE(result, net::ERR_IO_PENDING);

    if (result >= 0)
      result = DoReadLoop();

    if (result == net::ERR_IO_PENDING)
      return;

    DCHECK(responder_manager_->IsFatalError(result));
    responder_manager_->OnSocketHandlerReadError(id_, result);
  }

  uint16_t id_;
  std::unique_ptr<ResponseScheduler> scheduler_;
  std::unique_ptr<net::DatagramServerSocket> socket_;
  // A back pointer to the responder manager that owns this socket handler. The
  // handler should be destroyed before |responder_manager_| becomes invalid or
  // a weak reference should be used to access the manager when there is no such
  // guarantee in an operation.
  const raw_ptr<MdnsResponderManager> responder_manager_;
  scoped_refptr<net::IOBufferWithSize> io_buffer_;
  net::IPEndPoint recv_addr_;
  net::IPEndPoint multicast_addr_;

  base::WeakPtrFactory<SocketHandler> weak_factory_{this};
};

// Implements the rate limiting schemes for sending responses as defined by
// RateLimitScheme. Specifically:
//
// 1. Announcements for new names (RFC 6762, Section 8.3) and goodbyes (RFC
// 6762, Section 10.1) are rate limited per response on each interface, so that
// the interval between sending the above responses is no less than one second
// on the given interface.
//
// 2. Responses containing resource records for name resolution, and also
// negative responses to queries for non-existing records of generated names,
// are rate limited per record. The delay of such a response from the last
// per-record rate limited response is computed as the maximum delay of all
// records (names) contained. Per RFC 6762, Section 6, records are sent at a
// maximum rate of one per each second.
//
// 3. Responses to probing queries (RFC 6762, Section 8.1) are not rate
// limited.
//
// Also, if the projected delay of a response exceeds the maximum scheduled
// delay given by kMaxScheduledDelay, the response is NOT scheduled.
class MdnsResponderManager::SocketHandler::ResponseScheduler {
 public:
  enum class RateLimitScheme {
    // The next response will be sent at least after
    // kMinIntervalBetweenResponses since the last response that is rate limited
    // by the per-response scheme.
    PER_RESPONSE,
    // The delay of the response from the last one that is rate limited by the
    // per-record scheme, is computed as the maximum delay of all its records
    // (identified by names). The multicast of each record is separated by at
    // least kMinIntervalBetweenSameRecord.
    PER_RECORD,
    // The response is sent immediately.
    NO_LIMIT,
  };

  explicit ResponseScheduler(MdnsResponderManager::SocketHandler* handler)
      : handler_(handler),
        tick_clock_(base::DefaultTickClock::GetInstance()),
        dispatch_timer_(std::make_unique<base::OneShotTimer>(tick_clock_)),
        next_available_time_per_resp_sched_(tick_clock_->NowTicks()) {}

  ResponseScheduler(const ResponseScheduler&) = delete;
  ResponseScheduler& operator=(const ResponseScheduler&) = delete;

  ~ResponseScheduler() { dispatch_timer_->Stop(); }

  // Implements the rate limit scheme on the underlying interface managed by
  // |handler_|. Returns true if the send is scheduled on this interface.
  //
  // Pending sends scheduled are cancelled after |handler_| becomes invalid;
  bool ScheduleNextSend(scoped_refptr<net::IOBufferWithSize> buf,
                        scoped_refptr<MdnsResponseSendOption> option);
  void OnResponseSent(PendingPacket pending_packet, int result) {
    DCHECK(send_pending_);
    send_pending_ = false;
    scoped_refptr<MdnsResponseSendOption>& option = pending_packet.option;
    if (result < 0) {
      VLOG(1) << "Socket send error, socket=" << handler_->id()
              << ", error=" << result;
      if (CanBeRetriedAfterSendFailure(*option)) {
        ++option->num_send_retries_done;
        send_queue_.push(std::move(pending_packet));
      } else {
        VLOG(1) << "Response cannot be sent after " << kMaxMdnsResponseRetries
                << " retries.";
      }
    }
    DispatchPendingPackets();
  }

  // Also resets the scheduler.
  void SetTickClockForTesting(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
    dispatch_timer_ = std::make_unique<base::OneShotTimer>(tick_clock_);
    next_available_time_per_resp_sched_ = tick_clock_->NowTicks();
    next_available_time_for_name_.clear();
  }

  base::WeakPtr<ResponseScheduler> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  RateLimitScheme GetRateLimitSchemeForClass(
      MdnsResponseSendOption::ResponseClass klass) {
    switch (klass) {
      case MdnsResponseSendOption::ResponseClass::ANNOUNCEMENT:
      case MdnsResponseSendOption::ResponseClass::GOODBYE:
        return RateLimitScheme::PER_RESPONSE;
      case MdnsResponseSendOption::ResponseClass::NEGATIVE:
      case MdnsResponseSendOption::ResponseClass::REGULAR_RESOLUTION:
        return RateLimitScheme::PER_RECORD;
      case MdnsResponseSendOption::ResponseClass::PROBE_RESOLUTION:
        return RateLimitScheme::NO_LIMIT;
      case MdnsResponseSendOption::ResponseClass::UNSPECIFIED:
        NOTREACHED_IN_MIGRATION();
        return RateLimitScheme::PER_RESPONSE;
    }
  }
  // Returns null if the computed delay exceeds kMaxScheduledDelay and the next
  // available time is not updated.
  std::optional<base::TimeDelta> ComputeResponseDelayAndUpdateNextAvailableTime(
      RateLimitScheme rate_limit_scheme,
      const MdnsResponseSendOption& option);

  // Dispatches packets in the send queue serially with retries.
  void DispatchPendingPackets();

  // Determines if a response can be retried after send failure.
  bool CanBeRetriedAfterSendFailure(const MdnsResponseSendOption& option) {
    if (option.num_send_retries_done >= kMaxMdnsResponseRetries)
      return false;

    if (option.klass == MdnsResponseSendOption::ResponseClass::ANNOUNCEMENT ||
        option.klass == MdnsResponseSendOption::ResponseClass::GOODBYE ||
        option.klass == MdnsResponseSendOption::ResponseClass::PROBE_RESOLUTION)
      return true;

    return false;
  }

  // A back pointer to the socket handler that owns this scheduler. The
  // scheduler should be destroyed before |handler_| becomes invalid or a weak
  // reference should be used to access the handler when there is no such
  // guarantee in an operation.
  const raw_ptr<MdnsResponderManager::SocketHandler> handler_;
  raw_ptr<const base::TickClock> tick_clock_;
  std::unique_ptr<base::OneShotTimer> dispatch_timer_;
  std::map<std::string, base::TimeTicks> next_available_time_for_name_;
  base::TimeTicks next_available_time_per_resp_sched_;
  bool send_pending_ = false;
  // Packets with earlier ready time have higher priorities.
  std::priority_queue<PendingPacket> send_queue_;

  base::WeakPtrFactory<ResponseScheduler> weak_factory_{this};
};

bool MdnsResponderManager::SocketHandler::Send(
    scoped_refptr<net::IOBufferWithSize> buf,
    scoped_refptr<MdnsResponseSendOption> option) {
  return scheduler_->ScheduleNextSend(std::move(buf), std::move(option));
}

int MdnsResponderManager::SocketHandler::DoSend(PendingPacket pending_packet) {
  auto* buf_data = pending_packet.buf.get();
  size_t buf_size = pending_packet.buf->size();
  return socket_->SendTo(
      buf_data, buf_size, multicast_addr_,
      base::BindOnce(&ResponseScheduler::OnResponseSent,
                     scheduler_->GetWeakPtr(), std::move(pending_packet)));
}

void MdnsResponderManager::SocketHandler::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  scheduler_->SetTickClockForTesting(tick_clock);
}

bool MdnsResponderManager::SocketHandler::ResponseScheduler::ScheduleNextSend(
    scoped_refptr<net::IOBufferWithSize> buf,
    scoped_refptr<MdnsResponseSendOption> option) {
  if (send_queue_.size() >= kSendQueueCapacity) {
    VLOG(1)
        << "mDNS packet discarded after reaching the capacity of send queue.";
    return false;
  }

  auto rate_limit_scheme = GetRateLimitSchemeForClass(option->klass);
  std::optional<base::TimeDelta> delay;
  if (rate_limit_scheme == RateLimitScheme::NO_LIMIT) {
    // Skip the scheduling for this response. Currently the zero delay is only
    // used for negative responses generated by the responder itself. Responses
    // with positive name resolution generated by the responder and also those
    // triggered via the Mojo connection (i.e. announcements and goodbye
    // packets) are rate limited via the scheduled delay below.
    delay = base::TimeDelta();
  } else {
    // TODO(qingsi): The computation of the delay is done statically below at
    // schedule-time. Change it to computing dynamically so that the delay is
    // based on the time of the last send completion.
    delay = ComputeResponseDelayAndUpdateNextAvailableTime(rate_limit_scheme,
                                                           *option);
  }
  if (!delay)
    return false;

  PendingPacket pending_packet(std::move(buf), std::move(option),
                               tick_clock_->NowTicks() + delay.value());
  send_queue_.push(std::move(pending_packet));
  DispatchPendingPackets();
  return true;
}

std::optional<base::TimeDelta> MdnsResponderManager::SocketHandler::
    ResponseScheduler::ComputeResponseDelayAndUpdateNextAvailableTime(
        RateLimitScheme rate_limit_scheme,
        const MdnsResponseSendOption& option) {
  auto now = tick_clock_->NowTicks();
  const auto extra_delay_for_shared_result =
      option.shared_result ? base::RandTimeDelta(kMinRandDelayForSharedResult,
                                                 kMaxRandDelayForSharedResult)
                           : base::TimeDelta();

  // RFC 6762 requires the rate limiting applied on a per-record basis. When a
  // response contains multiple records, each identified by the name, we compute
  // the delay as the maximum delay of records contained. See the definition of
  // RateLimitScheme::PER_RECORD.
  //
  // For responses that are triggered via the Mojo connection, we perform more
  // restrictive rate limiting on a per-response basis. See the
  // definition of RateLimitScheme::PER_RESPONSE.
  if (rate_limit_scheme == RateLimitScheme::PER_RESPONSE) {
    auto delay =
        std::max(next_available_time_per_resp_sched_ - now, base::TimeDelta()) +
        extra_delay_for_shared_result;

    if (delay > kMaxScheduledDelay)
      return std::nullopt;

    next_available_time_per_resp_sched_ =
        now + delay + kMinIntervalBetweenMdnsResponses;
    return delay;
  }

  DCHECK(rate_limit_scheme == RateLimitScheme::PER_RECORD);
  DCHECK(!option.names_for_rate_limit.empty());
  auto next_available_time_for_response = now;
  // TODO(qingsi): There are a couple of issues with computing the delay of a
  // response as the maximum of each name contained and updating the next
  // available time for each name accordingly.
  //
  // 1) It can unnecessarily delay the records with the names that are not
  // backlogged in the schedule.
  //
  // 2) The update of the next available time following 1) further delays the
  // future responses for these victim names, which could escalate the
  // congestion until we start to drop the response after exceeding
  // kMaxScheduledDelay.
  //
  // The root cause is we currently maintain a one-to-one mapping between
  // queries and responses, such that a response answers the questions in the
  // corresponding query entirely (note however that DnsQuery currently supports
  // only a single question). We could mitigate this issue by splitting or
  // merging responses. See the comment block at the beginning of this file
  // about features to implement.
  for (const auto& name : option.names_for_rate_limit) {
    // The following computation assumes that we always send the address record
    // and the negative record at the same time (as we do) for any given name.
    next_available_time_for_response = std::max(
        next_available_time_for_response, next_available_time_for_name_[name]);
  }
  base::TimeDelta delay =
      std::max(next_available_time_for_response - now, base::TimeDelta()) +
      extra_delay_for_shared_result;

  if (delay > kMaxScheduledDelay)
    return std::nullopt;

  for (const auto& name : option.names_for_rate_limit) {
    next_available_time_for_name_[name] =
        next_available_time_for_response + kMinIntervalBetweenSameRecord;
  }
  return delay;
}

void MdnsResponderManager::SocketHandler::ResponseScheduler::
    DispatchPendingPackets() {
  while (!send_pending_ && !send_queue_.empty()) {
    const auto now = tick_clock_->NowTicks();
    const auto next_send_ready_time = send_queue_.top().send_ready_time;
    if (now >= next_send_ready_time) {
      auto pending_packet = std::move(send_queue_.top());
      send_queue_.pop();
      const auto& option = pending_packet.option;
      if (option->cancelled_callback && option->cancelled_callback->Run())
        continue;

      int rv = handler_->DoSend(std::move(pending_packet));
      if (rv == net::ERR_IO_PENDING) {
        send_pending_ = true;
      } else if (rv < net::OK) {
        VLOG(1) << "mDNS packet discarded due to socket send error, socket="
                << handler_->id() << ", error=" << rv;
      }
    } else {
      // We have no packet due; post a task to flush the send queue later.
      //
      // Note that the owning handler of this scheduler may be removed if it
      // encounters read error as we process in |OnSocketHandlerReadError|. We
      // should guarantee any posted task can be cancelled if the scheduler goes
      // away, which we do via the weak pointer.
      const base::TimeDelta time_to_next_packet = next_send_ready_time - now;
      dispatch_timer_->Start(
          FROM_HERE, time_to_next_packet,
          base::BindOnce(&MdnsResponderManager::SocketHandler::
                             ResponseScheduler::DispatchPendingPackets,
                         GetWeakPtr()));
      return;
    }
  }
}

MdnsResponseSendOption::MdnsResponseSendOption() = default;
MdnsResponseSendOption::~MdnsResponseSendOption() = default;

// static
constexpr base::TimeDelta MdnsResponderManager::kManagerStartThrottleDelay;

MdnsResponderManager::MdnsResponderManager() : MdnsResponderManager(nullptr) {}

MdnsResponderManager::MdnsResponderManager(
    net::MDnsSocketFactory* socket_factory)
    : socket_factory_(socket_factory),
      name_generator_(std::make_unique<RandomUuidNameGenerator>()) {
  if (!socket_factory_) {
    owned_socket_factory_ = net::MDnsSocketFactory::CreateDefault();
    socket_factory_ = owned_socket_factory_.get();
  }
  StartIfNeeded();
}

MdnsResponderManager::~MdnsResponderManager() {
  // Note that sending the goodbye is best-effort since it may have a non-zero
  // delay because of backlogged responses from rate-limiting. Delayed send will
  // be cancelled after the manager is destroyed.
  SendGoodbyePacketForMdnsNameGeneratorServiceIfNecessary();
  // When destroyed, each responder will send out Goodbye messages for owned
  // names via the back pointer to the manager. As a result, we should destroy
  // the remaining responders before the manager is destroyed.
  responders_.clear();
}

void MdnsResponderManager::StartIfNeeded() {
  if (start_result_ == SocketHandlerStartResult::ALL_SUCCESS ||
      start_result_ == SocketHandlerStartResult::PARTIAL_SUCCESS) {
    // Start not needed.
    return;
  }

  if (!throttled_start_end_.is_null() &&
      tick_clock_->NowTicks() < throttled_start_end_) {
    // Attempts are throttled. Noop for now.
    DCHECK(start_result_ == SocketHandlerStartResult::ALL_FAILURE);
    return;
  }
  throttled_start_end_ = base::TimeTicks();

  VLOG(1) << "Starting mDNS responder manager.";
  DCHECK(start_result_ == SocketHandlerStartResult::UNSPECIFIED ||
         start_result_ == SocketHandlerStartResult::ALL_FAILURE);
  DCHECK(socket_handler_by_id_.empty());
  std::vector<std::unique_ptr<net::DatagramServerSocket>> sockets;
  // Create and return only bound sockets.
  socket_factory_->CreateSockets(&sockets);

  uint16_t next_available_id = 1;
  for (std::unique_ptr<net::DatagramServerSocket>& socket : sockets) {
    socket_handler_by_id_.emplace(
        next_available_id,
        std::make_unique<MdnsResponderManager::SocketHandler>(
            next_available_id, std::move(socket), this));
    ++next_available_id;
  }

  for (auto it = socket_handler_by_id_.begin();
       it != socket_handler_by_id_.end();) {
    // Start to process untrusted input.
    int rv = it->second->Start();
    if (rv == net::OK) {
      ++it;
    } else {
      VLOG(1) << "Start failed, socket=" << it->second->id()
              << ", error=" << rv;
      it = socket_handler_by_id_.erase(it);
    }
  }
  size_t num_started_socket_handlers = socket_handler_by_id_.size();
  if (socket_handler_by_id_.empty()) {
    start_result_ = SocketHandlerStartResult::ALL_FAILURE;
    throttled_start_end_ = tick_clock_->NowTicks() + kManagerStartThrottleDelay;
    LOG(ERROR) << "mDNS responder manager failed to start.";
    return;
  }

  if (num_started_socket_handlers == next_available_id) {
    start_result_ = SocketHandlerStartResult::ALL_SUCCESS;
    return;
  }

  start_result_ = SocketHandlerStartResult::PARTIAL_SUCCESS;
}

void MdnsResponderManager::CreateMdnsResponder(
    mojo::PendingReceiver<mojom::MdnsResponder> receiver) {
  StartIfNeeded();
  if (start_result_ == SocketHandlerStartResult::UNSPECIFIED ||
      start_result_ == SocketHandlerStartResult::ALL_FAILURE) {
    LOG(ERROR) << "The mDNS responder manager is not started yet.";
    receiver = mojo::NullReceiver();
    return;
  }
  auto responder = std::make_unique<MdnsResponder>(std::move(receiver), this);
  responders_.insert(std::move(responder));
}

bool MdnsResponderManager::Send(scoped_refptr<net::IOBufferWithSize> buf,
                                scoped_refptr<MdnsResponseSendOption> option) {
  DCHECK(buf != nullptr);
  bool all_success = true;
  if (option->send_socket_handler_ids.empty()) {
    for (auto& id_handler_pair : socket_handler_by_id_)
      all_success &= id_handler_pair.second->Send(buf, option);

    return all_success;
  }
  for (auto id : option->send_socket_handler_ids) {
    DCHECK(socket_handler_by_id_.find(id) != socket_handler_by_id_.end());
    all_success &= socket_handler_by_id_[id]->Send(buf, option);
  }
  return all_success;
}

void MdnsResponderManager::OnMojoConnectionError(MdnsResponder* responder) {
  auto it = responders_.find(responder);
  CHECK(it != responders_.end(), base::NotFatalUntil::M130);
  responders_.erase(it);
}

void MdnsResponderManager::SetNameGeneratorForTesting(
    std::unique_ptr<MdnsResponderManager::NameGenerator> name_generator) {
  name_generator_ = std::move(name_generator);
  for (auto& responder : responders_)
    responder->SetNameGeneratorForTesting(name_generator_.get());
}

void MdnsResponderManager::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
  for (auto& id_handler_pair : socket_handler_by_id_) {
    id_handler_pair.second->SetTickClockForTesting(tick_clock);
  }
}

void MdnsResponderManager::HandleAddressNameConflictIfAny(
    const std::map<std::string, std::set<net::IPAddress>>& external_maps) {
  // Handle conflicts in names for address records.
  for (const auto& name_to_addresses : external_maps) {
    for (auto& responder : responders_) {
      if (responder->HasConflictWithExternalResolution(
              name_to_addresses.first, {name_to_addresses.second})) {
        // In the rare case when we encounter conflicting resolutions for a
        // randomly generated name, We close the connection and let the other
        // side of the pipe observe and handle the error, which could possibly
        // rebind to a responder and generate new names.
        OnMojoConnectionError(responder.get());
        // Since each name is uniquely owned by one instance of responders, we
        // can stop searching for this name once we find one conflict.
        break;
      }
    }
  }
}

void MdnsResponderManager::HandleTxtNameConflict() {
  // We will no longer respond to queries to list the generated names. This also
  // cancels the scheduled responses.
  LOG(ERROR)
      << "Stop responding to queries for the mDNS name generator service after "
         "observing a name conflict from an external TXT record.";
  should_respond_to_generator_service_query_ = false;
}

void MdnsResponderManager::OnMdnsQueryReceived(
    const net::DnsQuery& query,
    uint16_t recv_socket_handler_id) {
  // TODO(qingsi): Ideally we should consolidate the handling of the service
  // query using the same responder mechanism as after this block (i.e. there
  // would be a responder owning the service instance name). The current
  // responder only provides APIs to create address records, and hence limited
  // to handle only such records. Once we have expanded the API surface to
  // include the service publishing, the handling logic should be unified.
  const std::optional<std::string> qname =
      net::dns_names_util::NetworkToDottedName(query.qname());
  if (base::FeatureList::IsEnabled(
          features::kMdnsResponderGeneratedNameListing)) {
    if (should_respond_to_generator_service_query_ && qname &&
        qname.value() == kMdnsNameGeneratorServiceInstanceName) {
      HandleMdnsNameGeneratorServiceQuery(query, recv_socket_handler_id);
      return;
    }
  }

  for (auto& responder : responders_)
    responder->OnMdnsQueryReceived(query, recv_socket_handler_id);
}

void MdnsResponderManager::OnSocketHandlerReadError(uint16_t socket_handler_id,
                                                    int result) {
  VLOG(1) << "Socket read error, socket=" << socket_handler_id
          << ", error=" << result;

  // We should not remove the socket handler for a non-fatal error.
  DCHECK(IsFatalError(result));
  auto it = socket_handler_by_id_.find(socket_handler_id);
  CHECK(it != socket_handler_by_id_.end(), base::NotFatalUntil::M130);
  // It is safe to remove the handler in error since the handler has exited the
  // read loop and is done with |OnRead|.
  socket_handler_by_id_.erase(it);
  if (socket_handler_by_id_.empty()) {
    LOG(ERROR)
        << "All socket handlers failed. Restarting the mDNS responder manager.";
    start_result_ = MdnsResponderManager::SocketHandlerStartResult::UNSPECIFIED;
    DCHECK(throttled_start_end_.is_null());
    StartIfNeeded();
  }
}

bool MdnsResponderManager::IsFatalError(int result) {
  if (result >= 0)
    return false;
  if (result == net::ERR_MSG_TOO_BIG || result == net::ERR_IO_PENDING)
    return false;

  return true;
}

void MdnsResponderManager::HandleMdnsNameGeneratorServiceQuery(
    const net::DnsQuery& query,
    uint16_t recv_socket_handler_id) {
  uint16_t qtype = query.qtype();
  if (qtype != net::dns_protocol::kTypeTXT && !IsProbeQuery(query)) {
    VLOG(1) << "The mDNS name generator service query is discarded. Only "
               "queries for TXT records or probe queries are supported.";
    return;
  }

  if (names_.empty()) {
    VLOG(1) << "The mDNS name generator service query is discarded. No "
               "registered names to respond.";
    return;
  }

  auto option = base::MakeRefCounted<MdnsResponseSendOption>();
  option->send_socket_handler_ids.insert(recv_socket_handler_id);
  option->names_for_rate_limit.insert(kMdnsNameGeneratorServiceInstanceName);
  if (IsProbeQuery(query)) {
    option->klass = MdnsResponseSendOption::ResponseClass::PROBE_RESOLUTION;
  } else {
    option->klass = MdnsResponseSendOption::ResponseClass::REGULAR_RESOLUTION;
  }
  // There can be other Chrome instances in the same network that would respond
  // to this query.
  option->shared_result = true;
  option->cancelled_callback = base::BindRepeating(
      [](base::WeakPtr<MdnsResponderManager> manager) {
        return !manager || !manager->should_respond_to_generator_service_query_;
      },
      weak_factory_.GetWeakPtr());
  Send(mdns_helper::CreateResponseToMdnsNameGeneratorServiceQuery(
           kDefaultTtlForRecordWithHostname, names_),
       std::move(option));
  names_in_last_generator_response_ = names_;
}

// TODO(qingsi): When the list of owned names are updated, if we have ever sent
// a response to the generator service query, we should send a goodbye for the
// stale list of names and an update to advertise the new list. See RFC 6762,
// Section 8.4. Currently we only send the goodbye when the manager is
// destroyed. See the destructor of the manager.
void MdnsResponderManager::
    SendGoodbyePacketForMdnsNameGeneratorServiceIfNecessary() {
  if (names_in_last_generator_response_.empty())
    return;

  auto option = base::MakeRefCounted<MdnsResponseSendOption>();
  // Send on all interfaces by not setting the send socket.
  option->klass = MdnsResponseSendOption::ResponseClass::GOODBYE;
  // We do not set |shared_result| in the option for the goodbye to avoid the
  // random delay. The delay would guarantee the cancelling of the scheduled
  // send after the manager is destroyed.
  Send(mdns_helper::CreateResponseToMdnsNameGeneratorServiceQuery(
           base::TimeDelta(), names_in_last_generator_response_),
       std::move(option));
}

int MdnsResponderManager::SocketHandler::HandlePacket(int result) {
  if (result == 0 || result == net::ERR_IO_PENDING)
    return result;
  if (result < 0)
    return responder_manager_->IsFatalError(result) ? result : net::OK;

  net::DnsQuery query(io_buffer_.get());
  bool parsed_as_query = query.Parse(result);
  if (parsed_as_query) {
    responder_manager_->OnMdnsQueryReceived(query, id_);
    return result;
  }

  net::DnsResponse response(io_buffer_.get(), io_buffer_->size());
  if (!response.InitParseWithoutQuery(io_buffer_->size()) ||
      response.answer_count() == 0)
    return result;

  // There could be multiple records for the same name in the response.
  std::map<std::string, std::set<net::IPAddress>> external_address_maps;
  bool has_txt_record_conflict = false;
  auto parser = response.Parser();
  DCHECK_GT(response.answer_count(), 0u);
  for (size_t i = 0; i < response.answer_count(); ++i) {
    auto parsed_record =
        net::RecordParsed::CreateFrom(&parser, base::Time::Now());
    if (!parsed_record || !parsed_record->ttl())
      continue;

    switch (parsed_record->type()) {
      case net::ARecordRdata::kType:
        external_address_maps[parsed_record->name()].insert(
            parsed_record->rdata<net::ARecordRdata>()->address());
        break;
      case net::AAAARecordRdata::kType:
        external_address_maps[parsed_record->name()].insert(
            parsed_record->rdata<net::AAAARecordRdata>()->address());
        break;
      case net::TxtRecordRdata::kType: {
        if (parsed_record->name() == kMdnsNameGeneratorServiceInstanceName &&
            parsed_record->klass() & net::dns_protocol::kFlagCacheFlush)
          // TODO(qingsi): Do not share the instance name once we implement the
          // DNS-SD scheme for responding to service queries. For now we should
          // also validate that the TXT record follows the same key/value pair
          // scheme in |CreateTxtRdataWithNames| even if the cache-flush bit not
          // set.
          //
          // We currently allow Chrome instances to share the same instance name
          // for their lists of owned names, by not setting the cache-flush bit,
          // and hence the above conflict detection logic. If net::MdnsClient is
          // the intended receiver of these responses, it currently can not
          // merge the responses from multiple instances. Once we move to fully
          // implementing the DNS-SD scheme, this issue should be solved after
          // we use distinct instance names in the replied SRV and TXT records.
          has_txt_record_conflict = true;
        break;
      }
      default:
        break;
    }
  }
  responder_manager_->HandleAddressNameConflictIfAny(external_address_maps);

  if (has_txt_record_conflict)
    responder_manager_->HandleTxtNameConflict();

  return result;
}

MdnsResponder::MdnsResponder(
    mojo::PendingReceiver<mojom::MdnsResponder> receiver,
    MdnsResponderManager* manager)
    : receiver_(this, std::move(receiver)),
      manager_(manager),
      name_generator_(manager_->name_generator()) {
  receiver_.set_disconnect_handler(
      base::BindOnce(&MdnsResponderManager::OnMojoConnectionError,
                     base::Unretained(manager_), this));
}

MdnsResponder::~MdnsResponder() {
  for (const auto& name_addr_pair : name_addr_map_) {
    bool rv = manager_->RemoveName(name_addr_pair.first);
    DCHECK(rv);
  }
  SendGoodbyePacketForNameAddressMap(name_addr_map_);
}

void MdnsResponder::CreateNameForAddress(
    const net::IPAddress& address,
    mojom::MdnsResponder::CreateNameForAddressCallback callback) {
  DCHECK(address.IsValid() || address.empty());
  if (!address.IsValid()) {
    LOG(ERROR) << "Invalid IP address to create a name for";
    receiver_.reset();
    manager_->OnMojoConnectionError(this);
    return;
  }
  std::string name;
  auto it = FindNameCreatedForAddress(address);
  bool announcement_sched_at_least_once = false;
  if (it == name_addr_map_.end()) {
    name = name_generator_->CreateName() + ".local";
    // The name should be uniquely owned by one instance of responders.
    bool rv = manager_->AddName(name);
    DCHECK(rv);
    name_addr_map_[name] = address;
    DCHECK(name_refcount_map_.find(name) == name_refcount_map_.end());
    name_refcount_map_[name] = 1;
    // RFC 6762, Section 8.3.
    //
    // Send mDNS announcements, one second apart, for the newly created
    // name-address association. The scheduler will pace the announcements.
    std::map<std::string, net::IPAddress> map_to_announce({{name, address}});
    auto option = base::MakeRefCounted<MdnsResponseSendOption>();
    // Send on all interfaces.
    option->klass = MdnsResponseSendOption::ResponseClass::ANNOUNCEMENT;
    for (int i = 0; i < kMinNumAnnouncementsToSend; ++i) {
      bool announcement_scheduled = SendMdnsResponse(
          mdns_helper::CreateResolutionResponse(
              kDefaultTtlForRecordWithHostname, map_to_announce),
          option);
      announcement_sched_at_least_once |= announcement_scheduled;
      if (!announcement_scheduled)
        break;
    }
  } else {
    name = it->first;
    DCHECK(name_refcount_map_.find(name) != name_refcount_map_.end());
    ++name_refcount_map_[name];
  }
  std::move(callback).Run(name, announcement_sched_at_least_once);
}

void MdnsResponder::RemoveNameForAddress(
    const net::IPAddress& address,
    mojom::MdnsResponder::RemoveNameForAddressCallback callback) {
  DCHECK(address.IsValid() || address.empty());
  auto it = FindNameCreatedForAddress(address);
  if (it == name_addr_map_.end()) {
    std::move(callback).Run(false /* removed */, false /* goodbye_scheduled */);
    return;
  }
  std::string name = it->first;
  DCHECK(name_refcount_map_.find(name) != name_refcount_map_.end());
  auto refcount = --name_refcount_map_[name];
  bool goodbye_scheduled = false;
  if (refcount == 0) {
    goodbye_scheduled = SendGoodbyePacketForNameAddressMap({*it});
    // The name removed should be previously owned by one instance of
    // responders.
    bool rv = manager_->RemoveName(name);
    DCHECK(rv);
    name_refcount_map_.erase(name);
    name_addr_map_.erase(it);
  }
  DCHECK(refcount == 0 || !goodbye_scheduled);
  std::move(callback).Run(refcount == 0, goodbye_scheduled);
}

void MdnsResponder::OnMdnsQueryReceived(const net::DnsQuery& query,
                                        uint16_t recv_socket_handler_id) {
  // Currently we only support a single question in DnsQuery.
  std::optional<std::string> dotted_name_to_resolve =
      net::dns_names_util::NetworkToDottedName(query.qname());
  if (!dotted_name_to_resolve)
    return;
  auto it = name_addr_map_.find(dotted_name_to_resolve.value());
  if (it == name_addr_map_.end())
    return;

  std::map<std::string, net::IPAddress> map_to_respond({*it});
  auto option = base::MakeRefCounted<MdnsResponseSendOption>();
  option->send_socket_handler_ids.insert(recv_socket_handler_id);
  option->names_for_rate_limit.insert(it->first);
  if (!QueryTypeAndAddressFamilyAreCompatible(query.qtype(),
                                              GetAddressFamily(it->second))) {
    // The query asks for a record that does not exist for the name and we send
    // a negative response.
    option->klass = MdnsResponseSendOption::ResponseClass::NEGATIVE;

    SendMdnsResponse(mdns_helper::CreateNegativeResponse(map_to_respond),
                     std::move(option));
    return;
  }
  // TODO(qingsi): Once we update DnsQuery and IsProbeQuery to properly detect
  // probe queries (see the comment inside IsProbeQuery), we should check the
  // probe queries first for conflicting records of names we own, and send the
  // negative responses without rate limiting. In other words, the check above
  // with QueryTypeAndAddressFamilyAreCompatible that results in the per-record
  // rate limiting should not apply to negative responses to probe queries.
  if (IsProbeQuery(query))
    option->klass = MdnsResponseSendOption::ResponseClass::PROBE_RESOLUTION;
  else
    option->klass = MdnsResponseSendOption::ResponseClass::REGULAR_RESOLUTION;

  // Send the name resolution for the received query.
  SendMdnsResponse(mdns_helper::CreateResolutionResponse(
                       kDefaultTtlForRecordWithHostname, map_to_respond),
                   std::move(option));
}

bool MdnsResponder::HasConflictWithExternalResolution(
    const std::string& name,
    const std::set<net::IPAddress>& external_mapped_addreses) {
  DCHECK(!external_mapped_addreses.empty());
  auto matching_record_it = name_addr_map_.find(name);
  if (matching_record_it == name_addr_map_.end())
    return false;

  if (external_mapped_addreses.size() == 1 &&
      *external_mapped_addreses.begin() == matching_record_it->second) {
    VLOG(1) << "Received an external response for an owned record.";
    return false;
  }

  LOG(ERROR) << "Received conflicting resolution for name: " << name;
  return true;
}

bool MdnsResponder::SendMdnsResponse(
    scoped_refptr<net::IOBufferWithSize> response,
    scoped_refptr<MdnsResponseSendOption> option) {
  DCHECK_NE(MdnsResponseSendOption::ResponseClass::UNSPECIFIED, option->klass);
  return manager_->Send(std::move(response), std::move(option));
}

bool MdnsResponder::SendGoodbyePacketForNameAddressMap(
    const std::map<std::string, net::IPAddress>& name_addr_map) {
  if (name_addr_map.empty())
    return false;

  auto option = base::MakeRefCounted<MdnsResponseSendOption>();
  // Send on all interfaces.
  option->klass = MdnsResponseSendOption::ResponseClass::GOODBYE;
  return SendMdnsResponse(mdns_helper::CreateResolutionResponse(
                              base::TimeDelta() /* ttl */, name_addr_map),
                          std::move(option));
}

std::map<std::string, net::IPAddress>::iterator
MdnsResponder::FindNameCreatedForAddress(const net::IPAddress& address) {
  auto ret = name_addr_map_.end();
  size_t count = 0;
  for (auto it = name_addr_map_.begin(); it != name_addr_map_.end(); ++it) {
    if (it->second == address) {
      ret = it;
      ++count;
      DCHECK_LE(count, 1u);
    }
  }
  return ret;
}

}  // namespace network
