// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MDNS_CLIENT_IMPL_H_
#define NET_DNS_MDNS_CLIENT_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/dns/mdns_cache.h"
#include "net/dns/mdns_client.h"
#include "net/socket/datagram_server_socket.h"
#include "net/socket/udp_server_socket.h"
#include "net/socket/udp_socket.h"

namespace base {
class Clock;
class OneShotTimer;
}  // namespace base

namespace net {

class NetLog;

class MDnsSocketFactoryImpl : public MDnsSocketFactory {
 public:
  MDnsSocketFactoryImpl() : net_log_(nullptr) {}
  explicit MDnsSocketFactoryImpl(NetLog* net_log) : net_log_(net_log) {}

  MDnsSocketFactoryImpl(const MDnsSocketFactoryImpl&) = delete;
  MDnsSocketFactoryImpl& operator=(const MDnsSocketFactoryImpl&) = delete;

  ~MDnsSocketFactoryImpl() override = default;

  void CreateSockets(
      std::vector<std::unique_ptr<DatagramServerSocket>>* sockets) override;

 private:
  const raw_ptr<NetLog> net_log_;
};

// A connection to the network for multicast DNS clients. It reads data into
// DnsResponse objects and alerts the delegate that a packet has been received.
class NET_EXPORT_PRIVATE MDnsConnection {
 public:
  class Delegate {
   public:
    // Handle an mDNS packet buffered in |response| with a size of |bytes_read|.
    virtual void HandlePacket(DnsResponse* response, int bytes_read) = 0;
    virtual void OnConnectionError(int error) = 0;
    virtual ~Delegate() = default;
  };

  explicit MDnsConnection(MDnsConnection::Delegate* delegate);

  MDnsConnection(const MDnsConnection&) = delete;
  MDnsConnection& operator=(const MDnsConnection&) = delete;

  virtual ~MDnsConnection();

  // Succeeds if at least one of the socket handlers succeeded.
  int Init(MDnsSocketFactory* socket_factory);
  void Send(const scoped_refptr<IOBuffer>& buffer, unsigned size);

 private:
  class SocketHandler {
   public:
    SocketHandler(std::unique_ptr<DatagramServerSocket> socket,
                  MDnsConnection* connection);

    SocketHandler(const SocketHandler&) = delete;
    SocketHandler& operator=(const SocketHandler&) = delete;

    ~SocketHandler();

    int Start();
    void Send(const scoped_refptr<IOBuffer>& buffer, unsigned size);

   private:
    int DoLoop(int rv);
    void OnDatagramReceived(int rv);

    // Callback for when sending a query has finished.
    void SendDone(int rv);

    std::unique_ptr<DatagramServerSocket> socket_;
    raw_ptr<MDnsConnection> connection_;
    IPEndPoint recv_addr_;
    DnsResponse response_;
    IPEndPoint multicast_addr_;
    bool send_in_progress_ = false;
    base::queue<std::pair<scoped_refptr<IOBuffer>, unsigned>> send_queue_;
  };

  // Callback for handling a datagram being received on either ipv4 or ipv6.
  void OnDatagramReceived(DnsResponse* response,
                          const IPEndPoint& recv_addr,
                          int bytes_read);

  void PostOnError(SocketHandler* loop, int rv);
  void OnError(int rv);

  // Only socket handlers which successfully bound and started are kept.
  std::vector<std::unique_ptr<SocketHandler>> socket_handlers_;

  raw_ptr<Delegate> delegate_;

  base::WeakPtrFactory<MDnsConnection> weak_ptr_factory_{this};
};

class MDnsListenerImpl;

class NET_EXPORT_PRIVATE MDnsClientImpl : public MDnsClient {
 public:
  // The core object exists while the MDnsClient is listening, and is deleted
  // whenever the number of listeners reaches zero. The deletion happens
  // asychronously, so destroying the last listener does not immediately
  // invalidate the core.
  class Core final : public MDnsConnection::Delegate {
   public:
    Core(base::Clock* clock, base::OneShotTimer* timer);

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    ~Core() override;

    // Initialize the core.
    int Init(MDnsSocketFactory* socket_factory);

    // Send a query with a specific rrtype and name. Returns true on success.
    bool SendQuery(uint16_t rrtype, const std::string& name);

    // Add/remove a listener to the list of listeners.
    void AddListener(MDnsListenerImpl* listener);
    void RemoveListener(MDnsListenerImpl* listener);

    // Query the cache for records of a specific type and name.
    void QueryCache(uint16_t rrtype,
                    const std::string& name,
                    std::vector<const RecordParsed*>* records) const;

    // Parse the response and alert relevant listeners.
    void HandlePacket(DnsResponse* response, int bytes_read) override;

    void OnConnectionError(int error) override;

    MDnsCache* cache_for_testing() { return &cache_; }

   private:
    FRIEND_TEST_ALL_PREFIXES(MDnsTest, CacheCleanupWithShortTTL);

    class ListenerKey {
     public:
      ListenerKey(const std::string& name, uint16_t type);
      ListenerKey(const ListenerKey&) = default;
      ListenerKey(ListenerKey&&) = default;
      bool operator<(const ListenerKey& key) const;
      const std::string& name_lowercase() const { return name_lowercase_; }
      uint16_t type() const { return type_; }

     private:
      std::string name_lowercase_;
      uint16_t type_;
    };
    typedef base::ObserverList<MDnsListenerImpl>::Unchecked ObserverListType;
    typedef std::map<ListenerKey, std::unique_ptr<ObserverListType>>
        ListenerMap;

    // Alert listeners of an update to the cache.
    void AlertListeners(MDnsCache::UpdateType update_type,
                        const ListenerKey& key, const RecordParsed* record);

