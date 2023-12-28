// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/test_service.h"

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_manager.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace dbus {

// Echo, SlowEcho, AsyncEcho, BrokenMethod, GetAll, Get, Set, PerformAction,
// GetManagedObjects
const int TestService::kNumMethodsToExport = 9;

TestService::Options::Options()
    : request_ownership_options(Bus::REQUIRE_PRIMARY) {
}

TestService::Options::~Options() = default;

TestService::TestService(const Options& options)
    : base::Thread("TestService"),
      service_name_(options.service_name),
      request_ownership_options_(options.request_ownership_options),
      dbus_task_runner_(options.dbus_task_runner),
      on_name_obtained_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
      num_exported_methods_(0),
      send_immediate_properties_changed_(false),
      has_ownership_(false),
      exported_object_(nullptr),
      exported_object_manager_(nullptr) {
  if (service_name_.empty()) {
    service_name_ = "org.chromium.TestService-" +
                    base::Uuid::GenerateRandomV4().AsLowercaseString();
  }
}

TestService::~TestService() {
  Stop();
}

bool TestService::StartService() {
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  return StartWithOptions(std::move(thread_options));
}

void TestService::WaitUntilServiceIsStarted() {
  // Wait until the ownership of the service name is obtained.
  on_name_obtained_.Wait();
}

void TestService::ShutdownAndBlock() {
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&TestService::ShutdownAndBlockInternal,
                                         base::Unretained(this)));
}

bool TestService::HasDBusThread() {
  return bus_->HasDBusThread();
}

void TestService::ShutdownAndBlockInternal() {
  if (HasDBusThread())
    bus_->ShutdownOnDBusThreadAndBlock();
  else
    bus_->ShutdownAndBlock();
}

void TestService::SendTestSignal(const std::string& message) {
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&TestService::SendTestSignalInternal,
                                         base::Unretained(this), message));
}

void TestService::SendTestSignalFromRoot(const std::string& message) {
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestService::SendTestSignalFromRootInternal,
                                base::Unretained(this), message));
}

void TestService::SendTestSignalInternal(const std::string& message) {
  Signal signal("org.chromium.TestInterface", "Test");
  MessageWriter writer(&signal);
  writer.AppendString(message);
  exported_object_->SendSignal(&signal);
}

void TestService::SendTestSignalFromRootInternal(const std::string& message) {
  Signal signal("org.chromium.TestInterface", "Test");
  MessageWriter writer(&signal);
  writer.AppendString(message);

  bus_->RequestOwnership(
      service_name_, request_ownership_options_,
      base::BindOnce(&TestService::OnOwnership, base::Unretained(this),
                     base::DoNothing()));

  // Use "/" just like dbus-send does.
  ExportedObject* root_object = bus_->GetExportedObject(ObjectPath("/"));
  root_object->SendSignal(&signal);
}

void TestService::RequestOwnership(base::OnceCallback<void(bool)> callback) {
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestService::RequestOwnershipInternal,
                                base::Unretained(this), std::move(callback)));
}

void TestService::RequestOwnershipInternal(
    base::OnceCallback<void(bool)> callback) {
  bus_->RequestOwnership(
      service_name_, request_ownership_options_,
      base::BindOnce(&TestService::OnOwnership, base::Unretained(this),
                     std::move(callback)));
}

void TestService::OnOwnership(base::OnceCallback<void(bool)> callback,
                              const std::string& service_name,
                              bool success) {
  has_ownership_ = success;
  LOG_IF(ERROR, !success) << "Failed to own: " << service_name;
  std::move(callback).Run(success);

  on_name_obtained_.Signal();
}

void TestService::ReleaseOwnership(base::OnceClosure callback) {
  bus_->GetDBusTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&TestService::ReleaseOwnershipInternal,
                                base::Unretained(this), std::move(callback)));
}

void TestService::ReleaseOwnershipInternal(base::OnceClosure callback) {
  bus_->ReleaseOwnership(service_name_);
  has_ownership_ = false;

  bus_->GetOriginTaskRunner()->PostTask(FROM_HERE, std::move(callback));
}

void TestService::SetSendImmediatePropertiesChanged() {
  send_immediate_properties_changed_ = true;
}

void TestService::OnExported(const std::string& interface_name,
                             const std::string& method_name,
                             bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export: " << interface_name << "."
               << method_name;
    // Returning here will make WaitUntilServiceIsStarted() to time out
    // and return false.
    return;
  }

  ++num_exported_methods_;
  if (num_exported_methods_ == kNumMethodsToExport) {
    // As documented in exported_object.h, the service name should be
    // requested after all methods are exposed.
    bus_->RequestOwnership(
        service_name_, request_ownership_options_,
        base::BindOnce(&TestService::OnOwnership, base::Unretained(this),
                       base::DoNothing()));
  }
}

