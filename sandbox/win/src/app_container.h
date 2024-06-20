// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_APP_CONTAINER_H_
#define SANDBOX_WIN_SRC_APP_CONTAINER_H_

#include <vector>

#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/security_capabilities.h"

namespace sandbox {

enum AppContainerType { kNone, kDerived, kProfile, kLowbox };

class [[clang::lto_visibility_public]] AppContainer {
 public:
  // Do an access check based on this profile for a named object. If method
  // returns true then access_status reflects whether access was granted and
  // granted_access gives the final access rights. The object_type can be one of
  // kFile or kRegistry which correspond to SE_FILE_OBJECT or SE_REGISTRY_KEY.
  // See ::GetNamedSecurityInfo for more information about how the enumeration
  // is used and what format object_name needs to be.
  virtual bool AccessCheck(const wchar_t* object_name,
                           base::win::SecurityObjectType object_type,
                           DWORD desired_access,
                           DWORD* granted_access,
                           BOOL* access_status) = 0;

  // Adds a capability by name to this profile.
  virtual void AddCapability(const wchar_t* capability_name) = 0;
  // Adds a capability from a known list.
  virtual void AddCapability(base::win::WellKnownCapability capability) = 0;
  // Adds a capability from a SID
  virtual bool AddCapabilitySddl(const wchar_t* sddl_sid) = 0;

  // Adds an impersonation capability by name to this profile.
  virtual void AddImpersonationCapability(const wchar_t* capability_name) = 0;
  // Adds an impersonation capability from a known list.
  virtual void AddImpersonationCapability(
      base::win::WellKnownCapability capability) = 0;
  // Adds an impersonation capability from a SID
  virtual bool AddImpersonationCapabilitySddl(const wchar_t* sddl_sid) = 0;

  // Enable Low Privilege AC.
  virtual void SetEnableLowPrivilegeAppContainer(bool enable) = 0;
  virtual bool GetEnableLowPrivilegeAppContainer() = 0;

  virtual AppContainerType GetAppContainerType() = 0;

  // Get a vector of capabilities.
  virtual const std::vector<base::win::Sid>& GetCapabilities() = 0;

  // Get a vector of impersonation only capabilities. Used if the process needs
  // a more privileged token to start.
  virtual const std::vector<base::win::Sid>& GetImpersonationCapabilities() = 0;

  // Get an allocated SecurityCapabilities object for this App Container.
  virtual std::unique_ptr<SecurityCapabilities> GetSecurityCapabilities() = 0;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_APP_CONTAINER_H_
