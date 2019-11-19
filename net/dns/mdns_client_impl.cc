// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mdns_client_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/dns/dns_util.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/util.h"
#include "net/dns/record_rdata.h"
#include "net/socket/datagram_socket.h"

// TODO(gene): Remove this temporary method of disabling NSEC support once it
// becomes clear whether this feature should be
// supported. http://crbug.com/255232
#define ENABLE_NSEC

namespace net {

namespace {

// The fractions of the record's original TTL after which an active listener
// (one that had |SetActiveRefresh(true)| called) will send a query to refresh
// its cache. This happens both at 85% of the original TTL and again at 95% of
// the original TTL.
const double kListenerRefreshRatio1 = 0.85;
const double kListenerRefreshRatio2 = 0.95;

}  // namespace

void MDnsSocketFactoryImpl::CreateSockets(
    std::vector<std::unique_ptr<DatagramServerSocket>>* sockets) {
  InterfaceIndexFamilyList interfaces(GetMDnsInterfacesToBind());
  for (size_t i = 0; i < interfaces.size(); ++i) {
    DCHECK(interfaces[i].second == ADDRESS_FAMILY_IPV4 ||
           interfaces[i].second == ADDRESS_FAMILY_IPV6);
    std::unique_ptr<DatagramServerSocket> socket(CreateAndBindMDnsSocket(
        interfaces[i].second, interfaces[i].first, net_log_));
    if (socket)
      sockets->push_back(std::move(socket));
  }
}

MDnsConnection::SocketHandler::SocketHandler(
    std::unique_ptr<DatagramServerSocket> socket,
    MDnsConnection* connection)
    : socket_(std::move(socket)),
      connection_(connection),
      response_(dns_protocol::kMaxMulticastSize),
      send_in_progress_(false) {}

MDnsConnection::SocketHandler::~SocketHandler() = default;

int MDnsConnection::SocketHandler::Start() {
  IPEndPoint end_point;
  int rv = socket_->GetLocalAddress(&end_point);
  if (rv != OK)
    return rv;
  DCHECK(end_point.GetFamily() == ADDRESS_FAMILY_IPV4 ||
         end_point.GetFamily() == ADDRESS_FAMILY_IPV6);
  multicast_addr_ = dns_util::GetMdnsGroupEndPoint(end_point.GetFamily());
  return DoLoop(0);
}

int MDnsConnection::SocketHandler::DoLoop(int rv) {
  do {
    if (rv > 0)
      connection_->OnDatagramReceived(&response_, recv_addr_, rv);

    rv = socket_->RecvFrom(
        response_.io_buffer(), response_.io_buffer_size(), &recv_addr_,
        base::BindOnce(&MDnsConnection::SocketHandler::OnDatagramReceived,
                       base::Unretained(this)));
  } while (rv > 0);

  if (rv != ERR_IO_PENDING)
    return rv;

  return OK;
}

void MDnsConnection::SocketHandler::OnDatagramReceived(int rv) {
  if (rv >= OK)
    rv = DoLoop(rv);

  if (rv != OK)
    connection_->PostOnError(this, rv);
}

void MDnsConnection::SocketHandler::Send(const scoped_refptr<IOBuffer>& buffer,
                                         unsigned size) {
  if (send_in_progress_) {
    send_queue_.push(std::make_pair(buffer, size));
    return;
  }
  int rv =
      socket_->SendTo(buffer.get(), size, multicast_addr_,
                      base::BindOnce(&MDnsConnection::SocketHandler::SendDone,
                                     base::Unretained(this)));
  if (rv == ERR_IO_PENDING) {
    send_in_progress_ = true;
  } else if (rv < OK) {
    connection_->PostOnError(this, rv);
  }
}

void MDnsConnection::SocketHandler::SendDone(int rv) {
  DCHECK(send_in_progress_);
  send_in_progress_ = false;
  if (rv != OK)
    connection_->PostOnError(this, rv);
  while (!send_in_progress_ && !send_queue_.empty()) {
    std::pair<scoped_refptr<IOBuffer>, unsigned> buffer = send_queue_.front();
    send_queue_.pop();
    Send(buffer.first, buffer.second);
  }
}

MDnsConnection::MDnsConnection(MDnsConnection::Delegate* delegate)
    : delegate_(delegate) {}

MDnsConnection::~MDnsConnection() = default;

int MDnsConnection::Init(MDnsSocketFactory* socket_factory) {
  std::vector<std::unique_ptr<DatagramServerSocket>> sockets;
  socket_factory->CreateSockets(&sockets);

  for (std::unique_ptr<DatagramServerSocket>& socket : sockets) {
    socket_handlers_.push_back(std::make_unique<MDnsConnection::SocketHandler>(
        std::move(socket), this));
  }

  // All unbound sockets need to be bound before processing untrusted input.
  // This is done for security reasons, so that an attacker can't get an unbound
  // socket.
  int last_failure = ERR_FAILED;
  for (size_t i = 0; i < socket_handlers_.size();) {
    int rv = socket_handlers_[i]->Start();
    if (rv != OK) {
      last_failure = rv;
      socket_handlers_.erase(socket_handlers_.begin() + i);
      VLOG(1) << "Start failed, socket=" << i << ", error=" << rv;
    } else {
      ++i;
    }
  }
  VLOG(1) << "Sockets ready:" << socket_handlers_.size();
  DCHECK_NE(ERR_IO_PENDING, last_failure);
  return socket_handlers_.empty() ? last_failure : OK;
}

void MDnsConnection::Send(const scoped_refptr<IOBuffer>& buffer,
                          unsigned size) {
  for (std::unique_ptr<SocketHandler>& handler : socket_handlers_)
    handler->Send(buffer, size);
}

void MDnsConnection::PostOnError(SocketHandler* loop, int rv) {
  int id = 0;
  for (const auto& it : socket_handlers_) {
    if (it.get() == loop)
      break;
    id++;
  }
  VLOG(1) << "Socket error. id=" << id << ", error=" << rv;
  // Post to allow deletion of this object by delegate.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&MDnsConnection::OnError,
                                weak_ptr_factory_.GetWeakPtr(), rv));
}