void TestService::Run(base::RunLoop* run_loop) {
  Bus::Options bus_options;
  bus_options.bus_type = Bus::SESSION;
  bus_options.connection_type = Bus::PRIVATE;
  bus_options.dbus_task_runner = dbus_task_runner_;
  bus_ = new Bus(bus_options);

  exported_object_ = bus_->GetExportedObject(
      ObjectPath("/org/chromium/TestObject"));

  int num_methods = 0;
  exported_object_->ExportMethod(
      "org.chromium.TestInterface", "Echo",
      base::BindRepeating(&TestService::Echo, base::Unretained(this)),
      base::BindOnce(&TestService::OnExported, base::Unretained(this)));
  ++num_methods;

  exported_object_->ExportMethod(
      "org.chromium.TestInterface", "SlowEcho",
      base::BindRepeating(&TestService::SlowEcho, base::Unretained(this)),
      base::BindOnce(&TestService::OnExported, base::Unretained(this)));
  ++num_methods;

  exported_object_->ExportMethod(
      "org.chromium.TestInterface", "AsyncEcho",
      base::BindRepeating(&TestService::AsyncEcho, base::Unretained(this)),
      base::BindOnce(&TestService::OnExported, base::Unretained(this)));
  ++num_methods;

  exported_object_->ExportMethod(
      "org.chromium.TestInterface", "BrokenMethod",
      base::BindRepeating(&TestService::BrokenMethod, base::Unretained(this)),
      base::BindOnce(&TestService::OnExported, base::Unretained(this)));
  ++num_methods;

  exported_object_->ExportMethod(
      "org.chromium.TestInterface", "PerformAction",
      base::BindRepeating(&TestService::PerformAction, base::Unretained(this)),
      base::BindOnce(&TestService::OnExported, base::Unretained(this)));
  ++num_methods;

  exported_object_->ExportMethod(
      kPropertiesInterface, kPropertiesGetAll,
      base::BindRepeating(&TestService::GetAllProperties,
                          base::Unretained(this)),
      base::BindOnce(&TestService::OnExported, base::Unretained(this)));
  ++num_methods;

  exported_object_->ExportMethod(
      kPropertiesInterface, kPropertiesGet,
      base::BindRepeating(&TestService::GetProperty, base::Unretained(this)),
      base::BindOnce(&TestService::OnExported, base::Unretained(this)));
  ++num_methods;

  exported_object_->ExportMethod(
      kPropertiesInterface, kPropertiesSet,
      base::BindRepeating(&TestService::SetProperty, base::Unretained(this)),
      base::BindOnce(&TestService::OnExported, base::Unretained(this)));
  ++num_methods;

  exported_object_manager_ = bus_->GetExportedObject(
      ObjectPath("/org/chromium/TestService"));

  exported_object_manager_->ExportMethod(
      kObjectManagerInterface, kObjectManagerGetManagedObjects,
      base::BindRepeating(&TestService::GetManagedObjects,
                          base::Unretained(this)),
      base::BindOnce(&TestService::OnExported, base::Unretained(this)));
  ++num_methods;

  // Just print an error message as we don't want to crash tests.
  // Tests will fail at a call to WaitUntilServiceIsStarted().
  if (num_methods != kNumMethodsToExport) {
    LOG(ERROR) << "The number of methods does not match";
  }
  run_loop->Run();
}

void TestService::Echo(MethodCall* method_call,
                       ExportedObject::ResponseSender response_sender) {
  MessageReader reader(method_call);
  std::string text_message;
  if (!reader.PopString(&text_message)) {
    std::move(response_sender).Run(std::unique_ptr<Response>());
    return;
  }

  std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
  MessageWriter writer(response.get());
  writer.AppendString(text_message);
  std::move(response_sender).Run(std::move(response));
}

void TestService::SlowEcho(MethodCall* method_call,
                           ExportedObject::ResponseSender response_sender) {
  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  Echo(method_call, std::move(response_sender));
}

void TestService::AsyncEcho(MethodCall* method_call,
                            ExportedObject::ResponseSender response_sender) {
  // Schedule a call to Echo() to send an asynchronous response after we return.
  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestService::Echo, base::Unretained(this), method_call,
                     std::move(response_sender)),
      TestTimeouts::tiny_timeout());
}

void TestService::BrokenMethod(MethodCall* method_call,
                               ExportedObject::ResponseSender response_sender) {
  std::move(response_sender).Run(std::unique_ptr<Response>());
}


