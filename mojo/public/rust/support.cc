// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The old Mojo SDK had a different API than Chromium's Mojo right now. The Rust
// bindings refer to several functions that no longer exist in the core C API.
// However, most of the functionality exists, albeit in a different form, in the
// C++ bindings. This file re-implements the old C API functions in terms of the
// new C++ helpers to ease the transition. In the long term, the Rust bindings
// must be updated properly for the changes to Mojo.

#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/wait.h"
#include "mojo/public/cpp/system/wait_set.h"

extern "C" {

// These functions support waiting on handle signal changes, e.g. to wait for
// a pipe to become readable. They used to be in the C API, but now the C API
// only exposes more primitive building blocks such as traps.
//
// We re-implement these functions in terms of the C++ API functions.

MojoResult MojoWait(MojoHandle handle,
                    MojoHandleSignals signals,
                    struct MojoHandleSignalsState* signals_state) {
  return mojo::Wait(mojo::Handle(handle), signals, signals_state);
}

MojoResult MojoWaitMany(const MojoHandle* raw_handles,
                        const MojoHandleSignals* signals,
                        std::size_t num_handles,
                        std::size_t* result_index,
                        MojoHandleSignalsState* signals_states) {
  std::vector<mojo::Handle> handles(num_handles);
  for (std::size_t i = 0; i < handles.size(); ++i)
    handles[i] = mojo::Handle(raw_handles[i]);
  return mojo::WaitMany(handles.data(), signals, num_handles, result_index,
                        signals_states);
}

namespace {

// These structs no longer exist in Mojo.
struct MojoCreateWaitSetOptions;
struct MojoWaitSetAddOptions;

struct MOJO_ALIGNAS(8) MojoWaitSetResult {
  std::uint64_t cookie;
  MojoResult wait_result;
  std::uint32_t reserved;
  struct MojoHandleSignalsState signals_state;
};

struct WaitSetAdapter {
  mojo::WaitSet wait_set;
  std::unordered_map<MojoHandle, std::uint64_t> handle_to_cookie;
  std::unordered_map<std::uint64_t, MojoHandle> cookie_to_handle;
};

}  // namespace

// Similar to the above, wait sets could wait for one of several handles to be
// signalled. Unlike WaitMany they maintained a set of handles in their state.
// They are long gone as first-class objects of the Mojo API. Previously they
// were owned through MojoHandle just like pipes and buffers. Here this is
// changed to support casting between wait set handles and pointers.
//
// Reimplementing these was a bit more complex since the new C++ API is similar
// but different in many respects. For example, the old API referred to handles
// in the set by 64-bit integer handles. The C++ WaitSet does not, so we need to
// maintain maps in both directions.
MojoResult MojoCreateWaitSet(const MojoCreateWaitSetOptions*,
                             std::size_t* handle) {
  auto adapter = std::make_unique<WaitSetAdapter>();
  *handle = reinterpret_cast<std::size_t>(adapter.release());
  return MOJO_RESULT_OK;
}

MojoResult MojoWaitSetAdd(std::size_t wait_set_handle,
                          MojoHandle handle,
                          MojoHandleSignals signals,
                          std::uint64_t cookie,
                          const MojoWaitSetAddOptions*) {
  auto* adapter = reinterpret_cast<WaitSetAdapter*>(wait_set_handle);
  MojoResult result =
      adapter->wait_set.AddHandle(mojo::Handle(handle), signals);
  if (result != MOJO_RESULT_OK)
    return result;
  adapter->handle_to_cookie[handle] = cookie;
  adapter->cookie_to_handle[cookie] = handle;
  return MOJO_RESULT_OK;
}

MojoResult MojoWaitSetRemove(std::size_t wait_set_handle,
                             std::uint64_t cookie) {
  auto* adapter = reinterpret_cast<WaitSetAdapter*>(wait_set_handle);
  auto handle_it = adapter->cookie_to_handle.find(cookie);
  if (handle_it == adapter->cookie_to_handle.end())
    return MOJO_RESULT_NOT_FOUND;

  MojoResult result =
      adapter->wait_set.RemoveHandle(mojo::Handle(handle_it->second));
  if (result != MOJO_RESULT_OK)
    return result;

  adapter->handle_to_cookie.erase(handle_it->second);
  adapter->cookie_to_handle.erase(handle_it);
  return MOJO_RESULT_OK;
}

MojoResult MojoWaitSetWait(std::size_t wait_set_handle,
                           std::uint32_t* num_results,
                           MojoWaitSetResult* results) {
  std::size_t max_results = *num_results;
  std::vector<mojo::Handle> ready_handles(max_results);
  std::vector<MojoResult> ready_results(max_results);
  std::vector<MojoHandleSignalsState> signals_states(max_results);

  std::size_t num_ready_handles = max_results;

  auto* adapter = reinterpret_cast<WaitSetAdapter*>(wait_set_handle);
  adapter->wait_set.Wait(nullptr, &num_ready_handles, ready_handles.data(),
                         ready_results.data(), signals_states.data());

  for (std::size_t i = 0; i < num_ready_handles; ++i) {
    results[i].cookie = adapter->handle_to_cookie.at(ready_handles[i].value());
    results[i].wait_result = ready_results[i];
    results[i].reserved = 0;
    results[i].signals_state = signals_states[i];
  }

  *num_results = num_ready_handles;

  return MOJO_RESULT_OK;
}

}  // extern "C"
