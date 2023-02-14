// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_EXPORTED_CALLBACK_MANAGER_H_
#define DEVICE_BLUETOOTH_FLOSS_EXPORTED_CALLBACK_MANAGER_H_

#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace {

template <typename T>
using MethodDelegate =
    base::RepeatingCallback<void(dbus::MethodCall*,
                                 base::WeakPtr<T>,
                                 dbus::ExportedObject::ResponseSender)>;

// A wrapper that runs the barrier closure |callback|.
void OnMethodExported(base::RepeatingClosure callback,
                      const std::string& interface,
                      const std::string& method,
                      bool success) {
  callback.Run();
}
}

// Private class helper.
template <typename T, typename... Args>
class CallbackForwarder {
 private:
  // Holds a parameter pack C++ has problem with inferring multiple variadic
  // template parameters. So this is one way to group each template parameter
  // pack to disambiguate them.
  template <typename...>
  struct TypeList;

  // Converts parameters, forwards to callback object, and reply the D-Bus
  // sender. This is actually a function but is written as a struct because C++
  // does not allow partial function template specialization.
  template <typename T1, typename T2>
  struct ParseParamsAndForward {};

  // Base case. There is no more parameters to parse and we have the complete
  // built parameters to forward to the callback function and then reply the
  // sender.
  template <typename... BuiltArgs>
  struct ParseParamsAndForward<TypeList<>, TypeList<BuiltArgs...>> {
    static void Do(dbus::MessageReader* reader,
                   dbus::MethodCall* method_call,
                   base::OnceCallback<void(Args...)> delegate,
                   dbus::ExportedObject::ResponseSender response_sender,
                   BuiltArgs... params) {
      std::move(delegate).Run(std::forward<BuiltArgs>(params)...);

      std::move(response_sender)
          .Run(dbus::Response::FromMethodCall(method_call));
    }
  };

  // Recursively (at compile-time) parse parameters and builds parameter list.
  // At the end of the recursion we will have a list of parameters already
  // parsed at |BuiltArgs... params| and ready to forward to the callback
  // function.
  template <typename FirstType,
            typename... RemainingArgs,
            typename... BuiltArgs>
  struct ParseParamsAndForward<TypeList<FirstType, RemainingArgs...>,
                               TypeList<BuiltArgs...>> {
    static void Do(dbus::MessageReader* reader,
                   dbus::MethodCall* method_call,
                   base::OnceCallback<void(Args...)> delegate,
                   dbus::ExportedObject::ResponseSender response_sender,
                   BuiltArgs... params) {
      std::decay_t<FirstType> data;
      if (!floss::FlossDBusClient::ReadDBusParam(reader, &data)) {
        std::stringstream message;
        floss::DBusTypeInfo type_info = floss::GetDBusTypeInfo(&data);
        std::string next_data_type =
            reader->HasMoreData() ? ("'" + reader->GetDataSignature() + "'")
                                  : "none";
        message << "Cannot parse the " << (sizeof...(BuiltArgs) + 1)
                << "th parameter, expected type signature '"
                << type_info.dbus_signature << "' "
                << "(" << type_info.type_name << ")"
                << ", got " << next_data_type;
        std::move(response_sender)
            .Run(dbus::ErrorResponse::FromMethodCall(
                method_call, floss::FlossDBusClient::kErrorInvalidParameters,
                message.str()));
        return;
      }

      ParseParamsAndForward<TypeList<RemainingArgs...>,
                            TypeList<BuiltArgs..., FirstType>>::
          Do(reader, method_call, std::move(delegate),
             std::move(response_sender), std::forward<BuiltArgs>(params)...,
             data);
    }
  };

  // The start of the recursive ParseParamsAndForward.
  static void Forward(dbus::MethodCall* method_call,
                      base::OnceCallback<void(Args...)> delegate,
                      dbus::ExportedObject::ResponseSender response_sender) {
    dbus::MessageReader reader(method_call);
    ParseParamsAndForward<TypeList<Args...>, TypeList<>>::Do(
        &reader, method_call, std::move(delegate), std::move(response_sender));
  }

 public:
  // Returns a RepeatingCallback with captured |func| that parses D-Bus
  // parameters and forwards it to |func|.
  //
  // Being a RepeatingCallback has the benefit that the invoker does not need to
  // know the signature of |func| at compile time.
  static MethodDelegate<T> CreateForwarder(void (T::*func)(Args...)) {
    return base::BindRepeating(
        [](void (T::*func)(Args...), dbus::MethodCall* method_call,
           base::WeakPtr<T> target,
           dbus::ExportedObject::ResponseSender response_sender) {
          Forward(method_call, base::BindOnce(func, target),
                  std::move(response_sender));
        },
        func);
  }
};

