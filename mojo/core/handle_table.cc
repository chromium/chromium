// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
}

}  // namespace

HandleTable::EntriesAccessor::EntriesAccessor() = default;

HandleTable::EntriesAccessor::~EntriesAccessor() = default;

bool HandleTable::EntriesAccessor::Add(const MojoHandle handle, Entry entry) {
  return handles_.emplace(handle, std::move(entry)).second;
}

const scoped_refptr<Dispatcher>* HandleTable::EntriesAccessor::GetDispatcher(
    const MojoHandle handle) {
  if (last_read_handle_ != MOJO_HANDLE_INVALID && last_read_handle_ == handle) {
    return &last_read_dispatcher_;
  }
  const auto iter = handles_.find(handle);
  if (iter == handles_.end()) {
    return nullptr;
  }
  last_read_handle_ = handle;
  last_read_dispatcher_ = iter->second.dispatcher;
  return &last_read_dispatcher_;
}

HandleTable::Entry* HandleTable::EntriesAccessor::GetMutable(
    const MojoHandle handle) {
  const auto iter = handles_.find(handle);
  return iter == handles_.end() ? nullptr : &iter->second;
}

MojoResult HandleTable::EntriesAccessor::Remove(
    const MojoHandle handle,
    const HandleTable::EntriesAccessor::RemovalCondition removal_condition,
    scoped_refptr<Dispatcher>* dispatcher) {
  auto iter = handles_.find(handle);
  if (iter == handles_.end()) {
    return MOJO_RESULT_NOT_FOUND;
  }
  const bool is_busy = iter->second.busy;
  const bool remove_only_if_busy =
      removal_condition == RemovalCondition::kRemoveOnlyIfBusy;
  if (remove_only_if_busy == is_busy) {
    if (dispatcher != nullptr) {
      *dispatcher = iter->second.dispatcher;
    }
    if (iter->first == last_read_handle_) {
      last_read_handle_ = MOJO_HANDLE_INVALID;
      last_read_dispatcher_.reset();
    }
    handles_.erase(iter);
  }
  return is_busy ? MOJO_RESULT_BUSY : MOJO_RESULT_OK;
}

const std::unordered_map<MojoHandle, HandleTable::Entry>&
HandleTable::EntriesAccessor::GetUnderlyingMap() const {
  return handles_;
}

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
  const bool inserted = entries_.Add(handle, Entry(std::move(dispatcher)));
  DCHECK(inserted);

  return handle;
}

bool HandleTable::AddDispatchersFromTransit(
    const std::vector<Dispatcher::DispatcherInTransit>& dispatchers,
    MojoHandle* handles) {
  // Oops, we're out of handles.
  if (next_available_handle_ == MOJO_HANDLE_INVALID) {
    return false;
  }

  // MOJO_HANDLE_INVALID is zero.
  DCHECK_GE(next_available_handle_, 1u);

  // If this insertion would cause handle overflow, we're out of handles.
  const uintptr_t num_handles_available =
      std::numeric_limits<uintptr_t>::max() - next_available_handle_ + 1;
  if (num_handles_available < dispatchers.size()) {
    return false;
  }

  for (size_t i = 0; i < dispatchers.size(); ++i) {
    MojoHandle handle = MOJO_HANDLE_INVALID;
    if (dispatchers[i].dispatcher) {
      handle = next_available_handle_++;
      const bool inserted =
          entries_.Add(handle, Entry(dispatchers[i].dispatcher));
      DCHECK(inserted);
    }
    handles[i] = handle;
  }

  return true;
}

scoped_refptr<Dispatcher> HandleTable::GetDispatcher(MojoHandle handle) {
  const scoped_refptr<Dispatcher>* dispatcher = entries_.GetDispatcher(handle);
  return dispatcher == nullptr ? nullptr : *dispatcher;
}

MojoResult HandleTable::GetAndRemoveDispatcher(
    MojoHandle handle,
    scoped_refptr<Dispatcher>* dispatcher) {
  scoped_refptr<Dispatcher> removed_dispatcher;
  const MojoResult remove_result = entries_.Remove(
      handle, EntriesAccessor::RemovalCondition::kRemoveOnlyIfNotBusy,
      &removed_dispatcher);
  if (remove_result == MOJO_RESULT_NOT_FOUND) {
    return MOJO_RESULT_INVALID_ARGUMENT;
  }
  if (remove_result == MOJO_RESULT_BUSY) {
    return MOJO_RESULT_BUSY;
  }
  *dispatcher = std::move(removed_dispatcher);
  return MOJO_RESULT_OK;
}

MojoResult HandleTable::BeginTransit(
    const MojoHandle* handles,
    size_t num_handles,
    std::vector<Dispatcher::DispatcherInTransit>* dispatchers) {
  dispatchers->reserve(dispatchers->size() + num_handles);
  for (size_t i = 0; i < num_handles; ++i) {
    Entry* entry = entries_.GetMutable(handles[i]);
    if (entry == nullptr) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    }
    if (entry->busy) {
      return MOJO_RESULT_BUSY;
    }

    Dispatcher::DispatcherInTransit d;
    d.local_handle = handles[i];
    d.dispatcher = entry->dispatcher;
    if (!d.dispatcher->BeginTransit())
      return MOJO_RESULT_BUSY;
    entry->busy = true;
    dispatchers->push_back(d);
  }
  return MOJO_RESULT_OK;
}

void HandleTable::CompleteTransitAndClose(
    const std::vector<Dispatcher::DispatcherInTransit>& dispatchers) {
  for (const auto& dispatcher : dispatchers) {
    const MojoResult remove_result =
        entries_.Remove(dispatcher.local_handle,
                        EntriesAccessor::RemovalCondition::kRemoveOnlyIfBusy,
                        /*dispatcher=*/nullptr);
    DCHECK(remove_result == MOJO_RESULT_BUSY);
    dispatcher.dispatcher->CompleteTransitAndClose();
  }
}

void HandleTable::CancelTransit(
    const std::vector<Dispatcher::DispatcherInTransit>& dispatchers) {
  for (const auto& dispatcher : dispatchers) {
    Entry* entry = entries_.GetMutable(dispatcher.local_handle);
    DCHECK(entry != nullptr && entry->busy);
    entry->busy = false;
    dispatcher.dispatcher->CancelTransit();
  }
}

void HandleTable::GetActiveHandlesForTest(std::vector<MojoHandle>* handles) {
  handles->clear();
  for (const auto& entry : entries_.GetUnderlyingMap())
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
    for (const auto& entry : entries_.GetUnderlyingMap()) {
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

HandleTable::Entry::Entry(scoped_refptr<Dispatcher> dispatcher)
    : dispatcher(std::move(dispatcher)) {}

HandleTable::Entry::~Entry() = default;
HandleTable::Entry::Entry(const Entry& entry) = default;

}  // namespace core
}  // namespace mojo
