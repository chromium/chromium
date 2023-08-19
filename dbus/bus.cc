// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/bus.h"

#include <stddef.h>

#include <memory>

#include "base/containers/contains.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "dbus/error.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/scoped_dbus_error.h"

namespace dbus {

namespace {

const char kDisconnectedSignal[] = "Disconnected";
const char kDisconnectedMatchRule[] =
    "type='signal', path='/org/freedesktop/DBus/Local',"
    "interface='org.freedesktop.DBus.Local', member='Disconnected'";

// The NameOwnerChanged member in org.freedesktop.DBus
const char kNameOwnerChangedSignal[] = "NameOwnerChanged";

// The match rule used to filter for changes to a given service name owner.
const char kServiceNameOwnerChangeMatchRule[] =
    "type='signal',interface='org.freedesktop.DBus',"
    "member='NameOwnerChanged',path='/org/freedesktop/DBus',"
    "sender='org.freedesktop.DBus',arg0='%s'";

// The class is used for watching the file descriptor used for D-Bus
// communication.
class Watch {
 public:
  explicit Watch(DBusWatch* watch) : raw_watch_(watch) {
    dbus_watch_set_data(raw_watch_, this, nullptr);
  }

  Watch(const Watch&) = delete;
  Watch& operator=(const Watch&) = delete;

  ~Watch() { dbus_watch_set_data(raw_watch_, nullptr, nullptr); }

  // Returns true if the underlying file descriptor is ready to be watched.
  bool IsReadyToBeWatched() {
    return dbus_watch_get_enabled(raw_watch_);
  }

  // Starts watching the underlying file descriptor.
  void StartWatching() {
    const int file_descriptor = dbus_watch_get_unix_fd(raw_watch_);
    const unsigned int flags = dbus_watch_get_flags(raw_watch_);

    // Using base::Unretained(this) is safe because watches are automatically
    // canceled when |read_watcher_| and |write_watcher_| are destroyed.
    if (flags & DBUS_WATCH_READABLE) {
      read_watcher_ = base::FileDescriptorWatcher::WatchReadable(
          file_descriptor,
          base::BindRepeating(&Watch::OnFileReady, base::Unretained(this),
                              DBUS_WATCH_READABLE));
    }
    if (flags & DBUS_WATCH_WRITABLE) {
      write_watcher_ = base::FileDescriptorWatcher::WatchWritable(
          file_descriptor,
          base::BindRepeating(&Watch::OnFileReady, base::Unretained(this),
                              DBUS_WATCH_WRITABLE));
    }
  }

  // Stops watching the underlying file descriptor.
  void StopWatching() {
    read_watcher_.reset();
    write_watcher_.reset();
  }

 private:
  void OnFileReady(unsigned int flags) {
    CHECK(dbus_watch_handle(raw_watch_, flags)) << "Unable to allocate memory";
  }

  raw_ptr<DBusWatch> raw_watch_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> read_watcher_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> write_watcher_;
};

// The class is used for monitoring the timeout used for D-Bus method
// calls.
class Timeout {
 public:
  explicit Timeout(DBusTimeout* timeout) : raw_timeout_(timeout) {
    // Associated |this| with the underlying DBusTimeout.
    dbus_timeout_set_data(raw_timeout_, this, nullptr);
  }

  Timeout(const Timeout&) = delete;
  Timeout& operator=(const Timeout&) = delete;

  ~Timeout() {
    // Remove the association between |this| and the |raw_timeout_|.
    dbus_timeout_set_data(raw_timeout_, nullptr, nullptr);
  }

  // Returns true if the timeout is ready to be monitored.
  bool IsReadyToBeMonitored() {
    return dbus_timeout_get_enabled(raw_timeout_);
  }

  // Starts monitoring the timeout.
  void StartMonitoring(Bus* bus) {
    bus->GetDBusTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&Timeout::HandleTimeout, weak_ptr_factory_.GetWeakPtr()),
        GetInterval());
  }

  // Stops monitoring the timeout.
  void StopMonitoring() { weak_ptr_factory_.InvalidateWeakPtrs(); }

  base::TimeDelta GetInterval() {
    return base::Milliseconds(dbus_timeout_get_interval(raw_timeout_));
  }

 private:
  // Calls DBus to handle the timeout.
  void HandleTimeout() { CHECK(dbus_timeout_handle(raw_timeout_)); }

  raw_ptr<DBusTimeout> raw_timeout_;

  base::WeakPtrFactory<Timeout> weak_ptr_factory_{this};
};

// Converts DBusError into dbus::Error.
Error ToError(const internal::ScopedDBusError& error) {
  return error.is_set() ? Error(error.name(), error.message()) : Error();
}

}  // namespace

Bus::Options::Options()
  : bus_type(SESSION),
    connection_type(PRIVATE) {
}

Bus::Options::~Options() = default;

Bus::Options::Options(Bus::Options&&) = default;

Bus::Options& Bus::Options::operator=(Bus::Options&&) = default;