namespace floss {

// Utility to manage callbacks. This simplifies:
// * Exporting and unexporting a callback object to/from D-Bus.
// * Forwarding received D-Bus method calls to C++ callback objects,
//   including parsing the method parameters according to the defined types.
//
// Example usage:
//
// // Create the manager and specify that this is for type ISomeCallback
// // and the D-Bus interface name is "org.some.interface".
// ExportedCallbackManager<ISomeCallback> manager("org.some.interface");
//
// // Must be called first before usage.
// manager.Init(bus);
//
// // Define forwarding for method "OnSomethingHappened" to function call of
// // ISomeCallback::OnSomethingHappened. The manager handles the parsing of
// // parameters and forwarding to the function according to the types.
// manager.AddMethod(
//     "OnSomethingHappened", &ISomeCallback::OnSomethingHappened);
// // Define all other methods.
// manager.AddMethod("SomeMethod", &ISomeCallback::SomeMethod);
//
// auto some_callback = std::make_unique<CreateSomeCallbackImpl>();
// // After defining the methods, it's ready to export callback objects.
// manager.ExportCallback(
//     dbus::ObjectPath("/path/to/callback"), some_callback->GetWeakPtr());
template <typename T>
class ExportedCallbackManager {
 public:
  // |interface_name| specifies the D-Bus interface name of the exported
  // callbacks managed by this utility.
  explicit ExportedCallbackManager(std::string interface_name)
      : interface_name_(std::move(interface_name)) {}

  // Initializes the manager with a |bus|. Must be called before any usage.
  void Init(scoped_refptr<dbus::Bus> bus) { bus_ = bus; }

  // Adds a method to be forwarded, following the specified method name and
  // the pointer to a member function of T.
  template <typename... Args>
  void AddMethod(std::string name, void (T::*func)(Args...)) {
    auto forwarder = CallbackForwarder<T, Args...>::CreateForwarder(func);
    methods_[name] = forwarder;
  }

  // Exports the callback object to D-Bus. This object will receive method calls
  // that are defined via AddMethod.
  //
  // |exported_callback| weak pointer has to be valid at time of invocation.
  bool ExportCallback(const dbus::ObjectPath& callback_path,
                      base::WeakPtr<T> exported_callback,
                      base::OnceCallback<void()> on_exported_callback) {
    CHECK(exported_callback) << "Callback ptr is not valid";
    CHECK(bus_) << "Called without Init";

    VLOG(1) << "Exporting callback at " << callback_path.value();

    if (base::Contains(exported_callbacks_, callback_path.value())) {
      LOG(ERROR) << "Cannot export existing object path";
      return false;
    }

    exported_callbacks_[callback_path.value()] = exported_callback;

    dbus::ExportedObject* exported_object =
        bus_->GetExportedObject(callback_path);
    if (!exported_object) {
      LOG(ERROR) << "Could not export client callback "
                 << callback_path.value();
      return false;
    }

    auto export_complete =
        base::BarrierClosure(methods_.size(), std::move(on_exported_callback));

    // Catch all registered methods with OnMethodCall and it will handle the
    // forwarding to the callback.
    for (auto const& [name, method] : methods_) {
      VLOG(1) << "Exporting method " << interface_name_ << "." << name;
      exported_object->ExportMethod(
          interface_name_, name,
          base::BindRepeating(&ExportedCallbackManager::OnMethodCall,
                              weak_ptr_factory_.GetWeakPtr(), name, method,
                              exported_callback),
          base::BindOnce(&OnMethodExported, export_complete));
    }

    return true;
  }

  // Removes the D-Bus object from being exported.
  void UnexportCallback(const dbus::ObjectPath& callback_path) {
    if (!base::Contains(exported_callbacks_, callback_path.value())) {
      LOG(WARNING) << "Not yet exported: " << callback_path.value();
      return;
    }

    bus_->UnregisterExportedObject(callback_path);

    exported_callbacks_.erase(callback_path.value());
  }

 private:
  void OnMethodCall(std::string method_name,
                    MethodDelegate<T> delegate,
                    base::WeakPtr<T> exported_callback,
                    dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender) {
    if (!exported_callback) {
      LOG(WARNING) << "Callback no longer exists for method " << method_name;
      std::move(response_sender)
          .Run(dbus::ErrorResponse::FromMethodCall(
              method_call, floss::FlossDBusClient::kErrorDoesNotExist,
              "Callback does not exist"));
      return;
    }

    DCHECK(method_name == method_call->GetMember())
        << "Method name from D-Bus does not match with the registered name";

    delegate.Run(method_call, exported_callback, std::move(response_sender));
  }

  scoped_refptr<dbus::Bus> bus_;

  std::string interface_name_;

  std::unordered_map<std::string, base::WeakPtr<T>> exported_callbacks_;

  std::unordered_map<std::string, MethodDelegate<T>> methods_;

  // WeakPtrFactory must be last.
  base::WeakPtrFactory<ExportedCallbackManager> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_EXPORTED_CALLBACK_MANAGER_H_
