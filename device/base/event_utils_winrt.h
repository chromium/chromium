// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BASE_EVENT_UTILS_WINRT_H_
#define DEVICE_BASE_EVENT_UTILS_WINRT_H_

#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include <ios>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/windows_types.h"

namespace device {

namespace internal {

template <typename Interface, typename... Args>
using IMemberFunction = HRESULT (__stdcall Interface::*)(Args...);

}  // namespace internal

// Convenience template function to construct an IEventHandler from a
// RepeatingCallback of a matching signature. In case of success, the
// EventRegistrationToken is returned to the caller. A return value of
// nullopt indicates a failure. Events will be posted to the same sequence
// the event handler was created on.
template <typename Interface,
          typename Args,
          typename SenderAbi,
          typename ArgsAbi>
std::optional<EventRegistrationToken> AddEventHandler(
    Interface* interface_called,
    internal::IMemberFunction<Interface,
                              ABI::Windows::Foundation::IEventHandler<Args*>*,
                              EventRegistrationToken*> function,
    base::RepeatingCallback<void(SenderAbi*, ArgsAbi*)> callback) {
  EventRegistrationToken token;
  HRESULT hr = ((*interface_called).*function)(
      Microsoft::WRL::Callback<ABI::Windows::Foundation::IEventHandler<Args*>>(
          [task_runner = base::SequencedTaskRunner::GetCurrentDefault(),
           callback = std::move(callback)](SenderAbi* sender, ArgsAbi* args) {
            task_runner->PostTask(
                FROM_HERE,
                BindOnce(callback, Microsoft::WRL::ComPtr<SenderAbi>(sender),
                         Microsoft::WRL::ComPtr<ArgsAbi>(args)));
            return S_OK;
          })
          .Get(),
      &token);
  if (FAILED(hr)) {
    DVLOG(2) << "Adding EventHandler failed: "
             << "0x" << std::hex << hr;
    return std::nullopt;
  }
  return token;
}

}  // namespace device

#endif  // DEVICE_BASE_EVENT_UTILS_WINRT_H_