Bus::Bus(const Options& options)
    : bus_type_(options.bus_type),
      connection_type_(options.connection_type),
      dbus_task_runner_(options.dbus_task_runner),
      on_shutdown_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      connection_(nullptr),
      origin_thread_id_(base::PlatformThread::CurrentId()),
      async_operations_set_up_(false),
      shutdown_completed_(false),
      num_pending_watches_(0),
      num_pending_timeouts_(0),
      address_(options.address) {
  // This is safe to call multiple times.
  dbus_threads_init_default();
  // The origin message loop is unnecessary if the client uses synchronous
  // functions only.
  if (base::SequencedTaskRunner::HasCurrentDefault())
    origin_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
}

Bus::~Bus() {
  DCHECK(!connection_);
  DCHECK(owned_service_names_.empty());
  DCHECK(match_rules_added_.empty());
  DCHECK(filter_functions_added_.empty());
  DCHECK(registered_object_paths_.empty());
  DCHECK_EQ(0, num_pending_watches_);
  // TODO(satorux): This check fails occasionally in browser_tests for tests
  // that run very quickly. Perhaps something does not have time to clean up.
  // Despite the check failing, the tests seem to run fine. crosbug.com/23416
  // DCHECK_EQ(0, num_pending_timeouts_);
}

ObjectProxy* Bus::GetObjectProxy(const std::string& service_name,
                                 const ObjectPath& object_path) {
  return GetObjectProxyWithOptions(service_name, object_path,
                                   ObjectProxy::DEFAULT_OPTIONS);
}

ObjectProxy* Bus::GetObjectProxyWithOptions(const std::string& service_name,
                                            const ObjectPath& object_path,
                                            int options) {
  AssertOnOriginThread();

  // Check if we already have the requested object proxy.
  const ObjectProxyTable::key_type key(service_name + object_path.value(),
                                       options);
  ObjectProxyTable::iterator iter = object_proxy_table_.find(key);
  if (iter != object_proxy_table_.end()) {
    return iter->second.get();
  }

  scoped_refptr<ObjectProxy> object_proxy =
      new ObjectProxy(this, service_name, object_path, options);
  object_proxy_table_[key] = object_proxy;

  return object_proxy.get();
}

bool Bus::RemoveObjectProxy(const std::string& service_name,
                            const ObjectPath& object_path,
                            base::OnceClosure callback) {
  return RemoveObjectProxyWithOptions(service_name, object_path,
                                      ObjectProxy::DEFAULT_OPTIONS,
                                      std::move(callback));
}

bool Bus::RemoveObjectProxyWithOptions(const std::string& service_name,
                                       const ObjectPath& object_path,
                                       int options,
                                       base::OnceClosure callback) {
  AssertOnOriginThread();

  // Check if we have the requested object proxy.
  const ObjectProxyTable::key_type key(service_name + object_path.value(),
                                       options);
  ObjectProxyTable::iterator iter = object_proxy_table_.find(key);
  if (iter != object_proxy_table_.end()) {
    scoped_refptr<ObjectProxy> object_proxy = iter->second;
    object_proxy_table_.erase(iter);
    // Object is present. Remove it now and Detach on the DBus thread.
    GetDBusTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&Bus::RemoveObjectProxyInternal, this,
                                  object_proxy, std::move(callback)));
    return true;
  }
  return false;
}

void Bus::RemoveObjectProxyInternal(scoped_refptr<ObjectProxy> object_proxy,
                                    base::OnceClosure callback) {
  AssertOnDBusThread();

  object_proxy->Detach();

  GetOriginTaskRunner()->PostTask(FROM_HERE, std::move(callback));
}

ExportedObject* Bus::GetExportedObject(const ObjectPath& object_path) {
  AssertOnOriginThread();

  // Check if we already have the requested exported object.
  ExportedObjectTable::iterator iter = exported_object_table_.find(object_path);
  if (iter != exported_object_table_.end()) {
    return iter->second.get();
  }

  scoped_refptr<ExportedObject> exported_object =
      new ExportedObject(this, object_path);
  exported_object_table_[object_path] = exported_object;

  return exported_object.get();
}

void Bus::UnregisterExportedObject(const ObjectPath& object_path) {
  AssertOnOriginThread();

  // Remove the registered object from the table first, to allow a new
  // GetExportedObject() call to return a new object, rather than this one.
  ExportedObjectTable::iterator iter = exported_object_table_.find(object_path);
  if (iter == exported_object_table_.end())
    return;

  scoped_refptr<ExportedObject> exported_object = iter->second;
  exported_object_table_.erase(iter);

  // Post the task to perform the final unregistration to the D-Bus thread.
  // Since the registration also happens on the D-Bus thread in
  // TryRegisterObjectPath(), and the task runner we post to is a
  // SequencedTaskRunner, there is a guarantee that this will happen before any
  // future registration call.
  GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&Bus::UnregisterExportedObjectInternal, this,
                                exported_object));
}

void Bus::UnregisterExportedObjectInternal(
    scoped_refptr<ExportedObject> exported_object) {
  AssertOnDBusThread();

  exported_object->Unregister();
}

