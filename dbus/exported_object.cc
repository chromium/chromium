// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/exported_object.h"

#include <stdint.h>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/bus.h"
#include "dbus/error.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/scoped_dbus_error.h"
#include "dbus/util.h"

namespace dbus {

ExportedObject::ExportedObject(Bus* bus,
                               const ObjectPath& object_path)
    : bus_(bus),
      object_path_(object_path),
      object_is_registered_(false) {
  LOG_IF(FATAL, !object_path_.IsValid()) << object_path_.value();
}

ExportedObject::~ExportedObject() {
  DCHECK(!object_is_registered_);
}

bool ExportedObject::ExportMethodAndBlock(
    const std::string& interface_name,
    const std::string& method_name,
    const MethodCallCallback& method_call_callback) {
  bus_->AssertOnDBusThread();

  // Check if the method is already exported.
  const std::string absolute_method_name =
      GetAbsoluteMemberName(interface_name, method_name);
  if (base::Contains(method_table_, absolute_method_name)) {
    LOG(ERROR) << absolute_method_name << " is already exported";
    return false;
  }

  if (!bus_->Connect())
    return false;
  if (!bus_->SetUpAsyncOperations())
    return false;
  if (!Register())
    return false;

  // Add the method callback to the method table.
  method_table_[absolute_method_name] = method_call_callback;

  return true;
}

bool ExportedObject::UnexportMethodAndBlock(const std::string& interface_name,
                                            const std::string& method_name) {
  bus_->AssertOnDBusThread();

  const std::string absolute_method_name =
      GetAbsoluteMemberName(interface_name, method_name);
  MethodTable::const_iterator iter = method_table_.find(absolute_method_name);
  if (iter == method_table_.end()) {
    LOG(ERROR) << absolute_method_name << " is not exported";
    return false;
  }

  method_table_.erase(iter);

  return true;
}

void ExportedObject::ExportMethod(
    const std::string& interface_name,
    const std::string& method_name,
    const MethodCallCallback& method_call_callback,
    OnExportedCallback on_exported_callback) {
  bus_->AssertOnOriginThread();

  base::OnceClosure task = base::BindOnce(
      &ExportedObject::ExportMethodInternal, this, interface_name, method_name,
      method_call_callback, std::move(on_exported_callback));
  bus_->GetDBusTaskRunner()->PostTask(FROM_HERE, std::move(task));
}

void ExportedObject::UnexportMethod(
    const std::string& interface_name,
    const std::string& method_name,
    OnUnexportedCallback on_unexported_callback) {
  bus_->AssertOnOriginThread();

  base::OnceClosure task = base::BindOnce(
      &ExportedObject::UnexportMethodInternal, this, interface_name,
      method_name, std::move(on_unexported_callback));
  bus_->GetDBusTaskRunner()->PostTask(FROM_HERE, std::move(task));
}

void ExportedObject::SendSignal(Signal* signal) {
  // For signals, the object path should be set to the path to the sender
  // object, which is this exported object here.
  CHECK(signal->SetPath(object_path_));

  // Increment the reference count so we can safely reference the
  // underlying signal message until the signal sending is complete. This
  // will be unref'ed in SendSignalInternal().
  DBusMessage* signal_message = signal->raw_message();
  dbus_message_ref(signal_message);

  if (bus_->GetDBusTaskRunner()->RunsTasksInCurrentSequence()) {
    // The Chrome OS power manager doesn't use a dedicated TaskRunner for
    // sending DBus messages.  Sending signals asynchronously can cause an
    // inversion in the message order if the power manager calls
    // ObjectProxy::CallMethodAndBlock() before going back to the top level of
    // the MessageLoop: crbug.com/472361.
    SendSignalInternal(signal_message);
  } else {
    bus_->GetDBusTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ExportedObject::SendSignalInternal, this,
                                  signal_message));
  }
}

void ExportedObject::Unregister() {
  bus_->AssertOnDBusThread();

  if (!object_is_registered_)
    return;

  bus_->UnregisterObjectPath(object_path_);
  object_is_registered_ = false;
}

void ExportedObject::ExportMethodInternal(
    const std::string& interface_name,
    const std::string& method_name,
    const MethodCallCallback& method_call_callback,
    OnExportedCallback on_exported_callback) {
  bus_->AssertOnDBusThread();

  const bool success = ExportMethodAndBlock(interface_name,
                                            method_name,
                                            method_call_callback);
  bus_->GetOriginTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ExportedObject::OnExported, this,
                                std::move(on_exported_callback), interface_name,
                                method_name, success));
}

void ExportedObject::UnexportMethodInternal(
    const std::string& interface_name,
    const std::string& method_name,
    OnUnexportedCallback on_unexported_callback) {
  bus_->AssertOnDBusThread();

  const bool success = UnexportMethodAndBlock(interface_name, method_name);
  bus_->GetOriginTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ExportedObject::OnUnexported, this,
                                std::move(on_unexported_callback),
                                interface_name, method_name, success));
}

void ExportedObject::OnExported(OnExportedCallback on_exported_callback,
                                const std::string& interface_name,
                                const std::string& method_name,
                                bool success) {
  bus_->AssertOnOriginThread();

  std::move(on_exported_callback).Run(interface_name, method_name, success);
}

