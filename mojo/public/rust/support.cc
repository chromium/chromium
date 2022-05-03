// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The old Mojo SDK had a different API than Chromium's Mojo right now. The Rust
// bindings refer to several functions that no longer exist in the core C API.
// However, most of the functionality exists, albeit in a different form, in the
// C++ bindings. This file re-implements the old C API functions in terms of the
// new C++ helpers to ease the transition. In the long term, the Rust bindings
// must be updated properly for the changes to Mojo.

#include "mojo/public/rust/support.h"

#include <stdint.h>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/wait.h"
#include "mojo/public/cpp/system/wait_set.h"

namespace {

// Adapter used to reimplement the C WaitSet in terms of the C++ mojo::WaitSet.
struct WaitSetAdapter {
  mojo::WaitSet wait_set;
  std::unordered_map<MojoHandle, std::uint64_t> handle_to_cookie;
  std::unordered_map<std::uint64_t, MojoHandle> cookie_to_handle;
};

}  // namespace

extern "C" {

MojoResult MojoCreateWaitSet(const MojoCreateWaitSetOptions*,
                             MojoWaitSetHandle* handle) {
  auto adapter = std::make_unique<WaitSetAdapter>();
  *handle = reinterpret_cast<std::size_t>(adapter.release());
  return MOJO_RESULT_OK;
}

MojoResult MojoWaitSetAdd(MojoWaitSetHandle wait_set_handle,
                          MojoHandle handle,
                          MojoHandleSignals signals,
                          uint64_t cookie,
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

MojoResult MojoWaitSetRemove(MojoWaitSetHandle wait_set_handle,
                             uint64_t cookie) {
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

MojoResult MojoWaitSetWait(MojoWaitSetHandle wait_set_handle,
                           uint32_t* num_results,
                           MojoWaitSetResult* results) {
  size_t max_results = *num_results;
  std::vector<mojo::Handle> ready_handles(max_results);
  std::vector<MojoResult> ready_results(max_results);
  std::vector<MojoHandleSignalsState> signals_states(max_results);

  size_t num_ready_handles = max_results;

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
