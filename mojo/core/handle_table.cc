// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/handle_table.h"

#include <stdint.h>

#include <limits>

#include "base/trace_event/memory_dump_manager.h"

namespace mojo {
namespace core {

namespace {

const char* GetNameForDispatcherType(Dispatcher::Type type) {
  switch (type) {
    case Dispatcher::Type::UNKNOWN:
      return "unknown";
    case Dispatcher::Type::MESSAGE_PIPE:
      return "message_pipe";
    case Dispatcher::Type::DATA_PIPE_PRODUCER:
      return "data_pipe_producer";
    case Dispatcher::Type::DATA_PIPE_CONSUMER:
      return "data_pipe_consumer";
    case Dispatcher::Type::SHARED_BUFFER:
      return "shared_buffer";
    case Dispatcher::Type::WATCHER:
      return "watcher";
    case Dispatcher::Type::PLATFORM_HANDLE:
      return "platform_handle";
    case Dispatcher::Type::INVITATION:
      return "invitation";
  }
  NOTREACHED();
  return "unknown";
}

}  // namespace

HandleTable::HandleTable() = default;

HandleTable::~HandleTable() = default;

base::Lock& HandleTable::GetLock() {
  return lock_;
}

MojoHandle HandleTable::AddDispatcher(scoped_refptr<Dispatcher> dispatcher) {
  // Oops, we're out of handles.
  if (next_available_handle_ == MOJO_HANDLE_INVALID)
    return MOJO_HANDLE_INVALID;

  MojoHandle handle = next_available_handle_++;
  auto result =
      handles_.insert(std::make_pair(handle, Entry(std::move(dispatcher))));
  DCHECK(result.second);

  return handle;
}

bool HandleTable::AddDispatchersFromTransit(
    const std::vector<Dispatcher::DispatcherInTransit>& dispatchers,
    MojoHandle* handles) {
  // Oops, we're out of handles.
  if (next_available_handle_ == MOJO_HANDLE_INVALID)
    return false;

  DCHECK_LE(dispatchers.size(), std::numeric_limits<uint32_t>::max());
  // If this insertion would cause handle overflow, we're out of handles.
  if (next_available_handle_ + dispatchers.size() < next_available_handle_)
    return false;

  for (size_t i = 0; i < dispatchers.size(); ++i) {
    MojoHandle handle = MOJO_HANDLE_INVALID;
    if (dispatchers[i].dispatcher) {
      handle = next_available_handle_++;
      auto result = handles_.insert(
          std::make_pair(handle, Entry(dispatchers[i].dispatcher)));
      DCHECK(result.second);
    }
    handles[i] = handle;
  }

  return true;
}

scoped_refptr<Dispatcher> HandleTable::GetDispatcher(MojoHandle handle) const {
  auto it = handles_.find(handle);
  if (it == handles_.end())
    return nullptr;
  return it->second.dispatcher;
}

MojoResult HandleTable::GetAndRemoveDispatcher(
    MojoHandle handle,
    scoped_refptr<Dispatcher>* dispatcher) {
  auto it = handles_.find(handle);
  if (it == handles_.end())
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (it->second.busy)
    return MOJO_RESULT_BUSY;

  *dispatcher = std::move(it->second.dispatcher);
  handles_.erase(it);
  return MOJO_RESULT_OK;
}

MojoResult HandleTable::BeginTransit(
    const MojoHandle* handles,
    size_t num_handles,
    std::vector<Dispatcher::DispatcherInTransit>* dispatchers) {
  dispatchers->reserve(dispatchers->size() + num_handles);
  for (size_t i = 0; i < num_handles; ++i) {
    auto it = handles_.find(handles[i]);
    if (it == handles_.end())
      return MOJO_RESULT_INVALID_ARGUMENT;
    if (it->second.busy)
      return MOJO_RESULT_BUSY;

    Dispatcher::DispatcherInTransit d;
    d.local_handle = handles[i];
    d.dispatcher = it->second.dispatcher;
    if (!d.dispatcher->BeginTransit())
      return MOJO_RESULT_BUSY;
    it->second.busy = true;
    dispatchers->push_back(d);
  }
  return MOJO_RESULT_OK;
}

void HandleTable::CompleteTransitAndClose(
    const std::vector<Dispatcher::DispatcherInTransit>& dispatchers) {
  for (const auto& dispatcher : dispatchers) {
    auto it = handles_.find(dispatcher.local_handle);
    DCHECK(it != handles_.end() && it->second.busy);
    handles_.erase(it);
    dispatcher.dispatcher->CompleteTransitAndClose();
  }
}

void HandleTable::CancelTransit(
    const std::vector<Dispatcher::DispatcherInTransit>& dispatchers) {
  for (const auto& dispatcher : dispatchers) {
    auto it = handles_.find(dispatcher.local_handle);
    DCHECK(it != handles_.end() && it->second.busy);
    it->second.busy = false;
    dispatcher.dispatcher->CancelTransit();
  }
}

void HandleTable::GetActiveHandlesForTest(std::vector<MojoHandle>* handles) {
  handles->clear();
  for (const auto& entry : handles_)
    handles->push_back(entry.first);
}

// MemoryDumpProvider implementation.
bool HandleTable::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                               base::trace_event::ProcessMemoryDump* pmd) {
  // Create entries for all relevant dispatcher types to ensure they are present
  // in the final dump.
  std::map<Dispatcher::Type, int> handle_count;
  handle_count[Dispatcher::Type::MESSAGE_PIPE];
  handle_count[Dispatcher::Type::DATA_PIPE_PRODUCER];
  handle_count[Dispatcher::Type::DATA_PIPE_CONSUMER];
  handle_count[Dispatcher::Type::SHARED_BUFFER];
  handle_count[Dispatcher::Type::WATCHER];
  handle_count[Dispatcher::Type::PLATFORM_HANDLE];
  handle_count[Dispatcher::Type::INVITATION];

  // Count the number of each dispatcher type.
  {
    base::AutoLock lock(GetLock());
    for (const auto& entry : handles_) {
      ++handle_count[entry.second.dispatcher->GetType()];
    }
  }

  for (const auto& entry : handle_count) {
    base::trace_event::MemoryAllocatorDump* inner_dump =
        pmd->CreateAllocatorDump(std::string("mojo/") +
                                 GetNameForDispatcherType(entry.first));
    inner_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameObjectCount,
        base::trace_event::MemoryAllocatorDump::kUnitsObjects, entry.second);
  }

  return true;
}

HandleTable::Entry::Entry() = default;

HandleTable::Entry::Entry(scoped_refptr<Dispatcher> dispatcher)
    : dispatcher(std::move(dispatcher)) {}

HandleTable::Entry::Entry(const Entry& other) = default;

HandleTable::Entry::~Entry() = default;

}  // namespace core
}  // namespace mojo
