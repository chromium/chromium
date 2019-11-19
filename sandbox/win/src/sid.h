// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_SID_H_
#define SANDBOX_SRC_SID_H_

#include <windows.h>

#include <string>

namespace sandbox {

// Known capabilities defined in Windows 8.
enum WellKnownCapabilities {
  kInternetClient,
  kInternetClientServer,
  kPrivateNetworkClientServer,
  kPicturesLibrary,
  kVideosLibrary,
  kMusicLibrary,
  kDocumentsLibrary,
  kEnterpriseAuthentication,
  kSharedUserCertificates,
  kRemovableStorage,
  kAppointments,
  kContacts,
  kMaxWellKnownCapability
};

// This class is used to hold and generate SIDS.
class Sid {
 public:
  // As PSID is just a void* make it explicit.
  explicit Sid(PSID sid);
  // Constructors initializing the object with the SID passed.
  // This is a converting constructor. It is not explicit.
  Sid(const SID* sid);
  Sid(WELL_KNOWN_SID_TYPE type);

  // Create a Sid from an AppContainer capability name. The name can be
  // completely arbitrary.
  static Sid FromNamedCapability(const wchar_t* capability_name);
  // Create a Sid from a known capability enumeration value. The Sids
  // match with the list defined in Windows 8.
  static Sid FromKnownCapability(WellKnownCapabilities capability);
  // Create a Sid from a SDDL format string, such as S-1-1-0.
  static Sid FromSddlString(const wchar_t* sddl_sid);
  // Create a Sid from a set of sub authorities.
  static Sid FromSubAuthorities(PSID_IDENTIFIER_AUTHORITY identifier_authority,
                                BYTE sub_authority_count,
                                PDWORD sub_authorities);
  // Create the restricted all application packages sid.
  static Sid AllRestrictedApplicationPackages();

  // Returns sid_.
  PSID GetPSID() const;

  // Gets whether the sid is valid.
  bool IsValid() const;

  // Converts the SID to a SDDL format string.
  bool ToSddlString(std::wstring* sddl_string) const;

 private:
  Sid();
  BYTE sid_[SECURITY_MAX_SID_SIZE];
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_SID_H_