void TestService::GetAllProperties(
    MethodCall* method_call,
    ExportedObject::ResponseSender response_sender) {
  MessageReader reader(method_call);
  std::string interface;
  if (!reader.PopString(&interface)) {
    std::move(response_sender).Run(std::unique_ptr<Response>());
    return;
  }

  std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
  MessageWriter writer(response.get());

  AddPropertiesToWriter(&writer);

  std::move(response_sender).Run(std::move(response));
}

void TestService::GetProperty(MethodCall* method_call,
                              ExportedObject::ResponseSender response_sender) {
  MessageReader reader(method_call);
  std::string interface;
  if (!reader.PopString(&interface)) {
    std::move(response_sender).Run(std::unique_ptr<Response>());
    return;
  }

  std::string name;
  if (!reader.PopString(&name)) {
    std::move(response_sender).Run(std::unique_ptr<Response>());
    return;
  }

  if (name == "Name") {
    // Return the previous value for the "Name" property:
    // Variant<"TestService">
    std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
    MessageWriter writer(response.get());

    writer.AppendVariantOfString("TestService");

    std::move(response_sender).Run(std::move(response));
  } else if (name == "Version") {
    // Return a new value for the "Version" property:
    // Variant<20>
    std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
    MessageWriter writer(response.get());

    writer.AppendVariantOfInt16(20);

    std::move(response_sender).Run(std::move(response));
  } else if (name == "Methods") {
    // Return the previous value for the "Methods" property:
    // Variant<["Echo", "SlowEcho", "AsyncEcho", "BrokenMethod"]>
    std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
    MessageWriter writer(response.get());
    MessageWriter variant_writer(nullptr);
    MessageWriter variant_array_writer(nullptr);

    writer.OpenVariant("as", &variant_writer);
    variant_writer.OpenArray("s", &variant_array_writer);
    variant_array_writer.AppendString("Echo");
    variant_array_writer.AppendString("SlowEcho");
    variant_array_writer.AppendString("AsyncEcho");
    variant_array_writer.AppendString("BrokenMethod");
    variant_writer.CloseContainer(&variant_array_writer);
    writer.CloseContainer(&variant_writer);

    std::move(response_sender).Run(std::move(response));
  } else if (name == "Objects") {
    // Return the previous value for the "Objects" property:
    // Variant<[objectpath:"/TestObjectPath"]>
    std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
    MessageWriter writer(response.get());
    MessageWriter variant_writer(nullptr);
    MessageWriter variant_array_writer(nullptr);

    writer.OpenVariant("ao", &variant_writer);
    variant_writer.OpenArray("o", &variant_array_writer);
    variant_array_writer.AppendObjectPath(ObjectPath("/TestObjectPath"));
    variant_writer.CloseContainer(&variant_array_writer);
    writer.CloseContainer(&variant_writer);

    std::move(response_sender).Run(std::move(response));
  } else if (name == "Bytes") {
    // Return the previous value for the "Bytes" property:
    // Variant<[0x54, 0x65, 0x73, 0x74]>
    std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
    MessageWriter writer(response.get());
    MessageWriter variant_writer(nullptr);
    MessageWriter variant_array_writer(nullptr);

    writer.OpenVariant("ay", &variant_writer);
    const uint8_t bytes[] = {0x54, 0x65, 0x73, 0x74};
    variant_writer.AppendArrayOfBytes(bytes);
    writer.CloseContainer(&variant_writer);

    std::move(response_sender).Run(std::move(response));
  } else {
    // Return error.
    std::move(response_sender).Run(std::unique_ptr<Response>());
    return;
  }
}

void TestService::SetProperty(MethodCall* method_call,
                              ExportedObject::ResponseSender response_sender) {
  MessageReader reader(method_call);
  std::string interface;
  if (!reader.PopString(&interface)) {
    std::move(response_sender).Run(std::unique_ptr<Response>());
    return;
  }

  std::string name;
  if (!reader.PopString(&name)) {
    std::move(response_sender).Run(std::unique_ptr<Response>());
    return;
  }

  if (name != "Name") {
    std::move(response_sender).Run(std::unique_ptr<Response>());
    return;
  }

  std::string value;
  if (!reader.PopVariantOfString(&value)) {
    std::move(response_sender).Run(std::unique_ptr<Response>());
    return;
  }

  SendPropertyChangedSignal(value);

  std::move(response_sender).Run(Response::FromMethodCall(method_call));
}