void MDnsConnection::OnError(int rv) {
  // TODO(noamsml): Specific handling of intermittent errors that can be handled
  // in the connection.
  delegate_->OnConnectionError(rv);
}

void MDnsConnection::OnDatagramReceived(
    DnsResponse* response,
    const IPEndPoint& recv_addr,
    int bytes_read) {
  // TODO(noamsml): More sophisticated error handling.
  DCHECK_GT(bytes_read, 0);
  delegate_->HandlePacket(response, bytes_read);
}

MDnsClientImpl::Core::Core(base::Clock* clock, base::OneShotTimer* timer)
    : clock_(clock),
      cleanup_timer_(timer),
      connection_(new MDnsConnection(this)) {
  DCHECK(cleanup_timer_);
  DCHECK(!cleanup_timer_->IsRunning());
}

MDnsClientImpl::Core::~Core() {
  cleanup_timer_->Stop();
}

int MDnsClientImpl::Core::Init(MDnsSocketFactory* socket_factory) {
  CHECK(!cleanup_timer_->IsRunning());
  return connection_->Init(socket_factory);
}

bool MDnsClientImpl::Core::SendQuery(uint16_t rrtype, const std::string& name) {
  std::string name_dns;
  if (!DNSDomainFromUnrestrictedDot(name, &name_dns))
    return false;

  DnsQuery query(0, name_dns, rrtype);
  query.set_flags(0);  // Remove the RD flag from the query. It is unneeded.

  connection_->Send(query.io_buffer(), query.io_buffer()->size());
  return true;
}

