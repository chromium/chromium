// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_MDNS_RESPONDER_H_
#define SERVICES_NETWORK_MDNS_RESPONDER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"

namespace net {
class IOBufferWithSize;
class IPAddress;
class MDnsSocketFactory;
}  // namespace net

namespace network {

class MdnsResponder;

namespace mdns_helper {

// Creates an mDNS response, of which the Answer section contains the address
// records for |name_addr_map|, and the Additional section contains the
// corresponding NSEC records that assert the existence of only address
// records in the Answer section.
COMPONENT_EXPORT(NETWORK_SERVICE)
scoped_refptr<net::IOBufferWithSize> CreateResolutionResponse(
    const base::TimeDelta& ttl,
    const std::map<std::string, net::IPAddress>& name_addr_map);
// Creates an mDNS response, of which the Answer section contains NSEC records
// that assert the existence of only address records for |name_addr_map|, and
// the Additional section contains the corresponding address records.
COMPONENT_EXPORT(NETWORK_SERVICE)
scoped_refptr<net::IOBufferWithSize> CreateNegativeResponse(
    const std::map<std::string, net::IPAddress>& name_addr_map);
// Creates an mDNS response to an mDNS name generator service query.
//
// An mDNS name generator service query is a query for TXT records associated
// with the name "Generated-Names._mdns_name_generator._udp.local". It is
// similar to the service query defined in DNS-SD but is not an implementation
// of the specification in RFC 6763. The Answer section of the response contains
// a TXT record for the list of |mdns_names|. The cache-flush bit is set in the
// TXT record if |cached_flush| is true.
COMPONENT_EXPORT(NETWORK_SERVICE)
scoped_refptr<net::IOBufferWithSize>
CreateResponseToMdnsNameGeneratorServiceQuery(
    const base::TimeDelta& ttl,
    const std::set<std::string>& mdns_names);

}  // namespace mdns_helper

// Options to configure the transmission of mDNS responses.
struct COMPONENT_EXPORT(NETWORK_SERVICE) MdnsResponseSendOption
    : public base::RefCounted<MdnsResponseSendOption> {
 public:
  enum class ResponseClass {
    UNSPECIFIED,
    ANNOUNCEMENT,
    PROBE_RESOLUTION,
    REGULAR_RESOLUTION,
    NEGATIVE,
    GOODBYE,
  };

  MdnsResponseSendOption();
  // As a shorthand, an empty set denotes all interfaces.
  std::set<uint16_t> send_socket_handler_ids;
  // Used for rate limiting.
  base::flat_set<std::string> names_for_rate_limit;
  // Used for retry after send failure.
  ResponseClass klass = ResponseClass::UNSPECIFIED;
  // The number of retries done for the same response due to send failure.
  uint8_t num_send_retries_done = 0;
  // Indicates if the response includes a resource record that is a member of a
  // shared resource record set.
  bool shared_result = false;
  // If not nullopt, returns true if the response to send is cancelled.
  std::optional<base::RepeatingCallback<bool()>> cancelled_callback;

 private:
  friend class base::RefCounted<MdnsResponseSendOption>;

  ~MdnsResponseSendOption();
};

// The responder manager creates and manages responder instances spawned for
// each Mojo binding. It also manages the underlying network IO by delegating
// the IO task to socket handlers of each interface. When there is a network
// stack error or a Mojo binding error, the manager also offers the
// corresponding error handling.
class COMPONENT_EXPORT(NETWORK_SERVICE) MdnsResponderManager {
 public:
  // Wraps a name generation method that can be configured in tests via
  // SetNameGeneratorForTesting below.
  //
  // The generated name by |CreateName| must be no more than 245 characters.
  class NameGenerator {
   public:
    virtual ~NameGenerator() = default;
    virtual std::string CreateName() = 0;
  };

  // Used in histograms to measure the service health.
  enum class ServiceError {
    // Fail to start the MdnsResponderManager after all socket handlers fail to
    // start on each interface.
    kFailToStartManager = 0,
    // Fail to create a MdnsResponder after all socket handlers fail to start on
    // each interface.
    kFailToCreateResponder = 1,
    // All socket handlers have encountered read errors and failed. Imminent to
    // restart the MdnsResponderManager.
    kFatalSocketHandlerError = 2,
    // An invalid IP address is given to register an mDNS name for.
    kInvalidIpToRegisterName = 3,
    // A record is received from the network such that it resolves a name
    // created
    // by the service to a different address.
    kConflictingNameResolution = 4,