void TestService::PerformAction(
      MethodCall* method_call,
      ExportedObject::ResponseSender response_sender) {
  MessageReader reader(method_call);
  std::string action;
  ObjectPath object_path;
  if (!reader.PopString(&action) || !reader.PopObjectPath(&object_path)) {
    std::move(response_sender).Run(std::unique_ptr<Response>());
    return;
  }

  if (action == "AddObject") {
    AddObject(object_path);
  } else if (action == "RemoveObject") {
    RemoveObject(object_path);
  } else if (action == "SetSendImmediatePropertiesChanged") {
    SetSendImmediatePropertiesChanged();
  } else if (action == "ReleaseOwnership") {
    ReleaseOwnership(base::BindOnce(&TestService::PerformActionResponse,
                                    base::Unretained(this), method_call,
                                    std::move(response_sender)));
    return;
  } else if (action == "Ownership") {
    ReleaseOwnership(base::BindOnce(&TestService::OwnershipReleased,
                                    base::Unretained(this), method_call,
                                    std::move(response_sender)));
    return;
  } else if (action == "InvalidateProperty") {
    SendPropertyInvalidatedSignal();
  }

  std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
  std::move(response_sender).Run(std::move(response));
}

void TestService::PerformActionResponse(
    MethodCall* method_call,
    ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
  std::move(response_sender).Run(std::move(response));
}

void TestService::OwnershipReleased(
    MethodCall* method_call,
    ExportedObject::ResponseSender response_sender) {
  RequestOwnership(base::BindOnce(&TestService::OwnershipRegained,
                                  base::Unretained(this), method_call,
                                  std::move(response_sender)));
}


void TestService::OwnershipRegained(
    MethodCall* method_call,
    ExportedObject::ResponseSender response_sender,
    bool success) {
  PerformActionResponse(method_call, std::move(response_sender));
}


void TestService::GetManagedObjects(
    MethodCall* method_call,
    ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<Response> response = Response::FromMethodCall(method_call);
  MessageWriter writer(response.get());

  // The managed objects response is a dictionary of object paths identifying
  // the object(s) with a dictionary of strings identifying the interface(s)
  // they implement and then a dictionary of property values.
  //
  // Thus this looks something like:
  //
  // {
  //   "/org/chromium/TestObject": {
  //     "org.chromium.TestInterface": { /* Properties */ }
  //   }
  // }


  MessageWriter array_writer(nullptr);
  MessageWriter dict_entry_writer(nullptr);
  MessageWriter object_array_writer(nullptr);
  MessageWriter object_dict_entry_writer(nullptr);

  writer.OpenArray("{oa{sa{sv}}}", &array_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendObjectPath(ObjectPath("/org/chromium/TestObject"));
  dict_entry_writer.OpenArray("{sa{sv}}", &object_array_writer);

  object_array_writer.OpenDictEntry(&object_dict_entry_writer);
  object_dict_entry_writer.AppendString("org.chromium.TestInterface");
  AddPropertiesToWriter(&object_dict_entry_writer);
  object_array_writer.CloseContainer(&object_dict_entry_writer);

  dict_entry_writer.CloseContainer(&object_array_writer);

  array_writer.CloseContainer(&dict_entry_writer);
  writer.CloseContainer(&array_writer);

  std::move(response_sender).Run(std::move(response));

  if (send_immediate_properties_changed_)
    SendPropertyChangedSignal("ChangedTestServiceName");
}

void TestService::AddPropertiesToWriter(MessageWriter* writer) {
  // The properties response is a dictionary of strings identifying the
  // property and a variant containing the property value. We return all
  // of the properties, thus the response is:
  //
  // {
  //   "Name": Variant<"TestService">,
  //   "Version": Variant<10>,
  //   "Methods": Variant<["Echo", "SlowEcho", "AsyncEcho", "BrokenMethod"]>,
  //   "Objects": Variant<[objectpath:"/TestObjectPath"]>
  //   "Bytes": Variant<[0x54, 0x65, 0x73, 0x74]>
  // }

  MessageWriter array_writer(nullptr);
  MessageWriter dict_entry_writer(nullptr);
  MessageWriter variant_writer(nullptr);
  MessageWriter variant_array_writer(nullptr);

  writer->OpenArray("{sv}", &array_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString("Name");
  dict_entry_writer.AppendVariantOfString("TestService");
  array_writer.CloseContainer(&dict_entry_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString("Version");
  dict_entry_writer.AppendVariantOfInt16(10);
  array_writer.CloseContainer(&dict_entry_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString("Methods");
  dict_entry_writer.OpenVariant("as", &variant_writer);
  variant_writer.OpenArray("s", &variant_array_writer);
  variant_array_writer.AppendString("Echo");
  variant_array_writer.AppendString("SlowEcho");
  variant_array_writer.AppendString("AsyncEcho");
  variant_array_writer.AppendString("BrokenMethod");
  variant_writer.CloseContainer(&variant_array_writer);
  dict_entry_writer.CloseContainer(&variant_writer);
  array_writer.CloseContainer(&dict_entry_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString("Objects");
  dict_entry_writer.OpenVariant("ao", &variant_writer);
  variant_writer.OpenArray("o", &variant_array_writer);
  variant_array_writer.AppendObjectPath(ObjectPath("/TestObjectPath"));
  variant_writer.CloseContainer(&variant_array_writer);
  dict_entry_writer.CloseContainer(&variant_writer);
  array_writer.CloseContainer(&dict_entry_writer);

  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString("Bytes");
  dict_entry_writer.OpenVariant("ay", &variant_writer);
  const uint8_t bytes[] = {0x54, 0x65, 0x73, 0x74};
  variant_writer.AppendArrayOfBytes(bytes);
  dict_entry_writer.CloseContainer(&variant_writer);
  array_writer.CloseContainer(&dict_entry_writer);

  writer->CloseContainer(&array_writer);
}

void TestService::AddObject(const ObjectPath& object_path) {
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&TestService::AddObjectInternal,
                                         base::Unretained(this), object_path));
}

void TestService::AddObjectInternal(const ObjectPath& object_path) {
  Signal signal(kObjectManagerInterface, kObjectManagerInterfacesAdded);
  MessageWriter writer(&signal);
  writer.AppendObjectPath(object_path);

  MessageWriter array_writer(nullptr);
  MessageWriter dict_entry_writer(nullptr);

  writer.OpenArray("{sa{sv}}", &array_writer);
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString("org.chromium.TestInterface");
  AddPropertiesToWriter(&dict_entry_writer);
  array_writer.CloseContainer(&dict_entry_writer);
  writer.CloseContainer(&array_writer);

  exported_object_manager_->SendSignal(&signal);
}

void TestService::RemoveObject(const ObjectPath& object_path) {
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&TestService::RemoveObjectInternal,
                                         base::Unretained(this), object_path));
}

void TestService::RemoveObjectInternal(const ObjectPath& object_path) {
  Signal signal(kObjectManagerInterface, kObjectManagerInterfacesRemoved);
  MessageWriter writer(&signal);

  writer.AppendObjectPath(object_path);

  std::vector<std::string> interfaces;
  interfaces.push_back("org.chromium.TestInterface");
  writer.AppendArrayOfStrings(interfaces);

  exported_object_manager_->SendSignal(&signal);
}

void TestService::SendPropertyChangedSignal(const std::string& name) {
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestService::SendPropertyChangedSignalInternal,
                                base::Unretained(this), name));
}