void MDnsClientImpl::Core::HandlePacket(DnsResponse* response,
                                        int bytes_read) {
  unsigned offset;
  // Note: We store cache keys rather than record pointers to avoid
  // erroneous behavior in case a packet contains multiple exclusive
  // records with the same type and name.
  std::map<MDnsCache::Key, MDnsCache::UpdateType> update_keys;
  DCHECK_GT(bytes_read, 0);
  if (!response->InitParseWithoutQuery(bytes_read)) {
    DVLOG(1) << "Could not understand an mDNS packet.";
    return;  // Message is unreadable.
  }

  // TODO(noamsml): duplicate query suppression.
  if (!(response->flags() & dns_protocol::kFlagResponse))
    return;  // Message is a query. ignore it.

  DnsRecordParser parser = response->Parser();
  unsigned answer_count = response->answer_count() +
      response->additional_answer_count();

  for (unsigned i = 0; i < answer_count; i++) {
    offset = parser.GetOffset();
    std::unique_ptr<const RecordParsed> record =
        RecordParsed::CreateFrom(&parser, clock_->Now());

    if (!record) {
      DVLOG(1) << "Could not understand an mDNS record.";

      if (offset == parser.GetOffset()) {
        DVLOG(1) << "Abandoned parsing the rest of the packet.";
        return;  // The parser did not advance, abort reading the packet.
      } else {
        continue;  // We may be able to extract other records from the packet.
      }
    }

    if ((record->klass() & dns_protocol::kMDnsClassMask) !=
        dns_protocol::kClassIN) {
      DVLOG(1) << "Received an mDNS record with non-IN class. Ignoring.";
      continue;  // Ignore all records not in the IN class.
    }

    MDnsCache::Key update_key = MDnsCache::Key::CreateFor(record.get());
    MDnsCache::UpdateType update = cache_.UpdateDnsRecord(std::move(record));

    // Cleanup time may have changed.
    ScheduleCleanup(cache_.next_expiration());

    update_keys.insert(std::make_pair(update_key, update));
  }

  for (auto i = update_keys.begin(); i != update_keys.end(); i++) {
    const RecordParsed* record = cache_.LookupKey(i->first);
    if (!record)
      continue;

    if (record->type() == dns_protocol::kTypeNSEC) {
#if defined(ENABLE_NSEC)
      NotifyNsecRecord(record);
#endif
    } else {
      AlertListeners(i->second, ListenerKey(record->name(), record->type()),
                     record);
    }
  }
}

void MDnsClientImpl::Core::NotifyNsecRecord(const RecordParsed* record) {
  DCHECK_EQ(dns_protocol::kTypeNSEC, record->type());
  const NsecRecordRdata* rdata = record->rdata<NsecRecordRdata>();
  DCHECK(rdata);

  // Remove all cached records matching the nonexistent RR types.
  std::vector<const RecordParsed*> records_to_remove;

  cache_.FindDnsRecords(0, record->name(), &records_to_remove, clock_->Now());

  for (auto i = records_to_remove.begin(); i != records_to_remove.end(); i++) {
    if ((*i)->type() == dns_protocol::kTypeNSEC)
      continue;
    if (!rdata->GetBit((*i)->type())) {
      std::unique_ptr<const RecordParsed> record_removed =
          cache_.RemoveRecord((*i));
      DCHECK(record_removed);
      OnRecordRemoved(record_removed.get());
    }
  }

  // Alert all listeners waiting for the nonexistent RR types.
  auto i = listeners_.upper_bound(ListenerKey(record->name(), 0));
  for (; i != listeners_.end() && i->first.first == record->name(); i++) {
    if (!rdata->GetBit(i->first.second)) {
      for (auto& observer : *i->second)
        observer.AlertNsecRecord();
    }
  }
}

void MDnsClientImpl::Core::OnConnectionError(int error) {
  // TODO(noamsml): On connection error, recreate connection and flush cache.
  VLOG(1) << "MDNS OnConnectionError (code: " << error << ")";
}

void MDnsClientImpl::Core::AlertListeners(
    MDnsCache::UpdateType update_type,
    const ListenerKey& key,
    const RecordParsed* record) {
  auto listener_map_iterator = listeners_.find(key);
  if (listener_map_iterator == listeners_.end()) return;

  for (auto& observer : *listener_map_iterator->second)
    observer.HandleRecordUpdate(update_type, record);
}

void MDnsClientImpl::Core::AddListener(
    MDnsListenerImpl* listener) {
  ListenerKey key(listener->GetName(), listener->GetType());

  auto& observer_list = listeners_[key];
  if (!observer_list)
    observer_list = std::make_unique<ObserverListType>();

  observer_list->AddObserver(listener);
}

