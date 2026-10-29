// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_UTIL_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_UTIL_H_

// Generic, product-agnostic Windows registry helpers and primitive `WorkItem`
// building blocks for the Windows host installer.
//
// What belongs here:
// - Reusable registry helper functions (e.g., creating a key and setting its
//   default value, formatting GUIDs).
// - Generic, reusable `WorkItem` subclasses for registry operations not already
//   provided by `//chrome/installer/util:work_item` (e.g., setting `REG_BINARY`
//   values or creating DACL-protected registry keys with rollback support).
//
// What does NOT belong here:
// - Chrome Remote Desktop product-specific registry schemas, key paths, GUIDs,
//   or feature registrations (e.g., Native Messaging, EventLog, COM server,
//   URL forwarder, Omaha). Put those in `registry_work_items.{h,cc}`.

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/installer/util/work_item.h"

class WorkItemList;

namespace remoting::installer {

// Both 32-bit and 64-bit registry views (`KEY_WOW64_32KEY` and
// `KEY_WOW64_64KEY`).
inline constexpr std::array<REGSAM, 2> kBothRegistryViews = {KEY_WOW64_32KEY,
                                                             KEY_WOW64_64KEY};

// A WorkItem that writes a `REG_BINARY` value to an existing registry key and
// restores the previous value (or deletes the newly created value) on rollback.
class SetRegBinaryValueWorkItem : public WorkItem {
 public:
  SetRegBinaryValueWorkItem(HKEY predefined_root,
                            std::wstring key_path,
                            REGSAM wow64_access,
                            std::wstring value_name,
                            base::span<const uint8_t> value_data);
  SetRegBinaryValueWorkItem(const SetRegBinaryValueWorkItem&) = delete;
  SetRegBinaryValueWorkItem& operator=(const SetRegBinaryValueWorkItem&) =
      delete;
  ~SetRegBinaryValueWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;

  HKEY predefined_root_;
  std::wstring key_path_;
  REGSAM wow64_access_;
  std::wstring value_name_;
  std::vector<uint8_t> value_;
  bool wrote_value_ = false;
  bool had_previous_value_ = false;
  DWORD previous_type_ = 0;
  std::vector<uint8_t> previous_value_;
};

// A WorkItem that creates or opens a registry key and applies a protected DACL
// parsed from `sddl`. On rollback, deletes the key only if it was newly created
// by `Do()` and contains no values or subkeys.
class CreateProtectedRegKeyWorkItem : public WorkItem {
 public:
  CreateProtectedRegKeyWorkItem(HKEY predefined_root,
                                std::wstring key_path,
                                REGSAM wow64_access,
                                std::string sddl);
  CreateProtectedRegKeyWorkItem(const CreateProtectedRegKeyWorkItem&) = delete;
  CreateProtectedRegKeyWorkItem& operator=(
      const CreateProtectedRegKeyWorkItem&) = delete;
  ~CreateProtectedRegKeyWorkItem() override;

 private:
  bool DoImpl() override;
  void RollbackImpl() override;

  HKEY predefined_root_;
  std::wstring key_path_;
  REGSAM wow64_access_;
  std::string sddl_;
  base::win::RegKey key_;
  bool key_created_ = false;
};

// Appends WorkItems to `list` that create `key_path` under `predefined_root`
// and set its unnamed default string value to `default_value`.
void AddCreateKeyAndSetDefaultValueWorkItems(
    WorkItemList* list,
    HKEY predefined_root,
    const std::wstring& key_path,
    const std::wstring& default_value,
    REGSAM wow64_access = WorkItem::kWow64Default);

// Formats `clsid` as a wide string in `{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}`
// format.
std::wstring FormatClsidString(const CLSID& clsid);

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_REGISTRY_UTIL_H_
