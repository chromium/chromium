// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_APP_CONTAINER_BASE_H_
#define SANDBOX_WIN_SRC_APP_CONTAINER_BASE_H_

#include <memory>
#include <vector>

#include <optional>
#include "base/win/access_token.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

class AppContainerBase final : public AppContainer {
 public:
  AppContainerBase(const AppContainerBase&) = delete;
  AppContainerBase& operator=(const AppContainerBase&) = delete;

  void AddRef() override;
  void Release() override;
  bool AccessCheck(const wchar_t* object_name,
                   base::win::SecurityObjectType object_type,
                   DWORD desired_access,
                   DWORD* granted_access,
                   BOOL* access_status) override;
  void AddCapability(const wchar_t* capability_name) override;
  void AddCapability(base::win::WellKnownCapability capability) override;
  bool AddCapabilitySddl(const wchar_t* sddl_sid) override;
  void AddImpersonationCapability(const wchar_t* capability_name) override;
  void AddImpersonationCapability(
      base::win::WellKnownCapability capability) override;
  bool AddImpersonationCapabilitySddl(const wchar_t* sddl_sid) override;
  void SetEnableLowPrivilegeAppContainer(bool enable) override;
  bool GetEnableLowPrivilegeAppContainer() override;
  AppContainerType GetAppContainerType() override;
  const std::vector<base::win::Sid>& GetCapabilities() override;
  const std::vector<base::win::Sid>& GetImpersonationCapabilities() override;
  std::unique_ptr<SecurityCapabilities> GetSecurityCapabilities() override;

  // Get the package SID for this AC.
  const base::win::Sid& GetPackageSid() const;

  // Creates a new AppContainer object. This will create a new profile
  // if it doesn't already exist. The profile must be deleted manually using
  // the Delete method if it's no longer required.
  static AppContainerBase* CreateProfile(const wchar_t* package_name,
                                         const wchar_t* display_name,
                                         const wchar_t* description);

  // Opens a derived AppContainer object. No checks will be made on
  // whether the package exists or not.
  static AppContainerBase* Open(const wchar_t* package_name);

  // Creates a new Lowbox object. Need to followup with a call to build lowbox
  // token
  static AppContainerBase* CreateLowbox(const wchar_t* sid);

  // Delete a profile based on name. Returns true if successful, or if the
  // package doesn't already exist.
  static bool Delete(const wchar_t* package_name);

  // Build an impersontion token from an existing token.
  // `token` specify the base token to create the new token from. Must have
  // TOKEN_DUPLICATE access. The token is created with the impersonation
  // capabilities list.
  std::optional<base::win::AccessToken> BuildImpersonationToken(
      const base::win::AccessToken& token);

  // Build a primary token from an existing token.
  // `token` specify the base token to create the new token from. Must have
  // TOKEN_DUPLICATE access. The token is created with the normal capabilities
  // list.
  std::optional<base::win::AccessToken> BuildPrimaryToken(
      const base::win::AccessToken& token);

 private:
  AppContainerBase(base::win::Sid& package_sid, AppContainerType type);
  ~AppContainerBase();

  bool AddCapability(const std::optional<base::win::Sid>& capability_sid,
                     bool impersonation_only);

  // Standard object-lifetime reference counter.
  volatile LONG ref_count_;
  base::win::Sid package_sid_;
  bool enable_low_privilege_app_container_;
  std::vector<base::win::Sid> capabilities_;
  std::vector<base::win::Sid> impersonation_capabilities_;
  AppContainerType type_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_APP_CONTAINER_BASE_H_