void MDnsClientImpl::Core::RemoveListener(MDnsListenerImpl* listener) {
  ListenerKey key(listener->GetName(), listener->GetType());
  auto observer_list_iterator = listeners_.find(key);

  DCHECK(observer_list_iterator != listeners_.end());
  DCHECK(observer_list_iterator->second->HasObserver(listener));

  observer_list_iterator->second->RemoveObserver(listener);

  // Remove the observer list from the map if it is empty
  if (!observer_list_iterator->second->might_have_observers()) {
    // Schedule the actual removal for later in case the listener removal
    // happens while iterating over the observer list.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&MDnsClientImpl::Core::CleanupObserverList,
                                  AsWeakPtr(), key));
  }
}

void MDnsClientImpl::Core::CleanupObserverList(const ListenerKey& key) {
  auto found = listeners_.find(key);
  if (found != listeners_.end() && !found->second->might_have_observers()) {
    listeners_.erase(found);
  }
}

void MDnsClientImpl::Core::ScheduleCleanup(base::Time cleanup) {
  // If cache is overfilled. Force an immediate cleanup.
  if (cache_.IsCacheOverfilled())
    cleanup = clock_->Now();

  // Cleanup is already scheduled, no need to do anything.
  if (cleanup == scheduled_cleanup_) {
    return;
  }
  scheduled_cleanup_ = cleanup;

  // This cancels the previously scheduled cleanup.
  cleanup_timer_->Stop();

  // If |cleanup| is empty, then no cleanup necessary.
  if (cleanup != base::Time()) {
    cleanup_timer_->Start(FROM_HERE,
                          std::max(base::TimeDelta(), cleanup - clock_->Now()),
                          base::BindOnce(&MDnsClientImpl::Core::DoCleanup,
                                         base::Unretained(this)));
  }
}

void MDnsClientImpl::Core::DoCleanup() {
  cache_.CleanupRecords(clock_->Now(),
                        base::Bind(&MDnsClientImpl::Core::OnRecordRemoved,
                                   base::Unretained(this)));

  ScheduleCleanup(cache_.next_expiration());
}

void MDnsClientImpl::Core::OnRecordRemoved(
    const RecordParsed* record) {
  AlertListeners(MDnsCache::RecordRemoved,
                 ListenerKey(record->name(), record->type()), record);
}

void MDnsClientImpl::Core::QueryCache(
    uint16_t rrtype,
    const std::string& name,
    std::vector<const RecordParsed*>* records) const {
  cache_.FindDnsRecords(rrtype, name, records, clock_->Now());
}

MDnsClientImpl::MDnsClientImpl()
    : clock_(base::DefaultClock::GetInstance()),
      cleanup_timer_(new base::OneShotTimer()) {}

MDnsClientImpl::MDnsClientImpl(base::Clock* clock,
                               std::unique_ptr<base::OneShotTimer> timer)
    : clock_(clock), cleanup_timer_(std::move(timer)) {}

MDnsClientImpl::~MDnsClientImpl() {
  StopListening();
}

int MDnsClientImpl::StartListening(MDnsSocketFactory* socket_factory) {
  DCHECK(!core_.get());
  core_.reset(new Core(clock_, cleanup_timer_.get()));
  int rv = core_->Init(socket_factory);
  if (rv != OK) {
    DCHECK_NE(ERR_IO_PENDING, rv);
    core_.reset();
  }
  return rv;
}

void MDnsClientImpl::StopListening() {
  core_.reset();
}

bool MDnsClientImpl::IsListening() const {
  return core_.get() != nullptr;
}

std::unique_ptr<MDnsListener> MDnsClientImpl::CreateListener(
    uint16_t rrtype,
    const std::string& name,
    MDnsListener::Delegate* delegate) {
  return std::unique_ptr<MDnsListener>(
      new MDnsListenerImpl(rrtype, name, clock_, delegate, this));
}

std::unique_ptr<MDnsTransaction> MDnsClientImpl::CreateTransaction(
    uint16_t rrtype,
    const std::string& name,
    int flags,
    const MDnsTransaction::ResultCallback& callback) {
  return std::unique_ptr<MDnsTransaction>(
      new MDnsTransactionImpl(rrtype, name, flags, callback, this));
}

MDnsListenerImpl::MDnsListenerImpl(uint16_t rrtype,
                                   const std::string& name,
                                   base::Clock* clock,
                                   MDnsListener::Delegate* delegate,
                                   MDnsClientImpl* client)
    : rrtype_(rrtype),
      name_(name),
      clock_(clock),
      client_(client),
      delegate_(delegate),
      started_(false),
      active_refresh_(false) {}