ObjectManager* Bus::GetObjectManager(const std::string& service_name,
                                     const ObjectPath& object_path) {
  AssertOnOriginThread();

  // Check if we already have the requested object manager.
  const ObjectManagerTable::key_type key(service_name + object_path.value());
  ObjectManagerTable::iterator iter = object_manager_table_.find(key);
  if (iter != object_manager_table_.end()) {
    return iter->second.get();
  }

  scoped_refptr<ObjectManager> object_manager =
      ObjectManager::Create(this, service_name, object_path);
  object_manager_table_[key] = object_manager;

  return object_manager.get();
}

bool Bus::RemoveObjectManager(const std::string& service_name,
                              const ObjectPath& object_path,
                              base::OnceClosure callback) {
  AssertOnOriginThread();
  DCHECK(!callback.is_null());

  const ObjectManagerTable::key_type key(service_name + object_path.value());
  ObjectManagerTable::iterator iter = object_manager_table_.find(key);
  if (iter == object_manager_table_.end())
    return false;

  // ObjectManager is present. Remove it now and CleanUp on the DBus thread.
  scoped_refptr<ObjectManager> object_manager = iter->second;
  object_manager_table_.erase(iter);

  GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&Bus::RemoveObjectManagerInternal, this,
                                object_manager, std::move(callback)));

  return true;
}

void Bus::RemoveObjectManagerInternal(
    scoped_refptr<dbus::ObjectManager> object_manager,
    base::OnceClosure callback) {
  AssertOnDBusThread();
  DCHECK(object_manager.get());

  object_manager->CleanUp();

  // The ObjectManager has to be deleted on the origin thread since it was
  // created there.
  GetOriginTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&Bus::RemoveObjectManagerInternalHelper, this,
                                object_manager, std::move(callback)));
}

void Bus::RemoveObjectManagerInternalHelper(
    scoped_refptr<dbus::ObjectManager> object_manager,
    base::OnceClosure callback) {
  AssertOnOriginThread();
  DCHECK(object_manager);

  // Release the object manager and run the callback.
  object_manager = nullptr;
  std::move(callback).Run();
}

bool Bus::Connect() {
  // dbus_bus_get_private() and dbus_bus_get() are blocking calls.
  AssertOnDBusThread();
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Check if it's already initialized.
  if (connection_)
    return true;

  internal::ScopedDBusError dbus_error;
  if (bus_type_ == CUSTOM_ADDRESS) {
    if (connection_type_ == PRIVATE) {
      connection_ =
          dbus_connection_open_private(address_.c_str(), dbus_error.get());
    } else {
      connection_ = dbus_connection_open(address_.c_str(), dbus_error.get());
    }
  } else {
    const DBusBusType dbus_bus_type = static_cast<DBusBusType>(bus_type_);
    if (connection_type_ == PRIVATE) {
      connection_ = dbus_bus_get_private(dbus_bus_type, dbus_error.get());
    } else {
      connection_ = dbus_bus_get(dbus_bus_type, dbus_error.get());
    }
  }
  if (!connection_) {
    LOG(ERROR) << "Failed to connect to the bus: "
               << (dbus_error.is_set() ? dbus_error.message() : "");
    return false;
  }

  if (bus_type_ == CUSTOM_ADDRESS) {
    // We should call dbus_bus_register here, otherwise unique name can not be
    // acquired. According to dbus specification, it is responsible to call
    // org.freedesktop.DBus.Hello method at the beging of bus connection to
    // acquire unique name. In the case of dbus_bus_get, dbus_bus_register is
    // called internally.
    if (!dbus_bus_register(connection_, dbus_error.get())) {
      LOG(ERROR) << "Failed to register the bus component: "
                 << (dbus_error.is_set() ? dbus_error.message() : "");
      return false;
    }
  }
  // We shouldn't exit on the disconnected signal.
  dbus_connection_set_exit_on_disconnect(connection_, false);

  // Watch Disconnected signal.
  AddFilterFunction(Bus::OnConnectionDisconnectedFilter, this);
  Error error;
  AddMatch(kDisconnectedMatchRule, &error);

  return true;
}

void Bus::ClosePrivateConnection() {
  // dbus_connection_close is blocking call.
  AssertOnDBusThread();
  DCHECK_EQ(PRIVATE, connection_type_)
      << "non-private connection should not be closed";
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  dbus_connection_close(connection_);
}

