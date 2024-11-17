// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GDBUS_CONNECTION_REF_H_
#define REMOTING_HOST_LINUX_GDBUS_CONNECTION_REF_H_

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_DBus_Properties.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/gvariant_type.h"
#include "ui/base/glib/scoped_gobject.h"

namespace remoting {

// A wrapper around a GDBusConnection providing Chromium-style async callbacks
// using GVariantRef for type-safe GVariant handling.
//
// Like with GDBusConnection, calls can be made from any thread. The reply
// callback will be invoke via the default task runner of the caller's virtual
// thread, as obtained by base::SequencedTaskRunner::GetCurrentDefault().
class GDBusConnectionRef {
 public:
  using CreateCallback =
      base::OnceCallback<void(base::expected<GDBusConnectionRef, std::string>)>;
  template <typename ReturnType>
  using CallCallback =
      base::OnceCallback<void(base::expected<ReturnType, std::string>)>;
  template <typename ArgType>
  using SignalCallback = base::RepeatingCallback<void(ArgType arguments)>;
  // Can be passed in lieu of SignalCallback when more details of the emitted
  // signal are needed (e.g., when registering the same callback for multiple
  // signals).
  template <typename ArgType>
  using DetailedSignalCallback =
      base::RepeatingCallback<void(std::string sender,
                                   gvariant::ObjectPath object_path,
                                   std::string interface_name,
                                   std::string signal_name,
                                   ArgType arguments)>;

  // Returned from SignalSubscribe. Dropping will free the callback and cancel
  // the underlying subscription. Must be dropped on the sequence where it was
  // created.
  class SignalSubscription;

  // The only valid operation for a default-constructed GDBusConnectionRef is
  // assigning a connection to it.
  GDBusConnectionRef();
  GDBusConnectionRef(const GDBusConnectionRef& other);
  GDBusConnectionRef(GDBusConnectionRef&& other);
  GDBusConnectionRef& operator=(const GDBusConnectionRef& other);
  GDBusConnectionRef& operator=(GDBusConnectionRef&& other);
  ~GDBusConnectionRef();

  // Create from an existing connection.
  explicit GDBusConnectionRef(ScopedGObject<GDBusConnection> connection);

  // Asynchronously tries to create an instance for the session bus and invokes
  // callback with the result.
  static void CreateForSessionBus(CreateCallback callback);

  // Asynchronously tries to create an instance for the system bus and invokes
  // callback with the result.
  static void CreateForSystemBus(CreateCallback callback);

  // Returns whether this instance is initialized for use (not default
  // constructed or moved from).
  bool is_initialized() const;

  // Obtains the underlying GDBusConnection pointer.
  inline GDBusConnection* raw() const { return connection_.get(); }

  // Dynamically-checked overloads. Type mismatches reported at run time.

  // Asynchronously invoke the provided method, checking types at run time.
  //
  // bus_name - The owner of the object on which to call the method. May be a
  //     unique or well-known bus name. If this is a direct peer connection
  //     rather than a bus connection, pass nullptr.
  // object_path - The remote object on which to call the method.
  // interface_name - The interface the method is a part of.
  // method_name - The name of the method to call.
  // arguments - The arguments to pass to the method. The method call will fail
  //     if the arguments don't match the actual parameters expected by the
  //     method.
  // callback - A callback to invoke with the result of the method call, or an
  //     error if something goes wrong. If the actual return type can't be
  //     converted to ReturnType, an error will be returned (but note that in
  //     this case, the method was still executed.) A ReturnType of
  //     GVariantRef<"r"> can accept any return value.
  // flags - See https://docs.gtk.org/gio/flags.DBusCallFlags.html
  // timeout_msec - Timeout for the call in milliseconds. Pass -1 to use the
  //     default, or G_MAXINT for no timeout.
  template <typename ArgType, typename ReturnType>
  void Call(const char* bus_name,
            const char* object_path,
            const char* interface_name,
            const char* method_name,
            const ArgType& arguments,
            CallCallback<ReturnType> callback,
            GDBusCallFlags flags = G_DBUS_CALL_FLAGS_NONE,
            gint timeout_msec = -1) const
    requires requires(GVariantRef<"r"> variant) {
      GVariantRef<"r">::TryFrom(arguments);
      variant.TryInto<ReturnType>();
    };

