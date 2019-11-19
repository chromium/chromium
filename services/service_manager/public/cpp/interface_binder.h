// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_

#include <string>
#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace service_manager {

template <typename... BinderArgs>
void BindCallbackAdapter(
    const base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle,
                                       BinderArgs...)>& callback,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle,
    BinderArgs... args) {
  callback.Run(std::move(handle), args...);
}

template <typename... BinderArgs>
class InterfaceBinder {
 public:
  virtual ~InterfaceBinder() {}

  // Asks the InterfaceBinder to bind an implementation of the specified
  // interface to the request passed via |handle|. If the InterfaceBinder binds
  // an implementation it must take ownership of the request handle.
  virtual void BindInterface(const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle handle,
                             BinderArgs... args) = 0;
};

template <typename Interface, typename... BinderArgs>
class CallbackBinder : public InterfaceBinder<BinderArgs...> {
 public:
  using BindCallback =
      base::RepeatingCallback<void(mojo::InterfaceRequest<Interface>,
                                   BinderArgs...)>;

  CallbackBinder(const BindCallback& callback,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : callback_(callback), task_runner_(task_runner) {}
  CallbackBinder(
      const base::RepeatingCallback<void(mojo::PendingReceiver<Interface>,
                                         BinderArgs...)>& callback,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : CallbackBinder(base::BindRepeating(&RunBindReceiverCallback, callback),
                       task_runner) {}
  ~CallbackBinder() override = default;

 private:
  // InterfaceBinder:
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle,
                     BinderArgs... args) override {
    mojo::InterfaceRequest<Interface> request(std::move(handle));
    if (task_runner_) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&CallbackBinder::RunCallback, callback_,
                                    std::move(request), args...));
    } else {
      RunCallback(callback_, std::move(request), args...);
    }
  }

  static void RunCallback(const BindCallback& callback,
                          mojo::InterfaceRequest<Interface> request,
                          BinderArgs... args) {
    callback.Run(std::move(request), args...);
  }

  static void RunBindReceiverCallback(
      const base::RepeatingCallback<void(mojo::PendingReceiver<Interface>,
                                         BinderArgs...)>& callback,
      mojo::InterfaceRequest<Interface> request,
      BinderArgs... args) {
    callback.Run(std::move(request), args...);
  }

  const BindCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(CallbackBinder);
};

template <typename... BinderArgs>
class GenericCallbackBinder : public InterfaceBinder<BinderArgs...> {
 public:
  using BindCallback = base::RepeatingCallback<
      void(const std::string&, mojo::ScopedMessagePipeHandle, BinderArgs...)>;

  GenericCallbackBinder(
      const BindCallback& callback,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : callback_(callback), task_runner_(task_runner) {}
  GenericCallbackBinder(
      const base::RepeatingCallback<void(mojo::ScopedMessagePipeHandle,
                                         BinderArgs...)>& callback,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : callback_(
            base::BindRepeating(&BindCallbackAdapter<BinderArgs...>, callback)),
        task_runner_(task_runner) {}
  ~GenericCallbackBinder() override {}

 private:
  // InterfaceBinder:
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle,
                     BinderArgs... args) override {
    if (task_runner_) {
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&GenericCallbackBinder::RunCallback, callback_,
                         interface_name, std::move(handle), args...));
      return;
    }
    RunCallback(callback_, interface_name, std::move(handle), args...);
  }

  static void RunCallback(const BindCallback& callback,
                          const std::string& interface_name,
                          mojo::ScopedMessagePipeHandle handle,
                          BinderArgs... args) {
    callback.Run(interface_name, std::move(handle), args...);
  }

  const BindCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(GenericCallbackBinder);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_