void Bus::ShutdownAndBlock() {
  AssertOnDBusThread();

  if (shutdown_completed_)
    return;  // Already shutdowned, just return.

  // Unregister the exported objects.
  for (ExportedObjectTable::iterator iter = exported_object_table_.begin();
       iter != exported_object_table_.end(); ++iter) {
    iter->second->Unregister();
  }

  // Release all service names.
  for (std::set<std::string>::iterator iter = owned_service_names_.begin();
       iter != owned_service_names_.end();) {
    // This is a bit tricky but we should increment the iter here as
    // ReleaseOwnership() may remove |service_name| from the set.
    const std::string& service_name = *iter++;
    ReleaseOwnership(service_name);
  }
  if (!owned_service_names_.empty()) {
    LOG(ERROR) << "Failed to release all service names. # of services left: "
               << owned_service_names_.size();
  }

  // Detach from the remote objects.
  for (ObjectProxyTable::iterator iter = object_proxy_table_.begin();
       iter != object_proxy_table_.end(); ++iter) {
    iter->second->Detach();
  }

  // Clean up the object managers.
  for (ObjectManagerTable::iterator iter = object_manager_table_.begin();
       iter != object_manager_table_.end(); ++iter) {
    iter->second->CleanUp();
  }

  // Release object proxies and exported objects here. We should do this
  // here rather than in the destructor to avoid memory leaks due to
  // cyclic references.
  object_proxy_table_.clear();
  exported_object_table_.clear();

  // Private connection should be closed.
  if (connection_) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    // Remove Disconnected watcher.
    Error error;
    RemoveFilterFunction(Bus::OnConnectionDisconnectedFilter, this);
    RemoveMatch(kDisconnectedMatchRule, &error);

    if (connection_type_ == PRIVATE)
      ClosePrivateConnection();
    // dbus_connection_close() won't unref.
    dbus_connection_unref(connection_);
  }

  connection_ = nullptr;
  shutdown_completed_ = true;
}

void Bus::ShutdownOnDBusThreadAndBlock() {
  AssertOnOriginThread();
  DCHECK(dbus_task_runner_);

  GetDBusTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Bus::ShutdownOnDBusThreadAndBlockInternal, this));

  // http://crbug.com/125222
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;

  // Wait until the shutdown is complete on the D-Bus thread.
  // The shutdown should not hang, but set timeout just in case.
  const int kTimeoutSecs = 3;
  const base::TimeDelta timeout(base::Seconds(kTimeoutSecs));
  const bool signaled = on_shutdown_.TimedWait(timeout);
  LOG_IF(ERROR, !signaled) << "Failed to shutdown the bus";
}

void Bus::RequestOwnership(const std::string& service_name,
                           ServiceOwnershipOptions options,
                           OnOwnershipCallback on_ownership_callback) {
  AssertOnOriginThread();

  GetDBusTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&Bus::RequestOwnershipInternal, this, service_name,
                     options, std::move(on_ownership_callback)));
}

void Bus::RequestOwnershipInternal(const std::string& service_name,
                                   ServiceOwnershipOptions options,
                                   OnOwnershipCallback on_ownership_callback) {
  AssertOnDBusThread();

  bool success = Connect();
  if (success)
    success = RequestOwnershipAndBlock(service_name, options);

  GetOriginTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(on_ownership_callback), service_name, success));
}

bool Bus::RequestOwnershipAndBlock(const std::string& service_name,
                                   ServiceOwnershipOptions options) {
  DCHECK(connection_);
  // dbus_bus_request_name() is a blocking call.
  AssertOnDBusThread();

  // Check if we already own the service name.
  if (base::Contains(owned_service_names_, service_name)) {
    return true;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  internal::ScopedDBusError error;
  const int result = dbus_bus_request_name(connection_,
                                           service_name.c_str(),
                                           options,
                                           error.get());
  if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    LOG(ERROR) << "Failed to get the ownership of " << service_name << ": "
               << (error.is_set() ? error.message() : "");
    return false;
  }
  owned_service_names_.insert(service_name);
  return true;
}