    // Schedule a cache cleanup to a specific time, cancelling other cleanups.
    void ScheduleCleanup(base::Time cleanup);

    // Clean up the cache and schedule a new cleanup.
    void DoCleanup();

    // Callback for when a record is removed from the cache.
    void OnRecordRemoved(const RecordParsed* record);

    void NotifyNsecRecord(const RecordParsed* record);

    // Delete and erase the observer list for |key|. Only deletes the observer
    // list if is empty.
    void CleanupObserverList(const ListenerKey& key);

    ListenerMap listeners_;

    MDnsCache cache_;

    raw_ptr<base::Clock> clock_;
    raw_ptr<base::OneShotTimer> cleanup_timer_;
    base::Time scheduled_cleanup_;

    std::unique_ptr<MDnsConnection> connection_;
    base::WeakPtrFactory<Core> weak_ptr_factory_{this};
  };

  MDnsClientImpl();

  // Test constructor, takes a mock clock and mock timer.
  MDnsClientImpl(base::Clock* clock,
                 std::unique_ptr<base::OneShotTimer> cleanup_timer);

  MDnsClientImpl(const MDnsClientImpl&) = delete;
  MDnsClientImpl& operator=(const MDnsClientImpl&) = delete;

  ~MDnsClientImpl() override;

  // MDnsClient implementation:
  std::unique_ptr<MDnsListener> CreateListener(
      uint16_t rrtype,
      const std::string& name,
      MDnsListener::Delegate* delegate) override;

  std::unique_ptr<MDnsTransaction> CreateTransaction(
      uint16_t rrtype,
      const std::string& name,
      int flags,
      const MDnsTransaction::ResultCallback& callback) override;

  int StartListening(MDnsSocketFactory* socket_factory) override;
  void StopListening() override;
  bool IsListening() const override;

  Core* core() { return core_.get(); }

 private:
  raw_ptr<base::Clock> clock_;
  std::unique_ptr<base::OneShotTimer> cleanup_timer_;

  std::unique_ptr<Core> core_;
};

class MDnsListenerImpl final : public MDnsListener {
 public:
  MDnsListenerImpl(uint16_t rrtype,
                   const std::string& name,
                   base::Clock* clock,
                   MDnsListener::Delegate* delegate,
                   MDnsClientImpl* client);

  MDnsListenerImpl(const MDnsListenerImpl&) = delete;
  MDnsListenerImpl& operator=(const MDnsListenerImpl&) = delete;

  ~MDnsListenerImpl() override;

  // MDnsListener implementation:
  bool Start() override;

  // Actively refresh any received records.
  void SetActiveRefresh(bool active_refresh) override;

  const std::string& GetName() const override;

  uint16_t GetType() const override;

  MDnsListener::Delegate* delegate() { return delegate_; }

  // Alert the delegate of a record update.
  void HandleRecordUpdate(MDnsCache::UpdateType update_type,
                          const RecordParsed* record_parsed);

  // Alert the delegate of the existence of an Nsec record.
  void AlertNsecRecord();

 private:
  void ScheduleNextRefresh();
  void DoRefresh();

  uint16_t rrtype_;
  std::string name_;
  raw_ptr<base::Clock> clock_;
  raw_ptr<MDnsClientImpl> client_;
  raw_ptr<MDnsListener::Delegate> delegate_;

  base::Time last_update_;
  uint32_t ttl_;
  bool started_ = false;
  bool active_refresh_ = false;

  base::CancelableRepeatingClosure next_refresh_;
  base::WeakPtrFactory<MDnsListenerImpl> weak_ptr_factory_{this};
};

class MDnsTransactionImpl final : public MDnsTransaction,
                                  public MDnsListener::Delegate {
 public:
  MDnsTransactionImpl(uint16_t rrtype,
                      const std::string& name,
                      int flags,
                      const MDnsTransaction::ResultCallback& callback,
                      MDnsClientImpl* client);

  MDnsTransactionImpl(const MDnsTransactionImpl&) = delete;
  MDnsTransactionImpl& operator=(const MDnsTransactionImpl&) = delete;

  ~MDnsTransactionImpl() override;

  // MDnsTransaction implementation:
  bool Start() override;

  const std::string& GetName() const override;
  uint16_t GetType() const override;

  // MDnsListener::Delegate implementation:
  void OnRecordUpdate(MDnsListener::UpdateType update,
                      const RecordParsed* record) override;
  void OnNsecRecord(const std::string& name, unsigned type) override;

  void OnCachePurged() override;

 private:
  bool is_active() { return !callback_.is_null(); }

  void Reset();

  // Trigger the callback and reset all related variables.
  void TriggerCallback(MDnsTransaction::Result result,
                       const RecordParsed* record);

  // Internal callback for when a cache record is found.
  void CacheRecordFound(const RecordParsed* record);

  // Signal the transactionis over and release all related resources.
  void SignalTransactionOver();

  // Reads records from the cache and calls the callback for every
  // record read.
  void ServeRecordsFromCache();

  // Send a query to the network and set up a timeout to time out the
  // transaction. Returns false if it fails to start listening on the network
  // or if it fails to send a query.
  bool QueryAndListen();

  uint16_t rrtype_;
  std::string name_;
  MDnsTransaction::ResultCallback callback_;

  std::unique_ptr<MDnsListener> listener_;
  base::CancelableOnceCallback<void()> timeout_;

  raw_ptr<MDnsClientImpl> client_;

  bool started_ = false;
  int flags_;
  base::WeakPtrFactory<MDnsTransactionImpl> weak_ptr_factory_{this};
};

}  // namespace net
#endif  // NET_DNS_MDNS_CLIENT_IMPL_H_
