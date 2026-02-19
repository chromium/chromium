// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_APP_CONTAINER_BASE_H_
#define SANDBOX_WIN_SRC_APP_CONTAINER_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/win/access_token.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

class AppContainerBase final : public AppContainer {
 public:
  AppContainerBase(std::wstring_view package_name,
                   base::win::Sid package_sid,
                   AppContainerType type);
  AppContainerBase(const AppContainerBase&) = delete;
  AppContainerBase& operator=(const AppContainerBase&) = delete;
  ~AppContainerBase();

  bool AccessCheck(base::wcstring_view object_name,
                   base::win::SecurityObjectType object_type,
                   DWORD desired_access,
                   DWORD* granted_access,
                   BOOL* access_status) override;
  void AddCapability(std::wstring_view capability_name) override;
  void AddCapability(base::win::WellKnownCapability capability) override;
  bool AddCapabilitySddl(base::wcstring_view sddl_sid) override;
  void AddImpersonationCapability(std::wstring_view capability_name) override;
  void AddImpersonationCapability(
      base::win::WellKnownCapability capability) override;
  bool AddImpersonationCapabilitySddl(base::wcstring_view sddl_sid) override;
  void SetEnableLowPrivilegeAppContainer(bool enable) override;
  bool GetEnableLowPrivilegeAppContainer() override;
  AppContainerType GetAppContainerType() override;
  const std::vector<base::win::Sid>& GetCapabilities() override;
  const std::vector<base::win::Sid>& GetImpersonationCapabilities() override;
  std::unique_ptr<SecurityCapabilities> GetSecurityCapabilities() override;

  // Get the package SID for this AC.
  const base::win::Sid& GetPackageSid() const;

  // Get the package name for this AC.
  std::wstring_view GetPackageName() const;

  // Creates a new AppContainer object. This will create a new profile
  // if it doesn't already exist. The profile must be deleted manually using
  // the Delete method if it's no longer required.
  static std::unique_ptr<AppContainerBase> CreateProfile(
      base::wcstring_view package_name,
      base::wcstring_view display_name);

  // Opens a derived AppContainer object. No checks will be made on
  // whether the package exists or not.
  static std::unique_ptr<AppContainerBase> Open(
      base::wcstring_view package_name);

  // Checks if a profile with a given name exists.
  static bool ProfileExists(base::wcstring_view package_name);

  // Delete a profile based on name. Returns true if successful, or if the
  // package doesn't already exist.
  static bool Delete(base::wcstring_view package_name);

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
  bool AddCapability(const std::optional<base::win::Sid>& capability_sid,
                     bool impersonation_only);

  std::wstring package_name_;
  base::win::Sid package_sid_;
  bool enable_low_privilege_app_container_;
  std::vector<base::win::Sid> capabilities_;
  std::vector<base::win::Sid> impersonation_capabilities_;
  AppContainerType type_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_APP_CONTAINER_BASE_H_
