// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/object_manager.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "dbus/bus.h"
#include "dbus/dbus_statistics.h"
#include "dbus/error.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"
#include "dbus/util.h"

namespace dbus {

ObjectManager::Object::Object()
  : object_proxy(nullptr) {
}

ObjectManager::Object::~Object() = default;

scoped_refptr<ObjectManager> ObjectManager::Create(
    Bus* bus,
    const std::string& service_name,
    const ObjectPath& object_path) {
  auto object_manager =
      base::WrapRefCounted(new ObjectManager(bus, service_name, object_path));

  // Set up a match rule and a filter function to handle PropertiesChanged
  // signals from the service. This is important to avoid any race conditions
  // that might cause us to miss PropertiesChanged signals once all objects are
  // initialized via GetManagedObjects.
  bus->GetDBusTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ObjectManager::SetupMatchRuleAndFilter, object_manager),
      base::BindOnce(&ObjectManager::OnSetupMatchRuleAndFilterComplete,
                     object_manager));
  return object_manager;
}

ObjectManager::ObjectManager(Bus* bus,
                             const std::string& service_name,
                             const ObjectPath& object_path)
    : bus_(bus), service_name_(service_name), object_path_(object_path) {
  LOG_IF(FATAL, !object_path_.IsValid()) << object_path_.value();
  DVLOG(1) << "Creating ObjectManager for " << service_name_
           << " " << object_path_.value();
  DCHECK(bus_);
  bus_->AssertOnOriginThread();
  object_proxy_ = bus_->GetObjectProxy(service_name_, object_path_);
  object_proxy_->SetNameOwnerChangedCallback(base::BindRepeating(
      &ObjectManager::NameOwnerChanged, weak_ptr_factory_.GetWeakPtr()));
}

ObjectManager::~ObjectManager() {
  // Clean up Object structures
  for (ObjectMap::iterator iter = object_map_.begin();
       iter != object_map_.end(); ++iter) {
    Object* object = iter->second;

    for (Object::PropertiesMap::iterator piter = object->properties_map.begin();
         piter != object->properties_map.end(); ++piter) {
      PropertySet* properties = piter->second;
      delete properties;
    }

    delete object;
  }
}

void ObjectManager::RegisterInterface(const std::string& interface_name,
                                      Interface* interface) {
  interface_map_[interface_name] = interface;
}

void ObjectManager::UnregisterInterface(const std::string& interface_name) {
  InterfaceMap::iterator iter = interface_map_.find(interface_name);
  if (iter != interface_map_.end())
    interface_map_.erase(iter);
}

bool ObjectManager::IsInterfaceRegisteredForTesting(
    const std::string& interface_name) const {
  return interface_map_.count(interface_name);
}

std::vector<ObjectPath> ObjectManager::GetObjects() {
  std::vector<ObjectPath> object_paths;

  for (ObjectMap::iterator iter = object_map_.begin();
       iter != object_map_.end(); ++iter)
    object_paths.push_back(iter->first);

  return object_paths;
}

std::vector<ObjectPath> ObjectManager::GetObjectsWithInterface(
      const std::string& interface_name) {
  std::vector<ObjectPath> object_paths;

  for (ObjectMap::iterator oiter = object_map_.begin();
       oiter != object_map_.end(); ++oiter) {
    Object* object = oiter->second;

    Object::PropertiesMap::iterator piter =
        object->properties_map.find(interface_name);
    if (piter != object->properties_map.end())
      object_paths.push_back(oiter->first);
  }

  return object_paths;
}

ObjectProxy* ObjectManager::GetObjectProxy(const ObjectPath& object_path) {
  ObjectMap::iterator iter = object_map_.find(object_path);
  if (iter == object_map_.end())
    return nullptr;

  Object* object = iter->second;
  return object->object_proxy;
}

PropertySet* ObjectManager::GetProperties(const ObjectPath& object_path,
                                          const std::string& interface_name) {
  ObjectMap::iterator iter = object_map_.find(object_path);
  if (iter == object_map_.end())
    return nullptr;

  Object* object = iter->second;
  Object::PropertiesMap::iterator piter =
      object->properties_map.find(interface_name);
  if (piter == object->properties_map.end())
    return nullptr;

  return piter->second;
}

void ObjectManager::GetManagedObjects() {
  MethodCall method_call(kObjectManagerInterface,
                         kObjectManagerGetManagedObjects);

  object_proxy_->CallMethod(&method_call, ObjectProxy::TIMEOUT_USE_DEFAULT,
                            base::BindOnce(&ObjectManager::OnGetManagedObjects,
                                           weak_ptr_factory_.GetWeakPtr()));
}