MDnsListenerImpl::~MDnsListenerImpl() {
  if (started_) {
    DCHECK(client_->core());
    client_->core()->RemoveListener(this);
  }
}

bool MDnsListenerImpl::Start() {
  DCHECK(!started_);

  started_ = true;

  DCHECK(client_->core());
  client_->core()->AddListener(this);

  return true;
}

void MDnsListenerImpl::SetActiveRefresh(bool active_refresh) {
  active_refresh_ = active_refresh;

  if (started_) {
    if (!active_refresh_) {
      next_refresh_.Cancel();
    } else if (last_update_ != base::Time()) {
      ScheduleNextRefresh();
    }
  }
}

const std::string& MDnsListenerImpl::GetName() const {
  return name_;
}

uint16_t MDnsListenerImpl::GetType() const {
  return rrtype_;
}

void MDnsListenerImpl::HandleRecordUpdate(MDnsCache::UpdateType update_type,
                                          const RecordParsed* record) {
  DCHECK(started_);

  if (update_type != MDnsCache::RecordRemoved) {
    ttl_ = record->ttl();
    last_update_ = record->time_created();

    ScheduleNextRefresh();
  }

  if (update_type != MDnsCache::NoChange) {
    MDnsListener::UpdateType update_external;

    switch (update_type) {
      case MDnsCache::RecordAdded:
        update_external = MDnsListener::RECORD_ADDED;
        break;
      case MDnsCache::RecordChanged:
        update_external = MDnsListener::RECORD_CHANGED;
        break;
      case MDnsCache::RecordRemoved:
        update_external = MDnsListener::RECORD_REMOVED;
        break;
      case MDnsCache::NoChange:
      default:
        NOTREACHED();
        // Dummy assignment to suppress compiler warning.
        update_external = MDnsListener::RECORD_CHANGED;
        break;
    }

    delegate_->OnRecordUpdate(update_external, record);
  }
}

void MDnsListenerImpl::AlertNsecRecord() {
  DCHECK(started_);
  delegate_->OnNsecRecord(name_, rrtype_);
}

void MDnsListenerImpl::ScheduleNextRefresh() {
  DCHECK(last_update_ != base::Time());

  if (!active_refresh_)
    return;

  // A zero TTL is a goodbye packet and should not be refreshed.
  if (ttl_ == 0) {
    next_refresh_.Cancel();
    return;
  }

  next_refresh_.Reset(base::Bind(&MDnsListenerImpl::DoRefresh,
                                 AsWeakPtr()));

  // Schedule refreshes at both 85% and 95% of the original TTL. These will both
  // be canceled and rescheduled if the record's TTL is updated due to a
  // response being received.
  base::Time next_refresh1 = last_update_ + base::TimeDelta::FromMilliseconds(
      static_cast<int>(base::Time::kMillisecondsPerSecond *
                       kListenerRefreshRatio1 * ttl_));

  base::Time next_refresh2 = last_update_ + base::TimeDelta::FromMilliseconds(
      static_cast<int>(base::Time::kMillisecondsPerSecond *
                       kListenerRefreshRatio2 * ttl_));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, next_refresh_.callback(), next_refresh1 - clock_->Now());

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, next_refresh_.callback(), next_refresh2 - clock_->Now());
}

void MDnsListenerImpl::DoRefresh() {
  client_->core()->SendQuery(rrtype_, name_);
}

MDnsTransactionImpl::MDnsTransactionImpl(
    uint16_t rrtype,
    const std::string& name,
    int flags,
    const MDnsTransaction::ResultCallback& callback,
    MDnsClientImpl* client)
    : rrtype_(rrtype),
      name_(name),
      callback_(callback),
      client_(client),
      started_(false),
      flags_(flags) {
  DCHECK((flags_ & MDnsTransaction::FLAG_MASK) == flags_);
  DCHECK(flags_ & MDnsTransaction::QUERY_CACHE ||
         flags_ & MDnsTransaction::QUERY_NETWORK);
}

MDnsTransactionImpl::~MDnsTransactionImpl() {
  timeout_.Cancel();
}