bool Bus::ReleaseOwnership(const std::string& service_name) {
  DCHECK(connection_);
  // dbus_bus_release_name() is a blocking call.
  AssertOnDBusThread();

  // Check if we already own the service name.
  std::set<std::string>::iterator found =
      owned_service_names_.find(service_name);
  if (found == owned_service_names_.end()) {
    LOG(ERROR) << service_name << " is not owned by the bus";
    return false;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  internal::ScopedDBusError error;
  const int result = dbus_bus_release_name(connection_, service_name.c_str(),
                                           error.get());
  if (result == DBUS_RELEASE_NAME_REPLY_RELEASED) {
    owned_service_names_.erase(found);
    return true;
  } else {
    LOG(ERROR) << "Failed to release the ownership of " << service_name << ": "
               << (error.is_set() ? error.message() : "")
               << ", result code: " << result;
    return false;
  }
}

bool Bus::SetUpAsyncOperations() {
  DCHECK(connection_);
  AssertOnDBusThread();

  if (async_operations_set_up_)
    return true;

  // Process all the incoming data if any, so that OnDispatchStatus() will
  // be called when the incoming data is ready.
  ProcessAllIncomingDataIfAny();

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  bool success = dbus_connection_set_watch_functions(
      connection_, &Bus::OnAddWatchThunk, &Bus::OnRemoveWatchThunk,
      &Bus::OnToggleWatchThunk, this, nullptr);
  CHECK(success) << "Unable to allocate memory";

  success = dbus_connection_set_timeout_functions(
      connection_, &Bus::OnAddTimeoutThunk, &Bus::OnRemoveTimeoutThunk,
      &Bus::OnToggleTimeoutThunk, this, nullptr);
  CHECK(success) << "Unable to allocate memory";

  dbus_connection_set_dispatch_status_function(
      connection_, &Bus::OnDispatchStatusChangedThunk, this, nullptr);

  async_operations_set_up_ = true;

  return true;
}

base::expected<std::unique_ptr<Response>, Error> Bus::SendWithReplyAndBlock(
    DBusMessage* request,
    int timeout_ms) {
  DCHECK(connection_);
  AssertOnDBusThread();

  base::ElapsedTimer elapsed;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  internal::ScopedDBusError dbus_error;
  DBusMessage* reply = dbus_connection_send_with_reply_and_block(
      connection_, request, timeout_ms, dbus_error.get());
  constexpr base::TimeDelta kLongCall = base::Seconds(1);
  LOG_IF(WARNING, elapsed.Elapsed() >= kLongCall)
      << "Bus::SendWithReplyAndBlock took "
      << elapsed.Elapsed().InMilliseconds() << "ms to process message: "
      << "type=" << dbus_message_type_to_string(dbus_message_get_type(request))
      << ", path=" << dbus_message_get_path(request)
      << ", interface=" << dbus_message_get_interface(request)
      << ", member=" << dbus_message_get_member(request);

  if (!reply) {
    return base::unexpected(ToError(dbus_error));
  }

  return base::ok(Response::FromRawMessage(reply));
}

void Bus::SendWithReply(DBusMessage* request,
                        DBusPendingCall** pending_call,
                        int timeout_ms) {
  DCHECK(connection_);
  AssertOnDBusThread();

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const bool success = dbus_connection_send_with_reply(
      connection_, request, pending_call, timeout_ms);
  CHECK(success) << "Unable to allocate memory";
}

void Bus::Send(DBusMessage* request, uint32_t* serial) {
  DCHECK(connection_);
  AssertOnDBusThread();

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const bool success = dbus_connection_send(connection_, request, serial);
  CHECK(success) << "Unable to allocate memory";
}

void Bus::AddFilterFunction(DBusHandleMessageFunction filter_function,
                            void* user_data) {
  DCHECK(connection_);
  AssertOnDBusThread();

  std::pair<DBusHandleMessageFunction, void*> filter_data_pair =
      std::make_pair(filter_function, user_data);
  if (base::Contains(filter_functions_added_, filter_data_pair)) {
    VLOG(1) << "Filter function already exists: " << filter_function
            << " with associated data: " << user_data;
    return;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const bool success = dbus_connection_add_filter(connection_, filter_function,
                                                  user_data, nullptr);
  CHECK(success) << "Unable to allocate memory";
  filter_functions_added_.insert(filter_data_pair);
}

void Bus::RemoveFilterFunction(DBusHandleMessageFunction filter_function,
                               void* user_data) {
  DCHECK(connection_);
  AssertOnDBusThread();

  std::pair<DBusHandleMessageFunction, void*> filter_data_pair =
      std::make_pair(filter_function, user_data);
  if (!base::Contains(filter_functions_added_, filter_data_pair)) {
    VLOG(1) << "Requested to remove an unknown filter function: "
            << filter_function
            << " with associated data: " << user_data;
    return;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  dbus_connection_remove_filter(connection_, filter_function, user_data);
  filter_functions_added_.erase(filter_data_pair);
}

void Bus::AddMatch(const std::string& match_rule, Error* error) {
  DCHECK(connection_);
  DCHECK(error);
  AssertOnDBusThread();

  std::map<std::string, int>::iterator iter =
      match_rules_added_.find(match_rule);
  if (iter != match_rules_added_.end()) {
    // The already existing rule's counter is incremented.
    iter->second++;

    VLOG(1) << "Match rule already exists: " << match_rule;
    return;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  internal::ScopedDBusError dbus_error;
  dbus_bus_add_match(connection_, match_rule.c_str(), dbus_error.get());
  if (dbus_error.is_set()) {
    *error = Error(dbus_error.name(), dbus_error.message());
  }
  match_rules_added_[match_rule] = 1;
}

bool Bus::RemoveMatch(const std::string& match_rule, Error* error) {
  DCHECK(connection_);
  DCHECK(error);
  AssertOnDBusThread();

  std::map<std::string, int>::iterator iter =
      match_rules_added_.find(match_rule);
  if (iter == match_rules_added_.end()) {
    LOG(ERROR) << "Requested to remove an unknown match rule: " << match_rule;
    return false;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // The rule's counter is decremented and the rule is deleted when reachs 0.
  iter->second--;
  if (iter->second == 0) {
    internal::ScopedDBusError dbus_error;
    dbus_bus_remove_match(connection_, match_rule.c_str(), dbus_error.get());
    if (dbus_error.is_set()) {
      *error = Error(dbus_error.name(), dbus_error.message());
    }
    match_rules_added_.erase(match_rule);
  }
  return true;
}

bool Bus::TryRegisterObjectPath(const ObjectPath& object_path,
                                const DBusObjectPathVTable* vtable,
                                void* user_data,
                                Error* error) {
  return TryRegisterObjectPathInternal(
      object_path, vtable, user_data, error,
      dbus_connection_try_register_object_path);
}

bool Bus::TryRegisterFallback(const ObjectPath& object_path,
                              const DBusObjectPathVTable* vtable,
                              void* user_data,
                              Error* error) {
  DCHECK(error);
  return TryRegisterObjectPathInternal(object_path, vtable, user_data, error,
                                       dbus_connection_try_register_fallback);
}

bool Bus::TryRegisterObjectPathInternal(
    const ObjectPath& object_path,
    const DBusObjectPathVTable* vtable,
    void* user_data,
    Error* error,
    TryRegisterObjectPathFunction* register_function) {
  DCHECK(connection_);
  DCHECK(error);
  AssertOnDBusThread();
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (base::Contains(registered_object_paths_, object_path)) {
    LOG(ERROR) << "Object path already registered: " << object_path.value();
    return false;
  }

  internal::ScopedDBusError dbus_error;
  const bool success =
      register_function(connection_, object_path.value().c_str(), vtable,
                        user_data, dbus_error.get());
  if (success) {
    registered_object_paths_.insert(object_path);
  } else if (dbus_error.is_set()) {
    *error = Error(dbus_error.name(), dbus_error.message());
  }
  return success;
}

void Bus::UnregisterObjectPath(const ObjectPath& object_path) {
  DCHECK(connection_);
  AssertOnDBusThread();

  if (!base::Contains(registered_object_paths_, object_path)) {
    LOG(ERROR) << "Requested to unregister an unknown object path: "
               << object_path.value();
    return;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const bool success = dbus_connection_unregister_object_path(
      connection_,
      object_path.value().c_str());
  CHECK(success) << "Unable to allocate memory";
  registered_object_paths_.erase(object_path);
}

void Bus::ShutdownOnDBusThreadAndBlockInternal() {
  AssertOnDBusThread();

  ShutdownAndBlock();
  on_shutdown_.Signal();
}

void Bus::ProcessAllIncomingDataIfAny() {
  AssertOnDBusThread();

  // As mentioned at the class comment in .h file, connection_ can be NULL.
  if (!connection_)
    return;

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // It is safe and necessary to call dbus_connection_get_dispatch_status even
  // if the connection is lost.
  if (dbus_connection_get_dispatch_status(connection_) ==
      DBUS_DISPATCH_DATA_REMAINS) {
    while (dbus_connection_dispatch(connection_) ==
           DBUS_DISPATCH_DATA_REMAINS) {
    }
  }
}

base::SequencedTaskRunner* Bus::GetDBusTaskRunner() {
  if (dbus_task_runner_)
    return dbus_task_runner_.get();
  else
    return GetOriginTaskRunner();
}

base::SequencedTaskRunner* Bus::GetOriginTaskRunner() {
  DCHECK(origin_task_runner_);
  return origin_task_runner_.get();
}

bool Bus::HasDBusThread() {
  return dbus_task_runner_ != nullptr;
}

void Bus::AssertOnOriginThread() {
  if (origin_task_runner_) {
    CHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  } else {
    CHECK_EQ(origin_thread_id_, base::PlatformThread::CurrentId());
  }
}

void Bus::AssertOnDBusThread() {
  if (dbus_task_runner_) {
    CHECK(dbus_task_runner_->RunsTasksInCurrentSequence());
  } else {
    AssertOnOriginThread();
  }
}

std::string Bus::GetServiceOwnerAndBlock(const std::string& service_name,
                                         GetServiceOwnerOption options) {
  AssertOnDBusThread();

  MethodCall get_name_owner_call("org.freedesktop.DBus", "GetNameOwner");
  MessageWriter writer(&get_name_owner_call);
  writer.AppendString(service_name);
  VLOG(1) << "Method call: " << get_name_owner_call.ToString();

  const ObjectPath obj_path("/org/freedesktop/DBus");
  if (!get_name_owner_call.SetDestination("org.freedesktop.DBus") ||
      !get_name_owner_call.SetPath(obj_path)) {
    if (options == REPORT_ERRORS)
      LOG(ERROR) << "Failed to get name owner.";
    return "";
  }

  auto result = SendWithReplyAndBlock(get_name_owner_call.raw_message(),
                                      ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!result.has_value()) {
    if (options == REPORT_ERRORS) {
      LOG(ERROR) << "Failed to get name owner. Got " << result.error().name()
                 << ": " << result.error().message();
    }
    return "";
  }

  MessageReader reader(result->get());
  std::string service_owner;
  if (!reader.PopString(&service_owner))
    service_owner.clear();
  return service_owner;
}

void Bus::GetServiceOwner(const std::string& service_name,
                          GetServiceOwnerCallback callback) {
  AssertOnOriginThread();

  GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&Bus::GetServiceOwnerInternal, this,
                                service_name, std::move(callback)));
}

void Bus::GetServiceOwnerInternal(const std::string& service_name,
                                  GetServiceOwnerCallback callback) {
  AssertOnDBusThread();

  std::string service_owner;
  if (Connect())
    service_owner = GetServiceOwnerAndBlock(service_name, SUPPRESS_ERRORS);
  GetOriginTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), service_owner));
}

void Bus::ListenForServiceOwnerChange(
    const std::string& service_name,
    const ServiceOwnerChangeCallback& callback) {
  AssertOnOriginThread();
  DCHECK(!service_name.empty());
  DCHECK(!callback.is_null());

  GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&Bus::ListenForServiceOwnerChangeInternal, this,
                                service_name, callback));
}

