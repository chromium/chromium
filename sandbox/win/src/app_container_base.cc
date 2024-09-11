// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/app_container_base.h"

#include <windows.h>

#include <userenv.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/types/expected.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {

struct FreeSidDeleter {
  inline void operator()(void* ptr) const { ::FreeSid(ptr); }
};

std::optional<base::win::Sid> DerivePackageSid(const wchar_t* package_name) {
  PSID package_sid_ptr = nullptr;
  HRESULT hr = ::DeriveAppContainerSidFromAppContainerName(package_name,
                                                           &package_sid_ptr);
  if (FAILED(hr)) {
    return std::nullopt;
  }

  std::unique_ptr<void, FreeSidDeleter> sid_deleter(package_sid_ptr);
  return base::win::Sid::FromPSID(package_sid_ptr);
}

HRESULT WINAPI AppContainerRegisterSid(PSID, LPCWSTR, LPCWSTR);
HRESULT WINAPI AppContainerUnregisterSid(PSID);
HRESULT WINAPI AppContainerLookupMoniker(PSID, LPWSTR*);
void WINAPI AppContainerFreeMemory(void*);

template <typename T>
T BindFunc(const char* name) {
  T fn = reinterpret_cast<T>(
      ::GetProcAddress(::GetModuleHandle(L"kernelbase.dll"), name));
  CHECK(fn);
  return fn;
}

HRESULT RegisterSid(const base::win::Sid& package_sid,
                    const wchar_t* moniker,
                    const wchar_t* display_name) {
  static auto register_sid_fn =
      BindFunc<decltype(&AppContainerRegisterSid)>("AppContainerRegisterSid");
  return register_sid_fn(package_sid.GetPSID(), moniker, display_name);
}

HRESULT UnregisterSid(const base::win::Sid& package_sid) {
  static auto unregister_sid_fn =
      BindFunc<decltype(&AppContainerUnregisterSid)>(
          "AppContainerUnregisterSid");
  return unregister_sid_fn(package_sid.GetPSID());
}

base::expected<std::wstring, HRESULT> LookupMoniker(
    const base::win::Sid& package_sid) {
  static auto lookup_moniker_fn =
      BindFunc<decltype(&AppContainerLookupMoniker)>(
          "AppContainerLookupMoniker");
  static auto free_memory_fn =
      BindFunc<decltype(&AppContainerFreeMemory)>("AppContainerFreeMemory");

  LPWSTR moniker_p;
  HRESULT hr = lookup_moniker_fn(package_sid.GetPSID(), &moniker_p);
  if (FAILED(hr)) {
    return base::unexpected(hr);
  }
  std::wstring moniker = moniker_p;
  free_memory_fn(moniker_p);
  return moniker;
}

std::optional<base::FilePath> GetProfilePath(const wchar_t* package_name) {
  base::FilePath local_app_data;
  if (!base::PathService::Get(base::DIR_LOCAL_APP_DATA, &local_app_data)) {
    return std::nullopt;
  }
  return local_app_data.Append(L"Packages").Append(package_name);
}

bool CreateAppContainerDirectory(const base::FilePath& profile_path,
                                 const base::win::Sid& package_sid) {
  base::FilePath ac_path = profile_path.Append(L"AC");
  if (!base::CreateDirectory(ac_path)) {
    return false;
  }

  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromFile(ac_path,
                                              DACL_SECURITY_INFORMATION);
  if (!sd) {
    return false;
  }
  if (!sd->SetMandatoryLabel(SECURITY_MANDATORY_LOW_RID,
                             OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE,
                             SYSTEM_MANDATORY_LABEL_NO_WRITE_UP)) {
    return false;
  }
  if (!sd->SetDaclEntry(package_sid, base::win::SecurityAccessMode::kGrant,
                        FILE_ALL_ACCESS,
                        OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE)) {
    return false;
  }
  if (!sd->WriteToFile(
          ac_path, DACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION)) {
    return false;
  }

  return base::CreateDirectory(ac_path.Append(L"Temp"));
}