bool MDnsTransactionImpl::Start() {
  DCHECK(!started_);
  started_ = true;

  base::WeakPtr<MDnsTransactionImpl> weak_this = AsWeakPtr();
  if (flags_ & MDnsTransaction::QUERY_CACHE) {
    ServeRecordsFromCache();

    if (!weak_this || !is_active()) return true;
  }

  if (flags_ & MDnsTransaction::QUERY_NETWORK) {
    return QueryAndListen();
  }

  // If this is a cache only query, signal that the transaction is over
  // immediately.
  SignalTransactionOver();
  return true;
}

const std::string& MDnsTransactionImpl::GetName() const {
  return name_;
}

uint16_t MDnsTransactionImpl::GetType() const {
  return rrtype_;
}

void MDnsTransactionImpl::CacheRecordFound(const RecordParsed* record) {
  DCHECK(started_);
  OnRecordUpdate(MDnsListener::RECORD_ADDED, record);
}

void MDnsTransactionImpl::TriggerCallback(MDnsTransaction::Result result,
                                          const RecordParsed* record) {
  DCHECK(started_);
  if (!is_active()) return;

  // Ensure callback is run after touching all class state, so that
  // the callback can delete the transaction.
  MDnsTransaction::ResultCallback callback = callback_;

  // Reset the transaction if it expects a single result, or if the result
  // is a final one (everything except for a record).
  if (flags_ & MDnsTransaction::SINGLE_RESULT ||
      result != MDnsTransaction::RESULT_RECORD) {
    Reset();
  }

  callback.Run(result, record);
}

void MDnsTransactionImpl::Reset() {
  callback_.Reset();
  listener_.reset();
  timeout_.Cancel();
}

void MDnsTransactionImpl::OnRecordUpdate(MDnsListener::UpdateType update,
                                         const RecordParsed* record) {
  DCHECK(started_);
  if (update ==  MDnsListener::RECORD_ADDED ||
      update == MDnsListener::RECORD_CHANGED)
    TriggerCallback(MDnsTransaction::RESULT_RECORD, record);
}

void MDnsTransactionImpl::SignalTransactionOver() {
  DCHECK(started_);
  if (flags_ & MDnsTransaction::SINGLE_RESULT) {
    TriggerCallback(MDnsTransaction::RESULT_NO_RESULTS, nullptr);
  } else {
    TriggerCallback(MDnsTransaction::RESULT_DONE, nullptr);
  }
}

void MDnsTransactionImpl::ServeRecordsFromCache() {
  std::vector<const RecordParsed*> records;
  base::WeakPtr<MDnsTransactionImpl> weak_this = AsWeakPtr();

  if (client_->core()) {
    client_->core()->QueryCache(rrtype_, name_, &records);
    for (auto i = records.begin(); i != records.end() && weak_this; ++i) {
      weak_this->TriggerCallback(MDnsTransaction::RESULT_RECORD, *i);
    }

#if defined(ENABLE_NSEC)
    if (records.empty()) {
      DCHECK(weak_this);
      client_->core()->QueryCache(dns_protocol::kTypeNSEC, name_, &records);
      if (!records.empty()) {
        const NsecRecordRdata* rdata =
            records.front()->rdata<NsecRecordRdata>();
        DCHECK(rdata);
        if (!rdata->GetBit(rrtype_))
          weak_this->TriggerCallback(MDnsTransaction::RESULT_NSEC, nullptr);
      }
    }
#endif
  }
}

bool MDnsTransactionImpl::QueryAndListen() {
  listener_ = client_->CreateListener(rrtype_, name_, this);
  if (!listener_->Start())
    return false;

  DCHECK(client_->core());
  if (!client_->core()->SendQuery(rrtype_, name_))
    return false;

  timeout_.Reset(base::Bind(&MDnsTransactionImpl::SignalTransactionOver,
                            AsWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, timeout_.callback(), kTransactionTimeout);

  return true;
}

void MDnsTransactionImpl::OnNsecRecord(const std::string& name, unsigned type) {
  TriggerCallback(RESULT_NSEC, nullptr);
}

void MDnsTransactionImpl::OnCachePurged() {
  // TODO(noamsml): Cache purge situations not yet implemented
}

}  // namespace net
