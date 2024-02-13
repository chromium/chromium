// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_EVENT_UTILS_WINRT_H_
#define DEVICE_BLUETOOTH_EVENT_UTILS_WINRT_H_

#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include <optional>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace device {

namespace internal {

template <typename Interface, typename... Args>
using IMemberFunction = HRESULT (__stdcall Interface::*)(Args...);

}  // namespace internal

// Convenience template function to construct a TypedEventHandler from a
// base::RepeatingCallback of a matching signature. In case of success, the
// EventRegistrationToken is returned to the caller. A return value of
// std::nullopt indicates a failure. Events are posted to the same thread the
// event handler was created on.
template <typename Interface,
          typename Sender,
          typename Args,
          typename SenderAbi,
          typename ArgsAbi>
std::optional<EventRegistrationToken> AddTypedEventHandler(
    Interface* i,
    internal::IMemberFunction<
        Interface,
        ABI::Windows::Foundation::ITypedEventHandler<Sender*, Args*>*,
        EventRegistrationToken*> function,
    base::RepeatingCallback<void(SenderAbi*, ArgsAbi*)> callback) {
  EventRegistrationToken token;
  HRESULT hr = ((*i).*function)(
      Microsoft::WRL::Callback<
          ABI::Windows::Foundation::ITypedEventHandler<Sender*, Args*>>(
          [task_runner(base::SingleThreadTaskRunner::GetCurrentDefault()),
           callback(std::move(callback))](SenderAbi* sender, ArgsAbi* args) {
            // Make sure we are still on the same thread.
            DCHECK_EQ(base::SingleThreadTaskRunner::GetCurrentDefault(),
                      task_runner);
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(callback,
                               Microsoft::WRL::ComPtr<SenderAbi>(sender),
                               Microsoft::WRL::ComPtr<ArgsAbi>(args)));
            return S_OK;
          })
          .Get(),
      &token);

  if (FAILED(hr)) {
    DVLOG(2) << "Adding EventHandler failed: "
             << logging::SystemErrorCodeToString(hr);
    return std::nullopt;
  }

  return token;
}

}  // namespace device

#endif  // DEVICE_BLUETOOTH_EVENT_UTILS_WINRT_H_