void Bus::ListenForServiceOwnerChangeInternal(
    const std::string& service_name,
    const ServiceOwnerChangeCallback& callback) {
  AssertOnDBusThread();
  DCHECK(!service_name.empty());
  DCHECK(!callback.is_null());

  if (!Connect() || !SetUpAsyncOperations())
    return;

  if (service_owner_changed_listener_map_.empty())
    AddFilterFunction(Bus::OnServiceOwnerChangedFilter, this);

  ServiceOwnerChangedListenerMap::iterator it =
      service_owner_changed_listener_map_.find(service_name);
  if (it == service_owner_changed_listener_map_.end()) {
    // Add a match rule for the new service name.
    const std::string name_owner_changed_match_rule =
        base::StringPrintf(kServiceNameOwnerChangeMatchRule,
                           service_name.c_str());
    dbus::Error error;
    AddMatch(name_owner_changed_match_rule, &error);
    if (error.IsValid()) {
      LOG(ERROR) << "Failed to add match rule for " << service_name
                 << ". Got " << error.name() << ": " << error.message();
      return;
    }

    service_owner_changed_listener_map_[service_name].push_back(callback);
    return;
  }

  // Check if the callback has already been added.
  std::vector<ServiceOwnerChangeCallback>& callbacks = it->second;
  for (size_t i = 0; i < callbacks.size(); ++i) {
    if (callbacks[i] == callback)
      return;
  }
  callbacks.push_back(callback);
}