  // Asynchronously retrieve the specified property value.
  //
  // bus_name - The owner of the object from which to retrieve the property. May
  //     be a unique or well-known bus name. If this is a direct peer connection
  //     rather than a bus connection, pass nullptr.
  // object_path - The remote object from which to retrieve the property.
  // interface_name - The interface the property is a part of.
  // property_name - The name of the property to retrieve.
  // callback - A callback to invoke with the retrieved property value, or an
  //     error if something goes wrong. If the property value cannot be
  //     converted to ValueType, an error will be returned. Accept a
  //     GVariantRef<> to handle any value type.
  template <typename ValueType>
  void GetProperty(const char* bus_name,
                   const char* object_path,
                   const char* interface_name,
                   const char* property_name,
                   CallCallback<ValueType> callback,
                   GDBusCallFlags flags = G_DBUS_CALL_FLAGS_NONE,
                   gint timeout_msec = -1) const
    requires requires(GVariantRef<> variant) { variant.TryInto<ValueType>(); };

  // Asynchronously set the specified property value.
  //
  // bus_name - The owner of the object on which to set the property. May be a
  //     unique or well-known bus name. If this is a direct peer connection
  //     rather than a bus connection, pass nullptr.
  // object_path - The remote object on which to set the property.
  // interface_name - The interface the property is a part of.
  // property_name - The name of the property to set.
  // value - The new property value. An error will be returned if the value is
  //     incompatible with the property.
  // callback - A callback to invoke when the property has been set, or an error
  //     if something goes wrong.
  template <typename ValueType>
  void SetProperty(const char* bus_name,
                   const char* object_path,
                   const char* interface_name,
                   const char* property_name,
                   const ValueType& value,
                   CallCallback<void> callback,
                   GDBusCallFlags flags = G_DBUS_CALL_FLAGS_NONE,
                   gint timeout_msec = -1) const
    requires requires() { GVariantRef<>::TryFrom(value); };

  // Subscribe to matching signals from the sender.
  //
  // bus_name - The sender from which to receive signals. May be a unique or
  //     well-known bus name. If this is a direct peer connection rather than a
  //     bus connection, pass nullptr.
  // object_path - The remote object from which to receive signals. May be
  //     nullptr to receive signals from all objects owned by the sender.
  // interface_name - The interface from which to receive signals. May be
  //     nullptr to receive signals from all interfaces.
  // signal_name - The name of the signals to receive. May be nullptr to
  //     receive signals with any name.
  // callback - The callback to invoke when a signal is received. Only signals
  //     convertible to ArgType will be delivered. Accept a GVariantRef<"r"> to
  //     receive signals of any type.
  //
  // Returns a subscription object, which must be dropped on the same sequence.
  // Dropping the returned object will unsubscribe from the signal and no more
  // signals will be sent.
  template <typename ArgType>
  std::unique_ptr<SignalSubscription> SignalSubscribe(
      const char* bus_name,
      const char* object_path,
      const char* interface_name,
      const char* signal_name,
      SignalCallback<ArgType> callback)
    requires requires(GVariantRef<"r"> variant) { variant.TryInto<ArgType>(); };

