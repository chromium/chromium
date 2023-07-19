// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_TEST_SERVICE_H_
#define DBUS_TEST_SERVICE_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"

namespace base {
class SequencedTaskRunner;
}

namespace dbus {

class MethodCall;
class MessageWriter;
class Response;

// The test service is used for end-to-end tests.  The service runs in a
// separate thread, so it does not interfere the test code that runs in
// the main thread.
//
// The test service exports an object with methods such as Echo() and
// SlowEcho(). The object has ability to send "Test" signal.
class TestService : public base::Thread {
 public:
  // Options for the test service.
  struct Options {
    Options();
    ~Options();

    // NULL by default (i.e. don't use the D-Bus thread).
    scoped_refptr<base::SequencedTaskRunner> dbus_task_runner;

    // Flags governing parameters of service ownership request.
    Bus::ServiceOwnershipOptions request_ownership_options;

    // Name of this service (randomly generated name will be used if empty).
    std::string service_name;
  };

  // The number of methods we'll export.
  static const int kNumMethodsToExport;

  explicit TestService(const Options& options);
  ~TestService() override;

  // Starts the service in a separate thread.
  // Returns true if the thread is started successfully.
  bool StartService();

  // Waits until the service is started (i.e. all methods are exported).
  void WaitUntilServiceIsStarted();

  // Shuts down the service and blocks until it's done.
  void ShutdownAndBlock();

  // Returns true if the bus has the D-Bus thread.
  bool HasDBusThread();

  // Sends "Test" signal with the given message from the exported object.
  void SendTestSignal(const std::string& message);

  // Sends "Test" signal with the given message from the root object ("/").
  // This function emulates dbus-send's behavior.
  void SendTestSignalFromRoot(const std::string& message);

  // Request the ownership of a well-known name "TestService".
  // |callback| will be called with the result when an ownership request is
  // completed.
  void RequestOwnership(base::OnceCallback<void(bool)> callback);

  // Release the ownership of the well-known name "TestService".
  // |callback| will be called when the ownership has been released.
  void ReleaseOwnership(base::OnceClosure callback);

  // Returns the name of this service.
  const std::string& service_name() const { return service_name_; }

  // Returns whether this instance has the name ownership or not.
  bool has_ownership() const { return has_ownership_; }

 private:
  // Helper function for SendTestSignal().
  void SendTestSignalInternal(const std::string& message);

  // Helper function for SendTestSignalFromRoot.
  void SendTestSignalFromRootInternal(const std::string& message);

  // Helper function for ShutdownAndBlock().
  void ShutdownAndBlockInternal();

  // Called when an ownership request is completed.
  // |callback| is the callback to be called with the result. |service_name| is
  // the requested well-known bus name. |callback| and |service_name| are bound
  // when the service requests the ownership. |success| is the result of the
  // completed request, and is propagated to |callback|.
  void OnOwnership(base::OnceCallback<void(bool)> callback,
                   const std::string& service_name,
                   bool success);

  // Called when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // base::Thread override.
  void Run(base::RunLoop* run_loop) override;

  //
  // Exported methods.
  //

  // Echos the text message received from the method call.
  void Echo(MethodCall* method_call,
            dbus::ExportedObject::ResponseSender response_sender);

  // Echos the text message received from the method call, but sleeps for
  // TestTimeouts::tiny_timeout_ms() before returning the response.
  void SlowEcho(MethodCall* method_call,
                dbus::ExportedObject::ResponseSender response_sender);

  // Echos the text message received from the method call, but sends its
  // response asynchronously after this callback has returned.
  void AsyncEcho(MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender);

  // Returns NULL, instead of a valid Response.
  void BrokenMethod(MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);

  // Returns a set of property values for testing.
  void GetAllProperties(MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender);

  // Returns a new value of 20 for the Version property when called.
  void GetProperty(MethodCall* method_call,
                   dbus::ExportedObject::ResponseSender response_sender);

  // Allows the name property to be changed, errors otherwise.
  void SetProperty(MethodCall* method_call,
                   dbus::ExportedObject::ResponseSender response_sender);

  // Performs an action for testing.
  void PerformAction(MethodCall* method_call,
                     dbus::ExportedObject::ResponseSender response_sender);

  // Object Manager: returns the set of objects and properties.
  void GetManagedObjects(MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender);

  // Add a properties dictionary to a message writer.
  void AddPropertiesToWriter(MessageWriter* writer);

  // Add a new object to the manager.
  void AddObject(const dbus::ObjectPath& object_path);
  void AddObjectInternal(const dbus::ObjectPath& object_path);

  // Remove an object from the manager.
  void RemoveObject(const dbus::ObjectPath& object_path);
  void RemoveObjectInternal(const dbus::ObjectPath& object_path);

  // Sends a property changed signal for the name property.
  void SendPropertyChangedSignal(const std::string& name);

  // Helper function for SendPropertyChangedSignal().
  void SendPropertyChangedSignalInternal(const std::string& name);

  // Sends a property invalidated signal for the name property.
  void SendPropertyInvalidatedSignal();

  // Helper function for SendPropertyInvalidatedSignal().
  void SendPropertyInvalidatedSignalInternal();

  // Helper function for RequestOwnership().
  void RequestOwnershipInternal(base::OnceCallback<void(bool)> callback);

  // Helper function for ReleaseOwnership().
  void ReleaseOwnershipInternal(base::OnceClosure callback);

  // Configures the test service to send a PropertiesChanged signal for the
  // "Name" property immediately after a call to GetManagedObjects.
  void SetSendImmediatePropertiesChanged();

  // Sends the response on completion of the performed action.
  void PerformActionResponse(
      MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Re-requests ownership of the well-known name after releasing it.
  void OwnershipReleased(
      MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Sends the action response after regaining the well-known name.
  void OwnershipRegained(
      MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender,
      bool success);

  // Name of this service.
  std::string service_name_;

  // Options to use when requesting service ownership.
  Bus::ServiceOwnershipOptions request_ownership_options_;

  scoped_refptr<base::SequencedTaskRunner> dbus_task_runner_;
  base::WaitableEvent on_name_obtained_;
  // The number of methods actually exported.
  int num_exported_methods_;

  // True if a PropertiesChanged signal for the "Name" property should be sent
  // immediately following a call to GetManagedObjects.
  bool send_immediate_properties_changed_;

  // True iff this instance has successfully acquired the name ownership.
  bool has_ownership_;

  scoped_refptr<Bus> bus_;
  raw_ptr<ExportedObject, AcrossTasksDanglingUntriaged> exported_object_;
  raw_ptr<ExportedObject, AcrossTasksDanglingUntriaged>
      exported_object_manager_;
};

}  // namespace dbus

#endif  // DBUS_TEST_SERVICE_H_