void Bus::UnlistenForServiceOwnerChange(
    const std::string& service_name,
    const ServiceOwnerChangeCallback& callback) {
  AssertOnOriginThread();
  DCHECK(!service_name.empty());
  DCHECK(!callback.is_null());

  GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&Bus::UnlistenForServiceOwnerChangeInternal,
                                this, service_name, callback));
}

void Bus::UnlistenForServiceOwnerChangeInternal(
    const std::string& service_name,
    const ServiceOwnerChangeCallback& callback) {
  AssertOnDBusThread();
  DCHECK(!service_name.empty());
  DCHECK(!callback.is_null());

  ServiceOwnerChangedListenerMap::iterator it =
      service_owner_changed_listener_map_.find(service_name);
  if (it == service_owner_changed_listener_map_.end())
    return;

  std::vector<ServiceOwnerChangeCallback>& callbacks = it->second;
  for (size_t i = 0; i < callbacks.size(); ++i) {
    if (callbacks[i] == callback) {
      callbacks.erase(callbacks.begin() + i);
      break;  // There can be only one.
    }
  }
  if (!callbacks.empty())
    return;

  // Last callback for |service_name| has been removed, remove match rule.
  const std::string name_owner_changed_match_rule =
      base::StringPrintf(kServiceNameOwnerChangeMatchRule,
                         service_name.c_str());
  Error error;
  RemoveMatch(name_owner_changed_match_rule, &error);
  // And remove |service_owner_changed_lister_map_| entry.
  service_owner_changed_listener_map_.erase(it);

  if (service_owner_changed_listener_map_.empty())
    RemoveFilterFunction(Bus::OnServiceOwnerChangedFilter, this);
}

std::string Bus::GetConnectionName() {
  if (!connection_)
    return "";
  return dbus_bus_get_unique_name(connection_);
}

bool Bus::IsConnected() {
  return connection_ != nullptr;
}

dbus_bool_t Bus::OnAddWatch(DBusWatch* raw_watch) {
  AssertOnDBusThread();

  // watch will be deleted when raw_watch is removed in OnRemoveWatch().
  Watch* watch = new Watch(raw_watch);
  if (watch->IsReadyToBeWatched()) {
    watch->StartWatching();
  }
  ++num_pending_watches_;
  return true;
}

void Bus::OnRemoveWatch(DBusWatch* raw_watch) {
  AssertOnDBusThread();

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  Watch* watch = static_cast<Watch*>(dbus_watch_get_data(raw_watch));
  delete watch;
  --num_pending_watches_;
}

void Bus::OnToggleWatch(DBusWatch* raw_watch) {
  AssertOnDBusThread();

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  Watch* watch = static_cast<Watch*>(dbus_watch_get_data(raw_watch));
  if (watch->IsReadyToBeWatched())
    watch->StartWatching();
  else
    watch->StopWatching();
}

dbus_bool_t Bus::OnAddTimeout(DBusTimeout* raw_timeout) {
  AssertOnDBusThread();

  // |timeout| will be deleted by OnRemoveTimeoutThunk().
  Timeout* timeout = new Timeout(raw_timeout);
  if (timeout->IsReadyToBeMonitored()) {
    timeout->StartMonitoring(this);
  }
  ++num_pending_timeouts_;
  return true;
}