void ObjectManager::CleanUp() {
  DCHECK(bus_);
  bus_->AssertOnDBusThread();
  DCHECK(!cleanup_called_);

  cleanup_called_ = true;

  if (!setup_success_)
    return;

  bus_->RemoveFilterFunction(&ObjectManager::HandleMessageThunk, this);
  Error error;
  bus_->RemoveMatch(match_rule_, &error);
  if (error.IsValid()) {
    LOG(ERROR) << "Failed to remove match rule: " << match_rule_;
  }

  match_rule_.clear();
  // After Cleanup(), the Bus doesn't own `this` anymore and might be deleted
  // before `this`.
  bus_ = nullptr;
}

bool ObjectManager::SetupMatchRuleAndFilter() {
  DCHECK(!setup_success_);

  if (cleanup_called_) {
    return false;
  }

  DCHECK(bus_);
  bus_->AssertOnDBusThread();

  if (!bus_->Connect() || !bus_->SetUpAsyncOperations())
    return false;

  // Try to get |service_name_owner_| from dbus if we haven't received any
  // NameOwnerChanged signals.
  if (service_name_owner_.empty()) {
    service_name_owner_ =
        bus_->GetServiceOwnerAndBlock(service_name_, Bus::SUPPRESS_ERRORS);
  }

  const std::string match_rule =
      base::StringPrintf(
          "type='signal', sender='%s', interface='%s', member='%s'",
          service_name_.c_str(),
          kPropertiesInterface,
          kPropertiesChanged);

  bus_->AddFilterFunction(&ObjectManager::HandleMessageThunk, this);

  Error error;
  bus_->AddMatch(match_rule, &error);
  if (error.IsValid()) {
    LOG(ERROR) << "ObjectManager failed to add match rule \"" << match_rule
               << "\". Got " << error.name() << ": " << error.message();
    bus_->RemoveFilterFunction(&ObjectManager::HandleMessageThunk, this);
    return false;
  }

  match_rule_ = match_rule;
  setup_success_ = true;

  return true;
}