void ExportedObject::OnUnexported(OnExportedCallback on_unexported_callback,
                                  const std::string& interface_name,
                                  const std::string& method_name,
                                  bool success) {
  bus_->AssertOnOriginThread();

  std::move(on_unexported_callback).Run(interface_name, method_name, success);
}

void ExportedObject::SendSignalInternal(DBusMessage* signal_message) {
  uint32_t serial = 0;
  bus_->Send(signal_message, &serial);
  dbus_message_unref(signal_message);
}

bool ExportedObject::Register() {
  bus_->AssertOnDBusThread();

  if (object_is_registered_)
    return true;

  Error error;

  DBusObjectPathVTable vtable = {};
  vtable.message_function = &ExportedObject::HandleMessageThunk;
  vtable.unregister_function = &ExportedObject::OnUnregisteredThunk;
  const bool success =
      bus_->TryRegisterObjectPath(object_path_, &vtable, this, &error);
  if (!success) {
    LOG(ERROR) << "Failed to register the object: " << object_path_.value()
               << ": " << error.message();
    return false;
  }

  object_is_registered_ = true;
  return true;
}

DBusHandlerResult ExportedObject::HandleMessage(
    DBusConnection* connection,
    DBusMessage* raw_message) {
  bus_->AssertOnDBusThread();
  // ExportedObject only handles method calls. Ignore other message types (e.g.
  // signal).
  if (dbus_message_get_type(raw_message) != DBUS_MESSAGE_TYPE_METHOD_CALL)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  // raw_message will be unrefed on exit of the function. Increment the
  // reference so we can use it in MethodCall.
  dbus_message_ref(raw_message);
  std::unique_ptr<MethodCall> method_call(
      MethodCall::FromRawMessage(raw_message));
  const std::string interface = method_call->GetInterface();
  const std::string member = method_call->GetMember();

  if (interface.empty()) {
    // We don't support method calls without interface.
    LOG(WARNING) << "Interface is missing: " << method_call->ToString();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  // Check if we know about the method.
  const std::string absolute_method_name = GetAbsoluteMemberName(
      interface, member);
  MethodTable::const_iterator iter = method_table_.find(absolute_method_name);
  if (iter == method_table_.end()) {
    // Don't know about the method.
    LOG(WARNING) << "Unknown method: " << method_call->ToString();
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  if (bus_->HasDBusThread()) {
    // Post a task to run the method in the origin thread.
    bus_->GetOriginTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ExportedObject::RunMethod, this,
                                  iter->second, std::move(method_call)));
  } else {
    // If the D-Bus thread is not used, just call the method directly.
    MethodCall* method = method_call.get();
    iter->second.Run(method, base::BindOnce(&ExportedObject::SendResponse, this,
                                            std::move(method_call)));
  }

  // It's valid to say HANDLED here, and send a method response at a later
  // time from OnMethodCompleted() asynchronously.
  return DBUS_HANDLER_RESULT_HANDLED;
}

void ExportedObject::RunMethod(const MethodCallCallback& method_call_callback,
                               std::unique_ptr<MethodCall> method_call) {
  bus_->AssertOnOriginThread();
  MethodCall* method = method_call.get();
  method_call_callback.Run(method,
                           base::BindOnce(&ExportedObject::SendResponse, this,
                                          std::move(method_call)));
}

void ExportedObject::SendResponse(std::unique_ptr<MethodCall> method_call,
                                  std::unique_ptr<Response> response) {
  DCHECK(method_call);
  if (bus_->HasDBusThread()) {
    bus_->GetDBusTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ExportedObject::OnMethodCompleted, this,
                                  std::move(method_call), std::move(response)));
  } else {
    OnMethodCompleted(std::move(method_call), std::move(response));
  }
}

void ExportedObject::OnMethodCompleted(std::unique_ptr<MethodCall> method_call,
                                       std::unique_ptr<Response> response) {
  bus_->AssertOnDBusThread();

  // Check if the bus is still connected. If the method takes long to
  // complete, the bus may be shut down meanwhile.
  if (!bus_->IsConnected())
    return;

  if (!response) {
    // Something bad happened in the method call.
    std::unique_ptr<ErrorResponse> error_response(ErrorResponse::FromMethodCall(
        method_call.get(), DBUS_ERROR_FAILED,
        "error occurred in " + method_call->GetMember()));
    bus_->Send(error_response->raw_message(), nullptr);
    return;
  }

  // The method call was successful.
  bus_->Send(response->raw_message(), nullptr);
}

void ExportedObject::OnUnregistered(DBusConnection* connection) {
}

DBusHandlerResult ExportedObject::HandleMessageThunk(
    DBusConnection* connection,
    DBusMessage* raw_message,
    void* user_data) {
  ExportedObject* self = reinterpret_cast<ExportedObject*>(user_data);
  return self->HandleMessage(connection, raw_message);
}

void ExportedObject::OnUnregisteredThunk(DBusConnection *connection,
                                         void* user_data) {
  ExportedObject* self = reinterpret_cast<ExportedObject*>(user_data);
  return self->OnUnregistered(connection);
}

}  // namespace dbus
