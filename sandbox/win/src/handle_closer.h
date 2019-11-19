// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_HANDLE_CLOSER_H_
#define SANDBOX_SRC_HANDLE_CLOSER_H_

#include <stddef.h>

#include <map>
#include <set>

#include <string>

#include "base/macros.h"
#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/target_process.h"

namespace sandbox {

// This is a map of handle-types to names that we need to close in the
// target process. A null set means we need to close all handles of the
// given type.
typedef std::map<const std::wstring, std::set<std::wstring>> HandleMap;

// Type and set of corresponding handle names to close.
struct HandleListEntry {
  size_t record_bytes;     // Rounded to sizeof(size_t) bytes.
  size_t offset_to_names;  // Nul terminated strings of name_count names.
  size_t name_count;
  wchar_t handle_type[1];
};

// Global parameters and a pointer to the list of entries.
struct HandleCloserInfo {
  size_t record_bytes;  // Rounded to sizeof(size_t) bytes.
  size_t num_handle_types;
  struct HandleListEntry handle_entries[1];
};

SANDBOX_INTERCEPT HandleCloserInfo* g_handle_closer_info;

// Adds handles to close after lockdown.
class HandleCloser {
 public:
  HandleCloser();
  ~HandleCloser();

  // Adds a handle that will be closed in the target process after lockdown.
  // A nullptr value for handle_name indicates all handles of the specified
  // type. An empty string for handle_name indicates the handle is unnamed.
  ResultCode AddHandle(const wchar_t* handle_type, const wchar_t* handle_name);

  // Serializes and copies the closer table into the target process.
  bool InitializeTargetHandles(TargetProcess* target);

 private:
  // Calculates the memory needed to copy the serialized handles list (rounded
  // to the nearest machine-word size).
  size_t GetBufferSize();

  // Serializes the handle list into the target process.
  bool SetupHandleList(void* buffer, size_t buffer_bytes);

  HandleMap handles_to_close_;

  DISALLOW_COPY_AND_ASSIGN(HandleCloser);
};

// Returns the object manager's name associated with a handle
bool GetHandleName(HANDLE handle, std::wstring* handle_name);

}  // namespace sandbox

#endif  // SANDBOX_SRC_HANDLE_CLOSER_H_
