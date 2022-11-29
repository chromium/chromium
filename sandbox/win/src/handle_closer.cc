// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/handle_closer.h"

#include <stddef.h>

#include <memory>

#include "base/check_op.h"
#include "base/memory/free_deleter.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "sandbox/win/src/win_utils.h"

namespace {

template <typename T>
T RoundUpToWordSize(T v) {
  if (size_t mod = v % sizeof(size_t))
    v += sizeof(size_t) - mod;
  return v;
}

template <typename T>
T* RoundUpToWordSize(T* v) {
  return reinterpret_cast<T*>(RoundUpToWordSize(reinterpret_cast<size_t>(v)));
}

}  // namespace

namespace sandbox {

// Memory buffer mapped from the parent, with the list of handles.
SANDBOX_INTERCEPT HandleCloserInfo* g_handles_to_close;

HandleCloser::HandleCloser() {}

HandleCloser::~HandleCloser() {}

ResultCode HandleCloser::AddHandle(const wchar_t* handle_type,
                                   const wchar_t* handle_name) {
  if (!handle_type)
    return SBOX_ERROR_BAD_PARAMS;

  // Cannot call AddHandle if the cache has been initialized.
  DCHECK(serialized_map_.empty());
  if (!serialized_map_.empty())
    return SBOX_ERROR_UNEXPECTED_CALL;

  std::wstring resolved_name;
  if (handle_name) {
    if (handle_type == std::wstring(L"Key")) {
      auto resolved_reg_path = ResolveRegistryName(handle_name);
      if (!resolved_reg_path)
        return SBOX_ERROR_BAD_PARAMS;
      resolved_name = resolved_reg_path.value();
    } else {
      resolved_name = handle_name;
    }
  }

  HandleMap::iterator names = handles_to_close_.find(handle_type);
  if (names == handles_to_close_.end()) {  // We have no entries for this type.
    std::pair<HandleMap::iterator, bool> result = handles_to_close_.insert(
        HandleMap::value_type(handle_type, HandleMap::mapped_type()));
    names = result.first;
    if (handle_name)
      names->second.insert(resolved_name);
  } else if (!handle_name) {  // Now we need to close all handles of this type.
    names->second.clear();
  } else if (!names->second.empty()) {  // Add another name for this type.
    names->second.insert(resolved_name);
  }  // If we're already closing all handles of type then we're done.

  return SBOX_ALL_OK;
}

size_t HandleCloser::GetBufferSize() {
  size_t bytes_total = offsetof(HandleCloserInfo, handle_entries);

  for (HandleMap::iterator i = handles_to_close_.begin();
       i != handles_to_close_.end(); ++i) {
    size_t bytes_entry = offsetof(HandleListEntry, handle_type) +
                         (i->first.size() + 1) * sizeof(wchar_t);
    for (HandleMap::mapped_type::iterator j = i->second.begin();
         j != i->second.end(); ++j) {
      bytes_entry += ((*j).size() + 1) * sizeof(wchar_t);
    }

    // Round up to the nearest multiple of word size.
    bytes_entry = RoundUpToWordSize(bytes_entry);
    bytes_total += bytes_entry;
  }

  return bytes_total;
}

bool HandleCloser::InitializeTargetHandles(TargetProcess& target) {
  // Do nothing on an empty list (global pointer already initialized to
  // nullptr).
  if (handles_to_close_.empty())
    return true;

  // Cache the serialized form as we might be used by shared configs.
  if (serialized_map_.empty()) {
    size_t bytes_needed = GetBufferSize();
    serialized_map_.resize(bytes_needed);

    if (!SetupHandleList(serialized_map_.data(), bytes_needed))
      return false;
  }

  void* remote_data;
  if (!CopyToChildMemory(target.Process(), serialized_map_.data(),
                         serialized_map_.size(), &remote_data)) {
    return false;
  }

  g_handles_to_close = reinterpret_cast<HandleCloserInfo*>(remote_data);

  ResultCode rc = target.TransferVariable(
      "g_handles_to_close", &g_handles_to_close, sizeof(g_handles_to_close));

  return (SBOX_ALL_OK == rc);
}

bool HandleCloser::SetupHandleList(void* buffer, size_t buffer_bytes) {
  ::ZeroMemory(buffer, buffer_bytes);
  HandleCloserInfo* handle_info = reinterpret_cast<HandleCloserInfo*>(buffer);
  handle_info->record_bytes = buffer_bytes;
  handle_info->num_handle_types = handles_to_close_.size();

  wchar_t* output = reinterpret_cast<wchar_t*>(&handle_info->handle_entries[0]);
  wchar_t* end = reinterpret_cast<wchar_t*>(reinterpret_cast<char*>(buffer) +
                                            buffer_bytes);
  for (HandleMap::iterator i = handles_to_close_.begin();
       i != handles_to_close_.end(); ++i) {
    if (output >= end)
      return false;
    HandleListEntry* list_entry = reinterpret_cast<HandleListEntry*>(output);
    output = &list_entry->handle_type[0];

    // Copy the typename and set the offset and count.
    i->first.copy(output, i->first.size());
    *(output += i->first.size()) = L'\0';
    output++;
    list_entry->offset_to_names = base::checked_cast<size_t>(
        reinterpret_cast<char*>(output) - reinterpret_cast<char*>(list_entry));
    list_entry->name_count = i->second.size();

    // Copy the handle names.
    for (HandleMap::mapped_type::iterator j = i->second.begin();
         j != i->second.end(); ++j) {
      output = base::ranges::copy(*j, output) + 1;
    }

    // Round up to the nearest multiple of sizeof(size_t).
    output = RoundUpToWordSize(output);
    list_entry->record_bytes = static_cast<size_t>(
        reinterpret_cast<char*>(output) - reinterpret_cast<char*>(list_entry));
  }

  DCHECK_EQ(reinterpret_cast<size_t>(output), reinterpret_cast<size_t>(end));
  return output <= end;
}

}  // namespace sandbox
