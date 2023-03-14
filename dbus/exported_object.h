// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_EXPORTED_OBJECT_H_
#define DBUS_EXPORTED_OBJECT_H_

#include <dbus/dbus.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "dbus/dbus_export.h"
#include "dbus/object_path.h"

namespace dbus {

class Bus;
class MethodCall;
class Response;
class Signal;

// ExportedObject is used to export objects and methods to other D-Bus
// clients.
//
// ExportedObject is a ref counted object, to ensure that |this| of the
// object is alive when callbacks referencing |this| are called.
class CHROME_DBUS_EXPORT ExportedObject
    : public base::RefCountedThreadSafe<ExportedObject> {
 public:
  // Client code should use Bus::GetExportedObject() instead of this
  // constructor.
  ExportedObject(Bus* bus, const ObjectPath& object_path);

  // Called to send a response from an exported method. |response| is the
  // response message. Callers should pass nullptr in the event of an error that
  // prevents the sending of a response.
  using ResponseSender =
      base::OnceCallback<void(std::unique_ptr<Response> response)>;

  // Called when an exported method is called. |method_call| is the request
  // message. |sender| is the callback that's used to send a response.
  //
  // |method_call| is owned by ExportedObject, hence client code should not
  // delete |method_call|.
  using MethodCallCallback =
      base::RepeatingCallback<void(MethodCall* method_call,
                                   ResponseSender sender)>;

  // Called when method exporting is done.
  // |success| indicates whether exporting was successful or not.
  using OnExportedCallback =
      base::OnceCallback<void(const std::string& interface_name,
                              const std::string& method_name,
                              bool success)>;

  // Called when method unexporting is done.
  // |success| indicates whether unexporting was successful or not.
  using OnUnexportedCallback =
      base::OnceCallback<void(const std::string& interface_name,
                              const std::string& method_name,
                              bool success)>;

  // Exports the method specified by |interface_name| and |method_name|,
  // and blocks until exporting is done. Returns true on success.
  //
  // |method_call_callback| will be called in the origin thread, when the
  // exported method is called. As it's called in the origin thread,
  // |method_callback| can safely reference objects in the origin thread
  // (i.e. UI thread in most cases).
  //
  // IMPORTANT NOTE: You should export all methods before requesting a
  // service name by Bus::RequestOwnership/AndBlock(). If you do it in the
  // wrong order (i.e. request a service name then export methods), there
  // will be a short time period where your service is unable to respond to
  // method calls because these methods aren't yet exposed. This race is a
  // real problem as clients may start calling methods of your service as
  // soon as you acquire a service name, by watching the name owner change.
  //
  // BLOCKING CALL.
  virtual bool ExportMethodAndBlock(
      const std::string& interface_name,
      const std::string& method_name,
      const MethodCallCallback& method_call_callback);

  // Unexports the method specified by |interface_name| and |method_name|,
  // and blocks until unexporting is done. Returns true on success.
  virtual bool UnexportMethodAndBlock(const std::string& interface_name,
                                      const std::string& method_name);

  // Requests to export the method specified by |interface_name| and
  // |method_name|. See Also ExportMethodAndBlock().
  //
  // |on_exported_callback| is called when the method is exported or
  // failed to be exported, in the origin thread.
  //
  // Must be called in the origin thread.
  virtual void ExportMethod(const std::string& interface_name,
                            const std::string& method_name,
                            const MethodCallCallback& method_call_callback,
                            OnExportedCallback on_exported_callback);

  // Requests to unexport the method specified by |interface_name| and
  // |method_name|. See also UnexportMethodAndBlock().
  //
  // |on_unexported_callback| is called when the method is unexported or
  // failed to be unexported, in the origin thread.
  //
  // Must be called in the origin thread.
  virtual void UnexportMethod(const std::string& interface_name,
                              const std::string& method_name,
                              OnUnexportedCallback on_unexported_callback);

  // Requests to send the signal from this object. The signal will be sent
  // synchronously if this method is called from the message loop in the D-Bus
  // thread and asynchronously otherwise.
  virtual void SendSignal(Signal* signal);

  // Unregisters the object from the bus. The Bus object will take care of
  // unregistering so you don't have to do this manually.
  //
  // BLOCKING CALL.
  virtual void Unregister();

 protected:
  // This is protected, so we can define sub classes.
  virtual ~ExportedObject();

 private:
  friend class base::RefCountedThreadSafe<ExportedObject>;

  // Helper function for ExportMethod().
  void ExportMethodInternal(const std::string& interface_name,
                            const std::string& method_name,
                            const MethodCallCallback& method_call_callback,
                            OnExportedCallback exported_callback);

  // Helper function for UnexportMethod().
  void UnexportMethodInternal(const std::string& interface_name,
                              const std::string& method_name,
                              OnUnexportedCallback unexported_callback);

  // Called when the object is exported.
  void OnExported(OnExportedCallback on_exported_callback,
                  const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Called when a method is unexported.
  void OnUnexported(OnExportedCallback on_unexported_callback,
                    const std::string& interface_name,
                    const std::string& method_name,
                    bool success);

  // Helper function for SendSignal().
  void SendSignalInternal(DBusMessage* signal_message);

  // Registers this object to the bus.
  // Returns true on success, or the object is already registered.
  //
  // BLOCKING CALL.
  bool Register();

  // Handles the incoming request messages and dispatches to the exported
  // methods.
  DBusHandlerResult HandleMessage(DBusConnection* connection,
                                  DBusMessage* raw_message);

  // Runs the method. Helper function for HandleMessage().
  void RunMethod(const MethodCallCallback& method_call_callback,
                 std::unique_ptr<MethodCall> method_call);

  // Callback invoked by service provider to send a response to a method call.
  // Can be called immediately from a MethodCallCallback to implement a
  // synchronous service or called later to implement an asynchronous service.
  void SendResponse(std::unique_ptr<MethodCall> method_call,
                    std::unique_ptr<Response> response);

  // Called on completion of the method run from SendResponse().
  // Takes ownership of |method_call| and |response|.
  void OnMethodCompleted(std::unique_ptr<MethodCall> method_call,
                         std::unique_ptr<Response> response);

  // Called when the object is unregistered.
  void OnUnregistered(DBusConnection* connection);

  // Redirects the function call to HandleMessage().
  static DBusHandlerResult HandleMessageThunk(DBusConnection* connection,
                                              DBusMessage* raw_message,
                                              void* user_data);

  // Redirects the function call to OnUnregistered().
  static void OnUnregisteredThunk(DBusConnection* connection,
                                  void* user_data);

  scoped_refptr<Bus> bus_;
  ObjectPath object_path_;
  bool object_is_registered_;

  // The method table where keys are absolute method names (i.e. interface
  // name + method name), and values are the corresponding callbacks.
  typedef std::map<std::string, MethodCallCallback> MethodTable;
  MethodTable method_table_;
};

}  // namespace dbus

#endif  // DBUS_EXPORTED_OBJECT_H_