    kMaxValue = kConflictingNameResolution,
  };

  // Delay between throttled attempts to start the `MdnsResponderManager`.
  constexpr static base::TimeDelta kManagerStartThrottleDelay =
      base::Seconds(1);

  MdnsResponderManager();
  explicit MdnsResponderManager(net::MDnsSocketFactory* socket_factory);

  MdnsResponderManager(const MdnsResponderManager&) = delete;
  MdnsResponderManager& operator=(const MdnsResponderManager&) = delete;

  ~MdnsResponderManager();

  // Creates an instance of MdnsResponder for the receiver.
  void CreateMdnsResponder(
      mojo::PendingReceiver<mojom::MdnsResponder> receiver);
  // The methods below are used to bookkeep names owned by responders, and
  // also for the extra uniqueness validation of these names. By default,
  // we use the RandomUuidNameGenerator (see mdns_responder.cc), which
  // probabilistically guarantees the uniqueness of generated names.
  //
  // Adds a name to the set of all existing names generated by all responders
  // (i.e., names owned by an instance of responder). Return true if the name is
  // not in the set before the addition; false otherwise.
  bool AddName(const std::string& name) {
    auto result = names_.insert(name);
    return result.second;
  }
  // Removes a name from the set of all existing names generated by all
  // responders. Return true if the name exists in the set before the removal;
  // false otherwise.
  bool RemoveName(const std::string& name) { return names_.erase(name) == 1; }
  // Sends an mDNS response in the wire format given by |buf|. See
  // MdnsResponseSendOption for configurable options in |option|.
  //
  // Sending responses is rate-limited, and this method returns true if the
  // response is successfully scheduled to send on all successfully bound
  // interfaces specified in |option.send_socket_handler_ids|, and false
  // otherwise.
  bool Send(scoped_refptr<net::IOBufferWithSize> buf,
            scoped_refptr<MdnsResponseSendOption> option);
  // The error handler that is invoked when the Mojo binding of |responder| is
  // closed (e.g. the InterfacePtr on the client side is destroyed) or
  // encounters an error. It removes this responder instance, which further
  // clears the existing name-address associations owned by this responder in
  // the local network.
  void OnMojoConnectionError(MdnsResponder* responder);

  // Called when an external mDNS response is received and it contains address
  // records of names generated by an owned MdnsResponder instance.
  void HandleAddressNameConflictIfAny(
      const std::map<std::string, std::set<net::IPAddress>>&
          external_address_maps);
  // Called when an external mDNS response is received and it contains a TXT
  // record for the mDNS name generator service instance name with the
  // cache-flush bit set.
  void HandleTxtNameConflict();

  NameGenerator* name_generator() const { return name_generator_.get(); }
  // Sets the name generator that is shared by all MdnsResponder instances.
  // Changing the name generator affects all existing responder instances and
  // also the ones spawned in the future.
  //
  // Used for tests only.
  void SetNameGeneratorForTesting(
      std::unique_ptr<NameGenerator> name_generator);

  // Sets the tick clock that is used for rate limiting of mDNS responses, and
  // also resets the internal schedule for rate limiting.
  //
  // Used for tests only.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

 private:
  enum class SocketHandlerStartResult {
    UNSPECIFIED,
    // Handlers started for all interfaces.
    ALL_SUCCESS,
    // Handlers started for a subset of interfaces.
    PARTIAL_SUCCESS,
    // No handler started.
    ALL_FAILURE,
  };
  // Handles the underlying sending and receiving of mDNS messages on each
  // interface available. The implementation mostly resembles
  // net::MdnsConnection::SocketHandler.
  class SocketHandler;

  // Initializes socket handlers and sets `start_result_`. Safe to call multiple
  // times after success or failure (and will noop after success). As a
  // protection against spammy network usage this will also noop if called too
  // soon after the last failure.
  void StartIfNeeded();
  // Dispatches a parsed query from a socket handler to each responder instance.
  void OnMdnsQueryReceived(const net::DnsQuery& query,
                           uint16_t recv_socket_handler_id);
  void OnSocketHandlerReadError(uint16_t socket_handler_id, int result);
  bool IsFatalError(int result);
  // Called when an mDNS name service query is received. The manager fills a
  // list of registered names in a TXT record in the response.
  void HandleMdnsNameGeneratorServiceQuery(const net::DnsQuery& query,
                                           uint16_t recv_socket_handler_id);
  // Sends a zero-TTL mDNS response with a TXT record of mDNS names from the
  // last generator service response sent. No-op if no generator service
  // response sent previously.
  void SendGoodbyePacketForMdnsNameGeneratorServiceIfNecessary();

