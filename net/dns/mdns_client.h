// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MDNS_CLIENT_H_
#define NET_DNS_MDNS_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/record_parsed.h"

namespace net {

class DatagramServerSocket;
class NetLog;
class RecordParsed;

// Represents a one-time record lookup. A transaction takes one
// associated callback (see |MDnsClient::CreateTransaction|) and calls it
// whenever a matching record has been found, either from the cache or
// by querying the network (it may choose to query either or both based on its
// creation flags, see MDnsTransactionFlags). Network-based transactions will
// time out after a reasonable number of seconds.
class NET_EXPORT MDnsTransaction {
 public:
  static const base::TimeDelta kTransactionTimeout;

  // Used to signify what type of result the transaction has received.
  enum Result {
    // Passed whenever a record is found.
    RESULT_RECORD,
    // The transaction is done. Applies to non-single-valued transactions. Is
    // called when the transaction has finished (this is the last call to the
    // callback).
    RESULT_DONE,
    // No results have been found. Applies to single-valued transactions. Is
    // called when the transaction has finished without finding any results.
    // For transactions that use the network, this happens when a timeout
    // occurs, for transactions that are cache-only, this happens when no
    // results are in the cache.
    RESULT_NO_RESULTS,
    // Called when an NSec record is read for this transaction's
    // query. This means there cannot possibly be a record of the type
    // and name for this transaction.
    RESULT_NSEC
  };

  // Used when creating an MDnsTransaction.
  enum Flags {
    // Transaction should return only one result, and stop listening after it.
    // Note that single result transactions will signal when their timeout is
    // reached, whereas multi-result transactions will not.
    SINGLE_RESULT = 1 << 0,
    // Query the cache or the network. May both be used. One must be present.
    QUERY_CACHE = 1 << 1,
    QUERY_NETWORK = 1 << 2,
    // TODO(noamsml): Add flag for flushing cache when feature is implemented
    // Mask of all possible flags on MDnsTransaction.
    FLAG_MASK = (1 << 3) - 1,
  };

  typedef base::RepeatingCallback<void(Result, const RecordParsed*)>
      ResultCallback;

  // Destroying the transaction cancels it.
  virtual ~MDnsTransaction() {}

  // Start the transaction. Return true on success. Cache-based transactions
  // will execute the callback synchronously.
  virtual bool Start() = 0;

  // Get the host or service name for the transaction.
  virtual const std::string& GetName() const = 0;

  // Get the type for this transaction (SRV, TXT, A, AAA, etc)
  virtual uint16_t GetType() const = 0;
};

// A listener listens for updates regarding a specific record or set of records.
// Created by the MDnsClient (see |MDnsClient::CreateListener|) and used to keep
// track of listeners.
//
// TODO(ericorth@chromium.org): Consider moving this inside MDnsClient to better
// organize the namespace and avoid confusion with
// net::HostResolver::MdnsListener.
class NET_EXPORT MDnsListener {
 public:
  // Used in the MDnsListener delegate to signify what type of change has been
  // made to a record.
  enum UpdateType {
    RECORD_ADDED,
    RECORD_CHANGED,
    RECORD_REMOVED
  };

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when a record is added, removed or updated.
    virtual void OnRecordUpdate(UpdateType update,
                                const RecordParsed* record) = 0;

    // Called when a record is marked nonexistent by an NSEC record.
    virtual void OnNsecRecord(const std::string& name, unsigned type) = 0;

    // Called when the cache is purged (due, for example, ot the network
    // disconnecting).
    virtual void OnCachePurged() = 0;
  };

  // Destroying the listener stops listening.
  virtual ~MDnsListener() {}

  // Start the listener. Return true on success.
  virtual bool Start() = 0;

  // Actively refresh any received records.
  virtual void SetActiveRefresh(bool active_refresh) = 0;

  // Get the host or service name for this query.
  // Return an empty string for no name.
  virtual const std::string& GetName() const = 0;

  // Get the type for this query (SRV, TXT, A, AAA, etc)
  virtual uint16_t GetType() const = 0;
};

// Creates bound datagram sockets ready to use by MDnsClient.
class NET_EXPORT MDnsSocketFactory {
 public:
  virtual ~MDnsSocketFactory() {}
  virtual void CreateSockets(
      std::vector<std::unique_ptr<DatagramServerSocket>>* sockets) = 0;

  static std::unique_ptr<MDnsSocketFactory> CreateDefault();
};

// Listens for Multicast DNS on the local network. You can access information
// regarding multicast DNS either by creating an |MDnsListener| to be notified
// of new records, or by creating an |MDnsTransaction| to look up the value of a
// specific records. When all listeners and active transactions are destroyed,
// the client stops listening on the network and destroys the cache.
class NET_EXPORT MDnsClient {
 public:
  virtual ~MDnsClient() {}

  // Create listener object for RRType |rrtype| and name |name|.
  virtual std::unique_ptr<MDnsListener> CreateListener(
      uint16_t rrtype,
      const std::string& name,
      MDnsListener::Delegate* delegate) = 0;

  // Create a transaction that can be used to query either the MDns cache, the
  // network, or both for records of type |rrtype| and name |name|. |flags| is
  // defined by MDnsTransactionFlags.
  virtual std::unique_ptr<MDnsTransaction> CreateTransaction(
      uint16_t rrtype,
      const std::string& name,
      int flags,
      const MDnsTransaction::ResultCallback& callback) = 0;

  virtual int StartListening(MDnsSocketFactory* factory) = 0;

  // Do not call this inside callbacks from related MDnsListener and
  // MDnsTransaction objects.
  virtual void StopListening() = 0;
  virtual bool IsListening() const = 0;

  // Create the default MDnsClient
  static std::unique_ptr<MDnsClient> CreateDefault();
};

// Gets the endpoint for the multicast group a socket should join to receive
// MDNS messages. Such sockets should also bind to the endpoint from
// GetMDnsReceiveEndPoint().
//
// This is also the endpoint messages should be sent to to send MDNS messages.
NET_EXPORT IPEndPoint GetMDnsGroupEndPoint(AddressFamily address_family);

// Gets the endpoint sockets should be bound to to receive MDNS messages. Such
// sockets should also join the multicast group from GetMDnsGroupEndPoint().
NET_EXPORT IPEndPoint GetMDnsReceiveEndPoint(AddressFamily address_family);

typedef std::vector<std::pair<uint32_t, AddressFamily>>
    InterfaceIndexFamilyList;
// Returns pairs of interface and address family to bind. Current
// implementation returns unique list of all available interfaces.
NET_EXPORT InterfaceIndexFamilyList GetMDnsInterfacesToBind();

// Create sockets, binds socket to MDns endpoint, and sets multicast interface
// and joins multicast group on for |interface_index|.
// Returns NULL if failed.
NET_EXPORT std::unique_ptr<DatagramServerSocket> CreateAndBindMDnsSocket(
    AddressFamily address_family,
    uint32_t interface_index,
    NetLog* net_log);

}  // namespace net

#endif  // NET_DNS_MDNS_CLIENT_H_
