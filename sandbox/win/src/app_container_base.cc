// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/app_container_base.h"

#include <memory>
#include <utility>

#include <windows.h>

#include <userenv.h>

#include "base/win/security_descriptor.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {

struct FreeSidDeleter {
  inline void operator()(void* ptr) const { ::FreeSid(ptr); }
};

}  // namespace

// static
AppContainerBase* AppContainerBase::CreateProfile(const wchar_t* package_name,
                                                  const wchar_t* display_name,
                                                  const wchar_t* description) {
  PSID package_sid_ptr = nullptr;
  HRESULT hr = ::CreateAppContainerProfile(
      package_name, display_name, description, nullptr, 0, &package_sid_ptr);
  if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))
    return Open(package_name);

  if (FAILED(hr))
    return nullptr;
  std::unique_ptr<void, FreeSidDeleter> sid_deleter(package_sid_ptr);
  auto package_sid = base::win::Sid::FromPSID(package_sid_ptr);
  if (!package_sid)
    return nullptr;
  return new AppContainerBase(*package_sid, AppContainerType::kProfile);
}

// static
AppContainerBase* AppContainerBase::Open(const wchar_t* package_name) {
  PSID package_sid_ptr = nullptr;
  HRESULT hr = ::DeriveAppContainerSidFromAppContainerName(package_name,
                                                           &package_sid_ptr);
  if (FAILED(hr))
    return nullptr;

  std::unique_ptr<void, FreeSidDeleter> sid_deleter(package_sid_ptr);
  auto package_sid = base::win::Sid::FromPSID(package_sid_ptr);
  if (!package_sid)
    return nullptr;
  return new AppContainerBase(*package_sid, AppContainerType::kDerived);
}

// static
AppContainerBase* AppContainerBase::CreateLowbox(const wchar_t* sid) {
  auto package_sid = base::win::Sid::FromSddlString(sid);
  if (!package_sid)
    return nullptr;

  return new AppContainerBase(*package_sid, AppContainerType::kLowbox);
}

// static
bool AppContainerBase::Delete(const wchar_t* package_name) {
  return SUCCEEDED(::DeleteAppContainerProfile(package_name));
}

AppContainerBase::AppContainerBase(base::win::Sid& package_sid,
                                   AppContainerType type)
    : ref_count_(0),
      package_sid_(std::move(package_sid)),
      enable_low_privilege_app_container_(false),
      type_(type) {}

AppContainerBase::~AppContainerBase() {}

void AppContainerBase::AddRef() {
  // ref_count starts at 0 for this class so can increase from 0 (once).
  CHECK(::InterlockedIncrement(&ref_count_) > 0);
}

void AppContainerBase::Release() {
  LONG result = ::InterlockedDecrement(&ref_count_);
  CHECK(result >= 0);
  if (result == 0) {
    delete this;
  }
}

bool AppContainerBase::AccessCheck(const wchar_t* object_name,
                                   base::win::SecurityObjectType object_type,
                                   DWORD desired_access,
                                   DWORD* granted_access,
                                   BOOL* access_status) {
  if (object_type != base::win::SecurityObjectType::kFile &&
      object_type != base::win::SecurityObjectType::kRegistry) {
    ::SetLastError(ERROR_INVALID_PARAMETER);
    return false;
  }

  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromName(
          object_name, object_type,
          OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
              DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION);
  if (!sd) {
    return false;
  }

  if (enable_low_privilege_app_container_) {
    // We can't create a LPAC token directly, so modify the DACL to simulate it.
    // Remove any ACEs for all application package SID.
    if (!sd->SetDaclEntry(base::win::WellKnownSid::kAllApplicationPackages,
                          base::win::SecurityAccessMode::kRevoke, 0, 0)) {
      return false;
    }
  }

  std::optional<base::win::AccessToken> primary =
      base::win::AccessToken::FromCurrentProcess(
          /*impersonation=*/false, TOKEN_DUPLICATE);
  if (!primary.has_value()) {
    return false;
  }
  std::optional<base::win::AccessToken> lowbox = BuildPrimaryToken(*primary);
  if (!lowbox) {
    return false;
  }
  std::optional<base::win::AccessToken> token_query =
      lowbox->DuplicateImpersonation(
          base::win::SecurityImpersonationLevel::kIdentification);
  if (!token_query) {
    return false;
  }

  std::optional<base::win::AccessCheckResult> result =
      sd->AccessCheck(*token_query, desired_access, object_type);
  if (!result) {
    return false;
  }
  *granted_access = result->granted_access;
  *access_status = result->access_status;
  return true;
}