void Bus::OnRemoveTimeout(DBusTimeout* raw_timeout) {
  AssertOnDBusThread();

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  Timeout* timeout = static_cast<Timeout*>(dbus_timeout_get_data(raw_timeout));
  delete timeout;
  --num_pending_timeouts_;
}

void Bus::OnToggleTimeout(DBusTimeout* raw_timeout) {
  AssertOnDBusThread();

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  Timeout* timeout = static_cast<Timeout*>(dbus_timeout_get_data(raw_timeout));
  if (timeout->IsReadyToBeMonitored()) {
    timeout->StartMonitoring(this);
  } else {
    timeout->StopMonitoring();
  }
}

void Bus::OnDispatchStatusChanged(DBusConnection* connection,
                                  DBusDispatchStatus status) {
  DCHECK_EQ(connection, connection_);
  AssertOnDBusThread();

  // We cannot call ProcessAllIncomingDataIfAny() here, as calling
  // dbus_connection_dispatch() inside DBusDispatchStatusFunction is
  // prohibited by the D-Bus library. Hence, we post a task here instead.
  // See comments for dbus_connection_set_dispatch_status_function().
  GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&Bus::ProcessAllIncomingDataIfAny, this));
}

void Bus::OnServiceOwnerChanged(DBusMessage* message) {
  DCHECK(message);
  AssertOnDBusThread();

  // |message| will be unrefed on exit of the function. Increment the
  // reference so we can use it in Signal::FromRawMessage() below.
  dbus_message_ref(message);
  std::unique_ptr<Signal> signal(Signal::FromRawMessage(message));

  // Confirm the validity of the NameOwnerChanged signal.
  if (signal->GetMember() != kNameOwnerChangedSignal ||
      signal->GetInterface() != DBUS_INTERFACE_DBUS ||
      signal->GetSender() != DBUS_SERVICE_DBUS) {
    return;
  }

  MessageReader reader(signal.get());
  std::string service_name;
  std::string old_owner;
  std::string new_owner;
  if (!reader.PopString(&service_name) ||
      !reader.PopString(&old_owner) ||
      !reader.PopString(&new_owner)) {
    return;
  }

  ServiceOwnerChangedListenerMap::const_iterator it =
      service_owner_changed_listener_map_.find(service_name);
  if (it == service_owner_changed_listener_map_.end())
    return;

  const std::vector<ServiceOwnerChangeCallback>& callbacks = it->second;
  for (size_t i = 0; i < callbacks.size(); ++i) {
    GetOriginTaskRunner()->PostTask(FROM_HERE,
                                    base::BindOnce(callbacks[i], new_owner));
  }
}

// static
dbus_bool_t Bus::OnAddWatchThunk(DBusWatch* raw_watch, void* data) {
  Bus* self = static_cast<Bus*>(data);
  return self->OnAddWatch(raw_watch);
}

// static
void Bus::OnRemoveWatchThunk(DBusWatch* raw_watch, void* data) {
  Bus* self = static_cast<Bus*>(data);
  self->OnRemoveWatch(raw_watch);
}

// static
void Bus::OnToggleWatchThunk(DBusWatch* raw_watch, void* data) {
  Bus* self = static_cast<Bus*>(data);
  self->OnToggleWatch(raw_watch);
}

// static
dbus_bool_t Bus::OnAddTimeoutThunk(DBusTimeout* raw_timeout, void* data) {
  Bus* self = static_cast<Bus*>(data);
  return self->OnAddTimeout(raw_timeout);
}

// static
void Bus::OnRemoveTimeoutThunk(DBusTimeout* raw_timeout, void* data) {
  Bus* self = static_cast<Bus*>(data);
  self->OnRemoveTimeout(raw_timeout);
}

// static
void Bus::OnToggleTimeoutThunk(DBusTimeout* raw_timeout, void* data) {
  Bus* self = static_cast<Bus*>(data);
  self->OnToggleTimeout(raw_timeout);
}

// static
void Bus::OnDispatchStatusChangedThunk(DBusConnection* connection,
                                       DBusDispatchStatus status,
                                       void* data) {
  Bus* self = static_cast<Bus*>(data);
  self->OnDispatchStatusChanged(connection, status);
}

// static
DBusHandlerResult Bus::OnConnectionDisconnectedFilter(
    DBusConnection* connection,
    DBusMessage* message,
    void* data) {
  if (dbus_message_is_signal(message,
                             DBUS_INTERFACE_LOCAL,
                             kDisconnectedSignal)) {
    // Abort when the connection is lost.
    LOG(FATAL) << "D-Bus connection was disconnected. Aborting.";
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

// static
DBusHandlerResult Bus::OnServiceOwnerChangedFilter(
    DBusConnection* connection,
    DBusMessage* message,
    void* data) {
  if (dbus_message_is_signal(message,
                             DBUS_INTERFACE_DBUS,
                             kNameOwnerChangedSignal)) {
    Bus* self = static_cast<Bus*>(data);
    self->OnServiceOwnerChanged(message);
  }
  // Always return unhandled to let others, e.g. ObjectProxies, handle the same
  // signal.
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

}  // namespace dbus
