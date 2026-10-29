// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generic, product-agnostic Windows registry helpers and primitive `WorkItem`
// building blocks for the Windows host installer.
//
// See `registry_util.h` for the boundary between generic registry primitives
// (which belong here) and Chrome Remote Desktop product-specific registry
// registrations (which belong in `registry_work_items.{h,cc}`).

#include "remoting/host/installer/win/setup/registry_util.h"

#include <windows.h>

#include <aclapi.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "remoting/host/win/security_descriptor.h"

namespace remoting::installer {

SetRegBinaryValueWorkItem::SetRegBinaryValueWorkItem(
    HKEY predefined_root,
    std::wstring key_path,
    REGSAM wow64_access,
    std::wstring value_name,
    base::span<const uint8_t> value_data)
    : predefined_root_(predefined_root),
      key_path_(std::move(key_path)),
      wow64_access_(wow64_access),
      value_name_(std::move(value_name)),
      value_(value_data.begin(), value_data.end()) {}

SetRegBinaryValueWorkItem::~SetRegBinaryValueWorkItem() = default;

bool SetRegBinaryValueWorkItem::DoImpl() {
  base::win::RegKey key;
  LONG result = key.Open(predefined_root_, key_path_.c_str(),
                         KEY_READ | KEY_SET_VALUE | wow64_access_);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to open key " << key_path_ << ": " << result;
    return false;
  }

  DWORD size = 0;
  DWORD type = 0;
  result = key.ReadValue(value_name_.c_str(), nullptr, &size, &type);
  if (result == ERROR_SUCCESS && rollback_enabled()) {
    had_previous_value_ = true;
    previous_type_ = type;
    if (size > 0) {
      previous_value_.resize(size);
      if (key.ReadValue(value_name_.c_str(), previous_value_.data(), &size,
                        &previous_type_) != ERROR_SUCCESS) {
        previous_value_.clear();
        had_previous_value_ = false;
      }
    }
  }

  result = key.WriteValue(value_name_.c_str(), value_.data(),
                          static_cast<DWORD>(value_.size()), REG_BINARY);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to write binary value " << value_name_ << " to "
               << key_path_ << ": " << result;
    return false;
  }

  wrote_value_ = true;
  return true;
}

void SetRegBinaryValueWorkItem::RollbackImpl() {
  if (!wrote_value_) {
    return;
  }
  base::win::RegKey key;
  if (key.Open(predefined_root_, key_path_.c_str(),
               KEY_SET_VALUE | wow64_access_) != ERROR_SUCCESS) {
    return;
  }
  if (had_previous_value_) {
    key.WriteValue(value_name_.c_str(),
                   previous_value_.empty() ? nullptr : previous_value_.data(),
                   static_cast<DWORD>(previous_value_.size()), previous_type_);
  } else {
    key.DeleteValue(value_name_.c_str());
  }
}

CreateProtectedRegKeyWorkItem::CreateProtectedRegKeyWorkItem(
    HKEY predefined_root,
    std::wstring key_path,
    REGSAM wow64_access,
    std::string sddl)
    : predefined_root_(predefined_root),
      key_path_(std::move(key_path)),
      wow64_access_(wow64_access),
      sddl_(std::move(sddl)) {}

CreateProtectedRegKeyWorkItem::~CreateProtectedRegKeyWorkItem() = default;

bool CreateProtectedRegKeyWorkItem::DoImpl() {
  ScopedSd sd = ConvertSddlToSd(sddl_);
  if (!sd) {
    PLOG(ERROR) << "Failed to convert SDDL for key " << key_path_;
    return false;
  }

  SECURITY_ATTRIBUTES sa = {sizeof(sa), sd.get(), FALSE};
  HKEY raw_key = nullptr;
  DWORD disposition = 0;
  LONG result =
      ::RegCreateKeyExW(predefined_root_, key_path_.c_str(), /*Reserved=*/0,
                        /*lpClass=*/nullptr, REG_OPTION_NON_VOLATILE,
                        KEY_SET_VALUE | KEY_QUERY_VALUE | WRITE_DAC |
                            READ_CONTROL | DELETE | wow64_access_,
                        &sa, &raw_key, &disposition);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to create protected key " << key_path_ << ": "
               << result;
    return false;
  }
  key_.Set(raw_key);
  key_created_ = (disposition == REG_CREATED_NEW_KEY);

  PACL dacl = nullptr;
  BOOL dacl_present = FALSE;
  BOOL dacl_defaulted = FALSE;
  if (!::GetSecurityDescriptorDacl(sd.get(), &dacl_present, &dacl,
                                   &dacl_defaulted) ||
      !dacl_present) {
    PLOG(ERROR) << "Failed to extract DACL for key " << key_path_;
    return false;
  }

  DWORD sec_result = ::SetSecurityInfo(
      key_.Handle(), SE_REGISTRY_KEY,
      DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
      /*psidOwner=*/nullptr, /*psidGroup=*/nullptr, dacl,
      /*pSacl=*/nullptr);
  if (sec_result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set DACL on key " << key_path_ << ": "
               << sec_result;
    return false;
  }

  return true;
}

void CreateProtectedRegKeyWorkItem::RollbackImpl() {
  if (!key_created_ || !key_.Valid()) {
    return;
  }
  DWORD subkey_count = 0;
  DWORD value_count = 0;
  if (::RegQueryInfoKeyW(key_.Handle(), nullptr, nullptr, nullptr,
                         &subkey_count, nullptr, nullptr, &value_count, nullptr,
                         nullptr, nullptr, nullptr) == ERROR_SUCCESS &&
      subkey_count == 0 && value_count == 0) {
    ::RegDeleteKeyExW(key_.Handle(), L"", wow64_access_, 0);
  }
  key_.Close();
  key_created_ = false;
}

void AddCreateKeyAndSetDefaultValueWorkItems(WorkItemList* list,
                                             HKEY predefined_root,
                                             const std::wstring& key_path,
                                             const std::wstring& default_value,
                                             REGSAM wow64_access) {
  CHECK(list);
  list->AddCreateRegKeyWorkItem(predefined_root, key_path, wow64_access);
  list->AddSetRegValueWorkItem(predefined_root, key_path, wow64_access, L"",
                               default_value, /*overwrite=*/true);
}

std::wstring FormatClsidString(const CLSID& clsid) {
  return base::win::WStringFromGUID(clsid);
}

}  // namespace remoting::installer