  // Variant of subscribe that provides sender information for the signal. In
  // addition to the signal data, provides the callback with the following
  // information:
  //
  // sender - The unique bus name of the sender of the signal. Note that this
  //     will be the unique name of the sender even if the subscription was
  //     created using the well-known name. If this is a direct peer connection,
  //     sender will be an empty string.
  // object_path - The remote object that was the source of the signal.
  // interface_name - The interface that was the source of the signal.
  // signal_name - The name of the signal.
  //
  // This additional information is mostly useful when the same callback is used
  // for multiple signals.
  template <typename ArgType>
  std::unique_ptr<SignalSubscription> SignalSubscribe(
      const char* bus_name,
      const char* object_path,
      const char* interface_name,
      const char* signal_name,
      DetailedSignalCallback<ArgType> callback)
    requires requires(GVariantRef<"r"> variant) { variant.TryInto<ArgType>(); };

  // Statically-checked overloads. Types checked against provided spec at
  // compile time. (The call may still fail for a variety of other reasons,
  // including the spec not matching the actual implementation on the bus.)

  // Asynchronously invoke the method declared by the provided MethodSpec.
  //
  // bus_name - The owner of the object on which to call the method. May be a
  //     unique or well-known bus name. If this is a direct peer connection
  //     rather than a bus connection, pass nullptr.
  // object_path - The remote object on which to call the method.
  // arguments - The arguments to pass to the method. Must be infallibly
  //     convertible to the input type declared by MethodSpec.
  // callback - A callback to invoke with the result of the method call, or an
  //     error if something goes wrong. The output type declared by MethodSpec
  //     must be infallibly convertible to ReturnType.
  // flags - See https://docs.gtk.org/gio/flags.DBusCallFlags.html
  // timeout_msec - Timeout for the call in milliseconds. Pass -1 to use the
  //     default, or G_MAXINT for no timeout.
  template <typename MethodSpec, typename ArgType, typename ReturnType>
  void Call(const char* bus_name,
            const char* object_path,
            const ArgType& arguments,
            CallCallback<ReturnType> callback,
            GDBusCallFlags flags = G_DBUS_CALL_FLAGS_NONE,
            gint timeout_msec = -1) const
    requires requires(GVariantRef<MethodSpec::kOutType> variant) {
      GVariantRef<MethodSpec::kInType>::From(arguments);
      variant.template Into<ReturnType>();
    };

  // Asynchronously retrieve the property declared by the provided PropertySpec.
  //
  // bus_name - The owner of the object from which to retrieve the property. May
  //     be a unique or well-known bus name. If this is a direct peer connection
  //     rather than a bus connection, pass nullptr.
  // object_path - The remote object from which to retrieve the property.
  // callback - A callback to invoke with the retrieved property value, or an
  //     error if something goes wrong. The property type declared by
  //     PropertySpec must be infallibly convertible to ValueType.
  template <typename PropertySpec, typename ValueType>
  void GetProperty(const char* bus_name,
                   const char* object_path,
                   CallCallback<ValueType> callback,
                   GDBusCallFlags flags = G_DBUS_CALL_FLAGS_NONE,
                   gint timeout_msec = -1) const
    requires(PropertySpec::kReadable &&
             requires(GVariantRef<PropertySpec::kType> variant) {
               variant.template Into<ValueType>();
             });

  // Asynchronously set the property declared by the provided PropertySpec.
  //
  // bus_name - The owner of the object on which to set the property. May be a
  //     unique or well-known bus name. If this is a direct peer connection
  //     rather than a bus connection, pass nullptr.
  // object_path - The remote object on which to set the property.
  // interface_name - The interface the property is a part of.
  // property_name - The name of the property to set.
  // value - The new property value. Must be infallibly convertible to the type
  //     declared by PropertySpec.
  // callback - A callback to invoke when the property has been set, or an error
  //     if something goes wrong.
  template <typename PropertySpec, typename ValueType>
  void SetProperty(const char* bus_name,
                   const char* object_path,
                   const ValueType& value,
                   CallCallback<void> callback,
                   GDBusCallFlags flags = G_DBUS_CALL_FLAGS_NONE,
                   gint timeout_msec = -1) const
    requires(PropertySpec::kWritable &&
             requires() { GVariantRef<PropertySpec::kType>::From(value); });