void ObjectManager::OnSetupMatchRuleAndFilterComplete(bool success) {
  if (!success) {
    LOG(WARNING) << service_name_ << " " << object_path_.value()
                 << ": Failed to set up match rule.";
    return;
  }
  DCHECK(object_proxy_);
  DCHECK(setup_success_);

  // |object_proxy_| is no longer valid if the Bus was shut down before this
  // call. Don't initiate any other action from the origin thread.
  if (cleanup_called_) {
    return;
  }

  DCHECK(bus_);
  bus_->AssertOnOriginThread();

  object_proxy_->ConnectToSignal(
      kObjectManagerInterface, kObjectManagerInterfacesAdded,
      base::BindRepeating(&ObjectManager::InterfacesAddedReceived,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ObjectManager::InterfacesAddedConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  object_proxy_->ConnectToSignal(
      kObjectManagerInterface, kObjectManagerInterfacesRemoved,
      base::BindRepeating(&ObjectManager::InterfacesRemovedReceived,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ObjectManager::InterfacesRemovedConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!service_name_owner_.empty())
    GetManagedObjects();
}

// static
DBusHandlerResult ObjectManager::HandleMessageThunk(DBusConnection* connection,
                                                    DBusMessage* raw_message,
                                                    void* user_data) {
  ObjectManager* self = reinterpret_cast<ObjectManager*>(user_data);
  return self->HandleMessage(connection, raw_message);
}

DBusHandlerResult ObjectManager::HandleMessage(DBusConnection* connection,
                                               DBusMessage* raw_message) {
  DCHECK(bus_);
  bus_->AssertOnDBusThread();

  // Handle the message only if it is a signal.
  // Note that the match rule in SetupMatchRuleAndFilter() is configured to
  // only accept signals, but we check here just in case.
  if (dbus_message_get_type(raw_message) != DBUS_MESSAGE_TYPE_SIGNAL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  // raw_message will be unrefed on exit of the function. Increment the
  // reference so we can use it in Signal.
  dbus_message_ref(raw_message);
  std::unique_ptr<Signal> signal(Signal::FromRawMessage(raw_message));

  const std::string interface = signal->GetInterface();
  const std::string member = signal->GetMember();

  statistics::AddReceivedSignal(service_name_, interface, member);

  // Handle the signal only if it is PropertiesChanged.
  // Note that the match rule in SetupMatchRuleAndFilter() is configured to
  // only accept PropertiesChanged signals, but we check here just in case.
  const std::string absolute_signal_name =
      GetAbsoluteMemberName(interface, member);
  const std::string properties_changed_signal_name =
      GetAbsoluteMemberName(kPropertiesInterface, kPropertiesChanged);
  if (absolute_signal_name != properties_changed_signal_name)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  VLOG(1) << "Signal received: " << signal->ToString();

  // Handle the signal only if it is from the service that the ObjectManager
  // instance is interested in.
  // Note that the match rule in SetupMatchRuleAndFilter() is configured to
  // only accept messages from the service name of our interest. However, the
  // service='...' filter does not work as intended. See crbug.com/507206#14
  // and #15 for details, hence it's necessary to check the sender here.
  std::string sender = signal->GetSender();
  if (service_name_owner_ != sender)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  const ObjectPath path = signal->GetPath();

  if (bus_->HasDBusThread()) {
    // Post a task to run the method in the origin thread. Transfer ownership of
    // |signal| to NotifyPropertiesChanged, which will handle the clean up.
    Signal* released_signal = signal.release();
    bus_->GetOriginTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ObjectManager::NotifyPropertiesChanged, this,
                                  path, released_signal));
  } else {
    // If the D-Bus thread is not used, just call the callback on the
    // current thread. Transfer the ownership of |signal| to
    // NotifyPropertiesChanged.
    NotifyPropertiesChanged(path, signal.release());
  }

  // We don't return DBUS_HANDLER_RESULT_HANDLED for signals because other
  // objects may be interested in them. (e.g. Signals from org.freedesktop.DBus)
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void ObjectManager::NotifyPropertiesChanged(
    const dbus::ObjectPath object_path,
    Signal* signal) {
  DCHECK(bus_);
  bus_->AssertOnOriginThread();

  NotifyPropertiesChangedHelper(object_path, signal);

  // Delete the message on the D-Bus thread. See comments in HandleMessage.
  bus_->GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&base::DeletePointer<Signal>, signal));
}

void ObjectManager::NotifyPropertiesChangedHelper(
    const dbus::ObjectPath object_path,
    Signal* signal) {
  DCHECK(bus_);
  bus_->AssertOnOriginThread();

  MessageReader reader(signal);
  std::string interface;
  if (!reader.PopString(&interface)) {
    LOG(WARNING) << "Property changed signal has wrong parameters: "
                 << "expected interface name: " << signal->ToString();
    return;
  }

  PropertySet* properties = GetProperties(object_path, interface);
  if (properties)
    properties->ChangedReceived(signal);
}

void ObjectManager::OnGetManagedObjects(Response* response) {
  if (response != nullptr) {
    MessageReader reader(response);
    MessageReader array_reader(nullptr);
    if (!reader.PopArray(&array_reader))
      return;

    while (array_reader.HasMoreData()) {
      MessageReader dict_entry_reader(nullptr);
      ObjectPath object_path;
      if (!array_reader.PopDictEntry(&dict_entry_reader) ||
          !dict_entry_reader.PopObjectPath(&object_path))
        continue;

      UpdateObject(object_path, &dict_entry_reader);
    }

  } else {
    LOG(WARNING) << service_name_ << " " << object_path_.value()
                 << ": Failed to get managed objects";
  }
}

void ObjectManager::InterfacesAddedReceived(Signal* signal) {
  DCHECK(signal);
  MessageReader reader(signal);
  ObjectPath object_path;
  if (!reader.PopObjectPath(&object_path)) {
    LOG(WARNING) << service_name_ << " " << object_path_.value()
                 << ": InterfacesAdded signal has incorrect parameters: "
                 << signal->ToString();
    return;
  }

  UpdateObject(object_path, &reader);
}

void ObjectManager::InterfacesAddedConnected(const std::string& interface_name,
                                             const std::string& signal_name,
                                             bool success) {
  LOG_IF(WARNING, !success) << service_name_ << " " << object_path_.value()
                            << ": Failed to connect to InterfacesAdded signal.";
}

void ObjectManager::InterfacesRemovedReceived(Signal* signal) {
  DCHECK(signal);
  MessageReader reader(signal);
  ObjectPath object_path;
  std::vector<std::string> interface_names;
  if (!reader.PopObjectPath(&object_path) ||
      !reader.PopArrayOfStrings(&interface_names)) {
    LOG(WARNING) << service_name_ << " " << object_path_.value()
                 << ": InterfacesRemoved signal has incorrect parameters: "
                 << signal->ToString();
    return;
  }

  for (size_t i = 0; i < interface_names.size(); ++i)
    RemoveInterface(object_path, interface_names[i]);
}

void ObjectManager::InterfacesRemovedConnected(
    const std::string& interface_name,
    const std::string& signal_name,
    bool success) {
  LOG_IF(WARNING, !success) << service_name_ << " " << object_path_.value()
                            << ": Failed to connect to "
                            << "InterfacesRemoved signal.";
}

void ObjectManager::UpdateObject(const ObjectPath& object_path,
                                 MessageReader* reader) {
  DCHECK(reader);
  MessageReader array_reader(nullptr);
  if (!reader->PopArray(&array_reader))
    return;

  while (array_reader.HasMoreData()) {
    MessageReader dict_entry_reader(nullptr);
    std::string interface_name;
    if (!array_reader.PopDictEntry(&dict_entry_reader) ||
        !dict_entry_reader.PopString(&interface_name))
      continue;

    AddInterface(object_path, interface_name, &dict_entry_reader);
  }
}


void ObjectManager::AddInterface(const ObjectPath& object_path,
                                 const std::string& interface_name,
                                 MessageReader* reader) {
  InterfaceMap::iterator iiter = interface_map_.find(interface_name);
  if (iiter == interface_map_.end())
    return;
  Interface* interface = iiter->second;

  ObjectMap::iterator oiter = object_map_.find(object_path);
  Object* object;
  if (oiter == object_map_.end()) {
    object = object_map_[object_path] = new Object;
    object->object_proxy = bus_->GetObjectProxy(service_name_, object_path);
  } else
    object = oiter->second;

  Object::PropertiesMap::iterator piter =
      object->properties_map.find(interface_name);
  PropertySet* property_set;
  const bool interface_added = (piter == object->properties_map.end());
  if (interface_added) {
    property_set = object->properties_map[interface_name] =
        interface->CreateProperties(object->object_proxy,
                                    object_path, interface_name);
  } else
    property_set = piter->second;

  property_set->UpdatePropertiesFromReader(reader);

  if (interface_added)
    interface->ObjectAdded(object_path, interface_name);
}

void ObjectManager::RemoveInterface(const ObjectPath& object_path,
                                    const std::string& interface_name) {
  ObjectMap::iterator oiter = object_map_.find(object_path);
  if (oiter == object_map_.end())
    return;
  Object* object = oiter->second;

  Object::PropertiesMap::iterator piter =
      object->properties_map.find(interface_name);
  if (piter == object->properties_map.end())
    return;

  // Inform the interface before removing the properties structure or object
  // in case it needs details from them to make its own decisions.
  InterfaceMap::iterator iiter = interface_map_.find(interface_name);
  if (iiter != interface_map_.end()) {
    Interface* interface = iiter->second;
    interface->ObjectRemoved(object_path, interface_name);
  }

  delete piter->second;
  object->properties_map.erase(piter);

  if (object->properties_map.empty()) {
    object_map_.erase(oiter);
    delete object;
  }
}

void ObjectManager::UpdateServiceNameOwner(const std::string& new_owner) {
  bus_->AssertOnDBusThread();
  service_name_owner_ = new_owner;
}

void ObjectManager::NameOwnerChanged(const std::string& old_owner,
                                     const std::string& new_owner) {
  bus_->AssertOnOriginThread();

  bus_->GetDBusTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ObjectManager::UpdateServiceNameOwner, this, new_owner));

  if (!old_owner.empty()) {
    ObjectMap::iterator iter = object_map_.begin();
    while (iter != object_map_.end()) {
      ObjectMap::iterator tmp = iter;
      ++iter;

      // PropertiesMap is mutated by RemoveInterface, and also Object is
      // destroyed; easier to collect the object path and interface names
      // and remove them safely.
      const dbus::ObjectPath object_path = tmp->first;
      Object* object = tmp->second;
      std::vector<std::string> interfaces;

      for (Object::PropertiesMap::iterator piter =
              object->properties_map.begin();
           piter != object->properties_map.end(); ++piter)
        interfaces.push_back(piter->first);

      for (std::vector<std::string>::iterator iiter = interfaces.begin();
           iiter != interfaces.end(); ++iiter)
        RemoveInterface(object_path, *iiter);
    }

  }

  if (!new_owner.empty())
    GetManagedObjects();
}

}  // namespace dbus