bool CreateProfileDirectory(const wchar_t* package_name,
                            const base::win::Sid& package_sid) {
  std::optional<base::FilePath> profile_path = GetProfilePath(package_name);
  if (!profile_path) {
    return false;
  }
  if (base::DirectoryExists(*profile_path)) {
    return true;
  }
  if (!base::CreateDirectory(*profile_path)) {
    return false;
  }
  if (!CreateAppContainerDirectory(*profile_path, package_sid)) {
    base::DeletePathRecursively(*profile_path);
    return false;
  }
  return true;
}

class ProfileLock {
 public:
  ProfileLock()
      : handle_(::CreateMutex(nullptr,
                              FALSE,
                              L"_app_container_profile_lock_0278d671-c445-4dfa-"
                              L"a8b4-d5ccf66d4cc3")) {
    locked_ = ::WaitForSingleObject(handle_.get(), INFINITE) == WAIT_OBJECT_0;
  }
  ProfileLock(const ProfileLock&) = delete;
  ProfileLock& operator=(const ProfileLock&) = delete;

  ~ProfileLock() {
    if (locked_) {
      ::ReleaseMutex(handle_.get());
    }
  }

  bool locked() { return locked_; }

 private:
  base::win::ScopedHandle handle_;
  bool locked_;
};

}  // namespace

// static
std::unique_ptr<AppContainerBase> AppContainerBase::CreateProfile(
    const wchar_t* package_name,
    const wchar_t* display_name) {
  auto package_sid = DerivePackageSid(package_name);
  if (!package_sid) {
    return nullptr;
  }

  ProfileLock lock;
  if (!lock.locked()) {
    return nullptr;
  }

  HRESULT hr = RegisterSid(*package_sid, package_name, display_name);
  if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
    return std::make_unique<AppContainerBase>(
        package_name, std::move(*package_sid), AppContainerType::kProfile);
  }

  if (FAILED(hr)) {
    return nullptr;
  }

  if (!CreateProfileDirectory(package_name, *package_sid)) {
    UnregisterSid(*package_sid);
    return nullptr;
  }

  return std::make_unique<AppContainerBase>(
      package_name, std::move(*package_sid), AppContainerType::kProfile);
}

// static
std::unique_ptr<AppContainerBase> AppContainerBase::Open(
    const wchar_t* package_name) {
  auto package_sid = DerivePackageSid(package_name);
  if (!package_sid) {
    return nullptr;
  }
  return std::make_unique<AppContainerBase>(
      package_name, std::move(*package_sid), AppContainerType::kDerived);
}

// static
std::unique_ptr<AppContainerBase> AppContainerBase::CreateLowbox(
    const wchar_t* sid) {
  auto package_sid = base::win::Sid::FromSddlString(sid);
  if (!package_sid)
    return nullptr;

  return std::make_unique<AppContainerBase>(L"lowbox", std::move(*package_sid),
                                            AppContainerType::kLowbox);
}

// static
bool AppContainerBase::ProfileExists(const wchar_t* package_name) {
  auto package_sid = DerivePackageSid(package_name);
  if (!package_sid) {
    return false;
  }
  return LookupMoniker(*package_sid).has_value();
}

// static
bool AppContainerBase::Delete(const wchar_t* package_name) {
  auto package_sid = DerivePackageSid(package_name);
  if (!package_sid) {
    return false;
  }
  auto profile_path = GetProfilePath(package_name);
  if (!profile_path) {
    return false;
  }

  ProfileLock lock;
  if (!lock.locked()) {
    return false;
  }

  bool result = LookupMoniker(*package_sid).has_value() &&
                SUCCEEDED(UnregisterSid(*package_sid));
  return base::DirectoryExists(*profile_path) &&
         base::DeletePathRecursively(*profile_path) && result;
}

AppContainerBase::AppContainerBase(const wchar_t* package_name,
                                   base::win::Sid package_sid,
                                   AppContainerType type)
    : package_name_(package_name),
      package_sid_(std::move(package_sid)),
      enable_low_privilege_app_container_(false),
      type_(type) {}

AppContainerBase::~AppContainerBase() {}

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

const wchar_t* AppContainerBase::GetPackageName() const {
  return package_name_.c_str();
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
