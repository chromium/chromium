// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_APP_CONTAINER_BASE_H_
#define SANDBOX_WIN_SRC_APP_CONTAINER_BASE_H_

#include <windows.h>

#include <accctrl.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/security_capabilities.h"
#include "sandbox/win/src/sid.h"

namespace sandbox {

class AppContainerBase final : public AppContainer {
 public:
  AppContainerBase(const AppContainerBase&) = delete;
  AppContainerBase& operator=(const AppContainerBase&) = delete;

  void AddRef() override;
  void Release() override;
  bool GetRegistryLocation(REGSAM desired_access,
                           base::win::ScopedHandle* key) override;
  bool GetFolderPath(base::FilePath* file_path) override;
  bool GetPipePath(const wchar_t* pipe_name,
                   base::FilePath* pipe_path) override;
  bool AccessCheck(const wchar_t* object_name,
                   SE_OBJECT_TYPE object_type,
                   DWORD desired_access,
                   DWORD* granted_access,
                   BOOL* access_status) override;
  bool AddCapability(const wchar_t* capability_name) override;
  bool AddCapability(WellKnownCapabilities capability) override;
  bool AddCapabilitySddl(const wchar_t* sddl_sid) override;
  bool AddImpersonationCapability(const wchar_t* capability_name) override;
  bool AddImpersonationCapability(WellKnownCapabilities capability) override;
  bool AddImpersonationCapabilitySddl(const wchar_t* sddl_sid) override;
  void SetEnableLowPrivilegeAppContainer(bool enable) override;
  bool GetEnableLowPrivilegeAppContainer() override;
  AppContainerType GetAppContainerType() override;

  // Get the package SID for this AC.
  Sid GetPackageSid() const;

  // Get an allocated SecurityCapabilities object for this App Container.
  std::unique_ptr<SecurityCapabilities> GetSecurityCapabilities();

  // Get a vector of capabilities.
  const std::vector<Sid>& GetCapabilities();

  // Get a vector of impersonation only capabilities. Used if the process needs
  // a more privileged token to start.
  const std::vector<Sid>& GetImpersonationCapabilities();

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

  // Build the token for the lowbox
  ResultCode BuildLowBoxToken(base::win::ScopedHandle* token,
                              base::win::ScopedHandle* lockdown = nullptr);

 private:
  AppContainerBase(const Sid& package_sid, AppContainerType type);
  ~AppContainerBase();

  bool AddCapability(const Sid& capability_sid, bool impersonation_only);

  // Standard object-lifetime reference counter.
  volatile LONG ref_count_;
  Sid package_sid_;
  bool enable_low_privilege_app_container_;
  std::vector<Sid> capabilities_;
  std::vector<Sid> impersonation_capabilities_;
  AppContainerType type_;
  base::win::ScopedHandle lowbox_directory_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_APP_CONTAINER_BASE_H_
