// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
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
  // interface to the receiver passed via |handle|. If the InterfaceBinder binds
  // an implementation it must take ownership of the receiver handle.
  virtual void BindInterface(const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle handle,
                             BinderArgs... args) = 0;
};

template <typename Interface, typename... BinderArgs>
class CallbackBinder : public InterfaceBinder<BinderArgs...> {
 public:
  using BindCallback =
      base::RepeatingCallback<void(mojo::PendingReceiver<Interface>,
                                   BinderArgs...)>;

  CallbackBinder(const BindCallback& callback,
                 const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : callback_(callback), task_runner_(task_runner) {}

  CallbackBinder(const CallbackBinder&) = delete;
  CallbackBinder& operator=(const CallbackBinder&) = delete;

  ~CallbackBinder() override = default;

 private:
  // InterfaceBinder:
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle handle,
                     BinderArgs... args) override {
    mojo::PendingReceiver<Interface> receiver(std::move(handle));
    if (task_runner_) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&CallbackBinder::RunCallback, callback_,
                                    std::move(receiver), args...));
    } else {
      RunCallback(callback_, std::move(receiver), args...);
    }
  }

  static void RunCallback(const BindCallback& callback,
                          mojo::PendingReceiver<Interface> receiver,
                          BinderArgs... args) {
    callback.Run(std::move(receiver), args...);
  }

  static void RunBindReceiverCallback(
      const base::RepeatingCallback<void(mojo::PendingReceiver<Interface>,
                                         BinderArgs...)>& callback,
      mojo::PendingReceiver<Interface> receiver,
      BinderArgs... args) {
    callback.Run(std::move(receiver), args...);
  }

  const BindCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
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

  GenericCallbackBinder(const GenericCallbackBinder&) = delete;
  GenericCallbackBinder& operator=(const GenericCallbackBinder&) = delete;

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
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_INTERFACE_BINDER_H_