  // Subscribe to signals matching the provided SignalSpec.
  //
  // bus_name - The sender from which to receive signals. May be a unique or
  //     well-known bus name. If this is a direct peer connection rather than a
  //     bus connection, pass nullptr.
  // object_path - The remote object from which to receive signals. May be
  //     nullptr to receive signals from all objects owned by the sender.
  // callback - The callback to invoke when a signal is received. The signal
  //     type declared by SignalSpec must be infallibly convertible to ArgType.
  //
  // Returns a subscription object, which must be dropped on the same sequence.
  // Dropping the returned object will unsubscribe from the signal and no more
  // signals will be sent.
  template <typename SignalSpec, typename ArgType>
  std::unique_ptr<SignalSubscription> SignalSubscribe(
      const char* bus_name,
      const char* object_path,
      SignalCallback<ArgType> callback)
    requires requires(GVariantRef<SignalSpec::kType> variant) {
      variant.template Into<ArgType>();
    };

  // Variant of subscribe that provides sender information for the signal. In
  // addition to the signal data, provides the callback with the following
  // information:
  //
  // sender - The unique bus name of the sender of the signal. Note that this
  //     will be the unique name of the sender even if the subscription was
  //     created using the well-known name. If this is a direct peer connection,
  //     sender will be an empty string.
  // object_path - The remote object that was the source of the signal.
  // interface_name - The interface that was the source of the signal.
  // signal_name - The name of the signal.
  //
  // This additional information is mostly useful when the same callback is used
  // for multiple signals.
  template <typename SignalSpec, typename ArgType>
  std::unique_ptr<SignalSubscription> SignalSubscribe(
      const char* bus_name,
      const char* object_path,
      DetailedSignalCallback<ArgType> callback)
    requires requires(GVariantRef<SignalSpec::kType> variant) {
      variant.template Into<ArgType>();
    };

 private:
  static void CreateForBus(GBusType bus, CreateCallback callback);

  // Common logic for all Calls.
  void CallInternal(const char* bus_name,
                    const char* object_path,
                    const char* interface_name,
                    const char* method_name,
                    const GVariantRef<"r">& arguments,
                    CallCallback<GVariantRef<"r">> callback,
                    GDBusCallFlags flags = G_DBUS_CALL_FLAGS_NONE,
                    gint timeout_msec = -1) const;

  ScopedGObject<GDBusConnection> connection_;
};

// Represents an active signal subscription.
class GDBusConnectionRef::SignalSubscription {
 public:
  // Unsubscribes from the signal.
  ~SignalSubscription();

 private:
  // Subscribes to the signal with the given callback.
  SignalSubscription(GDBusConnectionRef connection,
                     const char* sender,
                     const char* object_path,
                     const char* interface_name,
                     const char* signal_name,
                     DetailedSignalCallback<GVariantRef<"r">> callback);

  // Called when a signal arrives. Invokes the provided callback.
  void OnSignal(std::string sender,
                gvariant::ObjectPath object_path,
                std::string interface_name,
                std::string signal_name,
                GVariantRef<"r"> arguments);

  GDBusConnectionRef connection_;
  DetailedSignalCallback<GVariantRef<"r">> callback_;
  guint subscription_id_;
  base::WeakPtrFactory<SignalSubscription> weak_factory_;

  friend class GDBusConnectionRef;
};

template <typename ArgType, typename ReturnType>
void GDBusConnectionRef::Call(const char* bus_name,
                              const char* object_path,
                              const char* interface_name,
                              const char* method_name,
                              const ArgType& arguments,
                              CallCallback<ReturnType> callback,
                              GDBusCallFlags flags,
                              gint timeout_msec) const
  requires requires(GVariantRef<"r"> variant) {
    GVariantRef<"r">::TryFrom(arguments);
    variant.TryInto<ReturnType>();
  }
{
  // First check that the provided arguments can be converted to a GVariant
  // tuple.
  auto arg_variant = GVariantRef<"r">::TryFrom(arguments);
  if (!arg_variant.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(std::move(arg_variant).error())));
    return;
  }

  // Attempt to convert return value into the target type.
  auto convert_result =
      base::BindOnce([](base::expected<GVariantRef<"r">, std::string> result) {
        return std::move(result).and_then([](GVariantRef<"r"> variant) {
          return variant.TryInto<ReturnType>();
        });
      });

  CallInternal(bus_name, object_path, interface_name, method_name,
               arg_variant.value(),
               std::move(convert_result).Then(std::move(callback)));
}

