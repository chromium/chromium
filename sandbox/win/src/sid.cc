// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sid.h"

#include <memory>

#include <sddl.h>

#include "base/logging.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace {

DWORD WellKnownCapabilityToRid(WellKnownCapabilities capability) {
  switch (capability) {
    case kInternetClient:
      return SECURITY_CAPABILITY_INTERNET_CLIENT;
    case kInternetClientServer:
      return SECURITY_CAPABILITY_INTERNET_CLIENT_SERVER;
    case kPrivateNetworkClientServer:
      return SECURITY_CAPABILITY_PRIVATE_NETWORK_CLIENT_SERVER;
    case kPicturesLibrary:
      return SECURITY_CAPABILITY_PICTURES_LIBRARY;
    case kVideosLibrary:
      return SECURITY_CAPABILITY_VIDEOS_LIBRARY;
    case kMusicLibrary:
      return SECURITY_CAPABILITY_MUSIC_LIBRARY;
    case kDocumentsLibrary:
      return SECURITY_CAPABILITY_DOCUMENTS_LIBRARY;
    case kEnterpriseAuthentication:
      return SECURITY_CAPABILITY_ENTERPRISE_AUTHENTICATION;
    case kSharedUserCertificates:
      return SECURITY_CAPABILITY_SHARED_USER_CERTIFICATES;
    case kRemovableStorage:
      return SECURITY_CAPABILITY_REMOVABLE_STORAGE;
    case kAppointments:
      return SECURITY_CAPABILITY_APPOINTMENTS;
    case kContacts:
      return SECURITY_CAPABILITY_CONTACTS;
    default:
      break;
  }
  return 0;
}

}  // namespace

Sid::Sid() : sid_() {}

Sid::Sid(PSID sid) : sid_() {
  ::CopySid(SECURITY_MAX_SID_SIZE, sid_, sid);
}

Sid::Sid(const SID* sid) : sid_() {
  ::CopySid(SECURITY_MAX_SID_SIZE, sid_, const_cast<SID*>(sid));
}

Sid::Sid(WELL_KNOWN_SID_TYPE type) {
  DWORD size_sid = SECURITY_MAX_SID_SIZE;
  bool result = ::CreateWellKnownSid(type, nullptr, sid_, &size_sid);
  DCHECK(result);
  (void)result;
}

Sid Sid::FromKnownCapability(WellKnownCapabilities capability) {
  DWORD capability_rid = WellKnownCapabilityToRid(capability);
  if (!capability_rid)
    return Sid();
  SID_IDENTIFIER_AUTHORITY capability_authority = {
      SECURITY_APP_PACKAGE_AUTHORITY};
  DWORD sub_authorities[] = {SECURITY_CAPABILITY_BASE_RID, capability_rid};
  return FromSubAuthorities(&capability_authority, 2, sub_authorities);
}

Sid Sid::FromNamedCapability(const wchar_t* capability_name) {
  RtlDeriveCapabilitySidsFromNameFunction derive_capability_sids = nullptr;
  ResolveNTFunctionPtr("RtlDeriveCapabilitySidsFromName",
                       &derive_capability_sids);
  RtlInitUnicodeStringFunction init_unicode_string = nullptr;
  ResolveNTFunctionPtr("RtlInitUnicodeString", &init_unicode_string);

  if (!derive_capability_sids || !init_unicode_string)
    return Sid();

  if (!capability_name || ::wcslen(capability_name) == 0)
    return Sid();

  UNICODE_STRING name = {};
  init_unicode_string(&name, capability_name);
  Sid capability_sid;
  Sid group_sid;

  NTSTATUS status =
      derive_capability_sids(&name, group_sid.sid_, capability_sid.sid_);
  if (!NT_SUCCESS(status))
    return Sid();

  return capability_sid;
}

Sid Sid::FromSddlString(const wchar_t* sddl_sid) {
  PSID converted_sid;
  if (!::ConvertStringSidToSid(sddl_sid, &converted_sid))
    return Sid();

  return Sid(converted_sid);
}

Sid Sid::FromSubAuthorities(PSID_IDENTIFIER_AUTHORITY identifier_authority,
                            BYTE sub_authority_count,
                            PDWORD sub_authorities) {
  Sid sid;
  if (!::InitializeSid(sid.sid_, identifier_authority, sub_authority_count))
    return Sid();

  for (DWORD index = 0; index < sub_authority_count; ++index) {
    PDWORD sub_authority = GetSidSubAuthority(sid.sid_, index);
    *sub_authority = sub_authorities[index];
  }
  return sid;
}

Sid Sid::AllRestrictedApplicationPackages() {
  SID_IDENTIFIER_AUTHORITY package_authority = {SECURITY_APP_PACKAGE_AUTHORITY};
  DWORD sub_authorities[] = {SECURITY_APP_PACKAGE_BASE_RID,
                             SECURITY_BUILTIN_PACKAGE_ANY_RESTRICTED_PACKAGE};
  return FromSubAuthorities(&package_authority, 2, sub_authorities);
}

PSID Sid::GetPSID() const {
  return const_cast<BYTE*>(sid_);
}

bool Sid::IsValid() const {
  return !!::IsValidSid(GetPSID());
}

// Converts the SID to an SDDL format string.
bool Sid::ToSddlString(std::wstring* sddl_string) const {
  LPWSTR sid = nullptr;
  if (!::ConvertSidToStringSid(GetPSID(), &sid))
    return false;
  std::unique_ptr<void, LocalFreeDeleter> sid_ptr(sid);
  *sddl_string = sid;
  return true;
}

}  // namespace sandbox