void AppContainerBase::AddCapability(const wchar_t* capability_name) {
  AddCapability(base::win::Sid::FromNamedCapability(capability_name), false);
}

void AppContainerBase::AddCapability(
    base::win::WellKnownCapability capability) {
  AddCapability(base::win::Sid::FromKnownCapability(capability), false);
}

bool AppContainerBase::AddCapabilitySddl(const wchar_t* sddl_sid) {
  return AddCapability(base::win::Sid::FromSddlString(sddl_sid), false);
}

bool AppContainerBase::AddCapability(
    const std::optional<base::win::Sid>& capability_sid,
    bool impersonation_only) {
  if (!capability_sid)
    return false;
  if (!impersonation_only)
    capabilities_.push_back(capability_sid->Clone());
  impersonation_capabilities_.push_back(capability_sid->Clone());
  return true;
}

void AppContainerBase::AddImpersonationCapability(
    const wchar_t* capability_name) {
  AddCapability(base::win::Sid::FromNamedCapability(capability_name), true);
}

void AppContainerBase::AddImpersonationCapability(
    base::win::WellKnownCapability capability) {
  AddCapability(base::win::Sid::FromKnownCapability(capability), true);
}

bool AppContainerBase::AddImpersonationCapabilitySddl(const wchar_t* sddl_sid) {
  return AddCapability(base::win::Sid::FromSddlString(sddl_sid), true);
}

const std::vector<base::win::Sid>& AppContainerBase::GetCapabilities() {
  return capabilities_;
}

const std::vector<base::win::Sid>&
AppContainerBase::GetImpersonationCapabilities() {
  return impersonation_capabilities_;
}

const base::win::Sid& AppContainerBase::GetPackageSid() const {
  return package_sid_;
}

void AppContainerBase::SetEnableLowPrivilegeAppContainer(bool enable) {
  enable_low_privilege_app_container_ = enable;
}

bool AppContainerBase::GetEnableLowPrivilegeAppContainer() {
  return enable_low_privilege_app_container_;
}

AppContainerType AppContainerBase::GetAppContainerType() {
  return type_;
}

std::unique_ptr<SecurityCapabilities>
AppContainerBase::GetSecurityCapabilities() {
  return std::make_unique<SecurityCapabilities>(package_sid_, capabilities_);
}

std::optional<base::win::AccessToken> AppContainerBase::BuildImpersonationToken(
    const base::win::AccessToken& token) {
  std::optional<base::win::AccessToken> lowbox = token.CreateAppContainer(
      package_sid_, impersonation_capabilities_, TOKEN_ALL_ACCESS);
  ;
  if (!lowbox.has_value()) {
    return std::nullopt;
  }

  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          lowbox->get(), base::win::SecurityObjectType::kKernel,
          DACL_SECURITY_INFORMATION);
  if (!sd) {
    return std::nullopt;
  }

  lowbox = lowbox->DuplicateImpersonation(
      base::win::SecurityImpersonationLevel::kImpersonation, TOKEN_ALL_ACCESS);
  if (!lowbox.has_value()) {
    return std::nullopt;
  }

  if (!sd->WriteToHandle(lowbox->get(), base::win::SecurityObjectType::kKernel,
                         DACL_SECURITY_INFORMATION)) {
    return std::nullopt;
  }

  return lowbox;
}

std::optional<base::win::AccessToken> AppContainerBase::BuildPrimaryToken(
    const base::win::AccessToken& token) {
  return token.CreateAppContainer(package_sid_, capabilities_,
                                  TOKEN_ALL_ACCESS);
}

}  // namespace sandbox