  std::unique_ptr<net::MDnsSocketFactory> owned_socket_factory_;
  raw_ptr<net::MDnsSocketFactory> socket_factory_;
  // Only the socket handlers that have successfully bound and started are kept.
  std::map<uint16_t, std::unique_ptr<SocketHandler>> socket_handler_by_id_;
  SocketHandlerStartResult start_result_ =
      SocketHandlerStartResult::UNSPECIFIED;
  // Used to bookkeep all names owned by |responders_| for
  // 1. the extra uniqueness validation of names generated by responders;
  // 2. generating responses to the mDNS name generator service queries.
  std::set<std::string> names_;
  std::unique_ptr<NameGenerator> name_generator_;
  // The names used to create the last response to a generator service query.
  std::set<std::string> names_in_last_generator_response_;
  bool should_respond_to_generator_service_query_ = true;

  std::set<std::unique_ptr<MdnsResponder>, base::UniquePtrComparator>
      responders_;

  raw_ptr<const base::TickClock> tick_clock_ =
      base::DefaultTickClock::GetInstance();

  // If not `base::TimeTicks()`, represents the end of the throttling period for
  // calls to `StartIfNeeded()`.
  base::TimeTicks throttled_start_end_;

  base::WeakPtrFactory<MdnsResponderManager> weak_factory_{this};
};

// Implementation of the mDNS service that can provide utilities of an mDNS
// responder.
class COMPONENT_EXPORT(NETWORK_SERVICE) MdnsResponder
    : public mojom::MdnsResponder {
 public:
  MdnsResponder(mojo::PendingReceiver<mojom::MdnsResponder> receiver,
                MdnsResponderManager* manager);

  MdnsResponder(const MdnsResponder&) = delete;
  MdnsResponder& operator=(const MdnsResponder&) = delete;

  // When destroyed, clears all existing name-address associations owned by this
  // responder in the local network by sending out goodbye packets. See
  // SendGoodbyePacketForNameAddressMap below.
  ~MdnsResponder() override;

  // mojom::MdnsResponder overrides.
  void CreateNameForAddress(
      const net::IPAddress& address,
      mojom::MdnsResponder::CreateNameForAddressCallback callback) override;
  void RemoveNameForAddress(
      const net::IPAddress& address,
      mojom::MdnsResponder::RemoveNameForAddressCallback callback) override;

  // Processes the given query and generates a response if the query contains an
  // mDNS name that this responder has a mapped IP address.
  void OnMdnsQueryReceived(const net::DnsQuery& query,
                           uint16_t recv_socket_handler_id);

  bool HasConflictWithExternalResolution(
      const std::string& name,
      const std::set<net::IPAddress>& external_mapped_addreses);

  void SetNameGeneratorForTesting(
      MdnsResponderManager::NameGenerator* name_generator) {
    name_generator_ = name_generator;
  }

 private:
  // Returns true if the response is successfully scheduled to send on all
  // successfully bound interfaces after rate limiting, and false otherwise. See
  // also MdnsResponderManager::Send.
  bool SendMdnsResponse(scoped_refptr<net::IOBufferWithSize> response,
                        scoped_refptr<MdnsResponseSendOption> option);
  // RFC 6761, Section 10.1 "Goodbye Packets".
  //
  // The responder should send out an unsolicited mDNS response with an resource
  // record of zero TTL to clear the name-to-address mapping in neighboring
  // hosts, when the mapping is no longer valid.
  //
  // Returns true if the goodbye message is successfully scheduled to send on
  // all interfaces after rate limiting, and false otherwise. See also
  // MdnsResponderManager::Send.
  bool SendGoodbyePacketForNameAddressMap(
      const std::map<std::string, net::IPAddress>& name_addr_map);

  std::map<std::string, net::IPAddress>::iterator FindNameCreatedForAddress(
      const net::IPAddress& address);

  mojo::Receiver<network::mojom::MdnsResponder> receiver_;
  // A back pointer to the responder manager that owns this responder. The
  // responder should be destroyed before |manager_| becomes invalid or a weak
  // reference should be used to access the manager when there is no such
  // guarantee in an operation.
  const raw_ptr<MdnsResponderManager> manager_;
  std::map<std::string, net::IPAddress> name_addr_map_;
  std::map<std::string, uint16_t> name_refcount_map_;
  raw_ptr<MdnsResponderManager::NameGenerator> name_generator_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_MDNS_RESPONDER_H_