void TestService::SendPropertyChangedSignalInternal(const std::string& name) {
  Signal signal(kPropertiesInterface, kPropertiesChanged);
  MessageWriter writer(&signal);
  writer.AppendString("org.chromium.TestInterface");

  MessageWriter array_writer(nullptr);
  MessageWriter dict_entry_writer(nullptr);

  writer.OpenArray("{sv}", &array_writer);
  array_writer.OpenDictEntry(&dict_entry_writer);
  dict_entry_writer.AppendString("Name");
  dict_entry_writer.AppendVariantOfString(name);
  array_writer.CloseContainer(&dict_entry_writer);
  writer.CloseContainer(&array_writer);

  MessageWriter invalidated_array_writer(nullptr);

  writer.OpenArray("s", &invalidated_array_writer);
  writer.CloseContainer(&invalidated_array_writer);

  exported_object_->SendSignal(&signal);
}

void TestService::SendPropertyInvalidatedSignal() {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestService::SendPropertyInvalidatedSignalInternal,
                     base::Unretained(this)));
}

void TestService::SendPropertyInvalidatedSignalInternal() {
  Signal signal(kPropertiesInterface, kPropertiesChanged);
  MessageWriter writer(&signal);
  writer.AppendString("org.chromium.TestInterface");

  MessageWriter array_writer(nullptr);
  MessageWriter dict_entry_writer(nullptr);

  writer.OpenArray("{sv}", &array_writer);
  writer.CloseContainer(&array_writer);

  MessageWriter invalidated_array_writer(nullptr);

  writer.OpenArray("s", &invalidated_array_writer);
  invalidated_array_writer.AppendString("Name");
  writer.CloseContainer(&invalidated_array_writer);

  exported_object_->SendSignal(&signal);
}

}  // namespace dbus