template <typename ValueType>
void GDBusConnectionRef::GetProperty(const char* bus_name,
                                     const char* object_path,
                                     const char* interface_name,
                                     const char* property_name,
                                     CallCallback<ValueType> callback,
                                     GDBusCallFlags flags,
                                     gint timeout_msec) const
  requires requires(GVariantRef<> variant) { variant.TryInto<ValueType>(); }
{
  // Ensure interface and property names are valid UTF-8.
  auto args =
      GVariantRef<"(ss)">::TryFrom(std::tuple(interface_name, property_name));
  if (!args.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::unexpected(std::move(args).error())));
    return;
  }

  // Unboxes the returned "v" value and attempts to convert it to the expected
  // type.
  auto convert_result = base::BindOnce(
      [](base::expected<GVariantRef<"(v)">, std::string> result) {
        return std::move(result).and_then([](GVariantRef<"(v)"> variant) {
          return variant.get<0>().get<0>().TryInto<ValueType>();
        });
      });

  Call<remoting::org_freedesktop_DBus_Properties::Get>(
      bus_name, object_path, args.value(),
      std::move(convert_result).Then(std::move(callback)), flags, timeout_msec);
}

template <typename ValueType>
void GDBusConnectionRef::SetProperty(const char* bus_name,
                                     const char* object_path,
                                     const char* interface_name,
                                     const char* property_name,
                                     const ValueType& value,
                                     CallCallback<void> callback,
                                     GDBusCallFlags flags,
                                     gint timeout_msec) const
  requires requires() { GVariantRef<>::TryFrom(value); }
{
  // Ensure interface and property names are valid UTF-8 and the value can be
  // converted to a GVariant.
  auto args = GVariantRef<"(ssv)">::TryFrom(std::tuple(
      interface_name, property_name, gvariant::Boxed<const ValueType&>{value}));
  if (!args.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  base::unexpected(std::move(args).error())));
    return;
  }

  // Ignores the returned std::tuple() and returns void instead.
  auto convert_result =
      base::BindOnce([](base::expected<std::tuple<>, std::string> result) {
        return std::move(result).transform([](std::tuple<>) {});
      });

  Call<remoting::org_freedesktop_DBus_Properties::Set>(
      bus_name, object_path, args.value(),
      std::move(convert_result).Then(std::move(callback)), flags, timeout_msec);
}

template <typename ArgType>
std::unique_ptr<GDBusConnectionRef::SignalSubscription>
GDBusConnectionRef::SignalSubscribe(const char* bus_name,
                                    const char* object_path,
                                    const char* interface_name,
                                    const char* signal_name,
                                    SignalCallback<ArgType> callback)
  requires requires(GVariantRef<"r"> variant) { variant.TryInto<ArgType>(); }
{
  return SignalSubscribe(
      bus_name, object_path, interface_name, signal_name,
      base::IgnoreArgs<std::string, gvariant::ObjectPath, std::string,
                       std::string>(std::move(callback)));
}

template <typename ArgType>
std::unique_ptr<GDBusConnectionRef::SignalSubscription>
GDBusConnectionRef::SignalSubscribe(const char* bus_name,
                                    const char* object_path,
                                    const char* interface_name,
                                    const char* signal_name,
                                    DetailedSignalCallback<ArgType> callback)
  requires requires(GVariantRef<"r"> variant) { variant.TryInto<ArgType>(); }
{
  // Attempts to convert return value into the target type and invokes the
  // provided callback if it matches.
  auto callback_wrapper = base::BindRepeating(
      [](const DetailedSignalCallback<ArgType>& callback, std::string sender,
         gvariant::ObjectPath object_path, std::string interface_name,
         std::string signal_name, GVariantRef<"r"> arguments) {
        base::expected<ArgType, std::string> try_result =
            arguments.TryInto<ArgType>();
        if (try_result.has_value()) {
          callback.Run(std::move(sender), std::move(object_path),
                       std::move(interface_name), std::move(signal_name),
                       std::move(try_result).value());
        }
      },
      std::move(callback));

  return std::unique_ptr<SignalSubscription>(
      new SignalSubscription(*this, bus_name, object_path, interface_name,
                             signal_name, std::move(callback_wrapper)));
}

template <typename MethodSpec, typename ArgType, typename ReturnType>
void GDBusConnectionRef::Call(const char* bus_name,
                              const char* object_path,
                              const ArgType& arguments,
                              CallCallback<ReturnType> callback,
                              GDBusCallFlags flags,
                              gint timeout_msec) const
  requires requires(GVariantRef<MethodSpec::kOutType> variant) {
    GVariantRef<MethodSpec::kInType>::From(arguments);
    variant.template Into<ReturnType>();
  }
{
  Call(bus_name, object_path, MethodSpec::kInterfaceName,
       MethodSpec::kMethodName, arguments, std::move(callback), flags,
       timeout_msec);
}

template <typename PropertySpec, typename ValueType>
void GDBusConnectionRef::GetProperty(const char* bus_name,
                                     const char* object_path,
                                     CallCallback<ValueType> callback,
                                     GDBusCallFlags flags,
                                     gint timeout_msec) const
  requires(PropertySpec::kReadable &&
           requires(GVariantRef<PropertySpec::kType> variant) {
             variant.template Into<ValueType>();
           })
{
  GetProperty(bus_name, object_path, PropertySpec::kInterfaceName,
              PropertySpec::kPropertyName, std::move(callback), flags,
              timeout_msec);
}

template <typename PropertySpec, typename ValueType>
void GDBusConnectionRef::SetProperty(const char* bus_name,
                                     const char* object_path,
                                     const ValueType& value,
                                     CallCallback<void> callback,
                                     GDBusCallFlags flags,
                                     gint timeout_msec) const
  requires(PropertySpec::kWritable &&
           requires() { GVariantRef<PropertySpec::kType>::From(value); })
{
  SetProperty(bus_name, object_path, PropertySpec::kInterfaceName,
              PropertySpec::kPropertyName, value, std::move(callback), flags,
              timeout_msec);
}

template <typename SignalSpec, typename ArgType>
std::unique_ptr<GDBusConnectionRef::SignalSubscription>
GDBusConnectionRef::SignalSubscribe(const char* bus_name,
                                    const char* object_path,
                                    SignalCallback<ArgType> callback)
  requires requires(GVariantRef<SignalSpec::kType> variant) {
    variant.template Into<ArgType>();
  }
{
  return SignalSubscribe(bus_name, object_path, SignalSpec::kInterfaceName,
                         SignalSpec::kSignalName, std::move(callback));
}

template <typename SignalSpec, typename ArgType>
std::unique_ptr<GDBusConnectionRef::SignalSubscription>
GDBusConnectionRef::SignalSubscribe(const char* bus_name,
                                    const char* object_path,
                                    DetailedSignalCallback<ArgType> callback)
  requires requires(GVariantRef<SignalSpec::kType> variant) {
    variant.template Into<ArgType>();
  }
{
  return SignalSubscribe(bus_name, object_path, SignalSpec::kInterfaceName,
                         SignalSpec::kSignalName, std::move(callback));
}

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GDBUS_CONNECTION_REF_H_
