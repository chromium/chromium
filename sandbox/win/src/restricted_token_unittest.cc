// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for the RestrictedToken.

#include "sandbox/win/src/restricted_token.h"

#include <vector>

#include "base/win/access_token.h"
#include "base/win/atl.h"
#include "base/win/scoped_handle.h"
#include "base/win/sid.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/security_capabilities.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sandbox {

namespace {

void TestDefaultDalc(bool restricted_required, bool additional_sid_required) {
  RestrictedToken token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  if (!restricted_required)
    token.SetLockdownDefaultDacl();
  ATL::CSid additional_sid = ATL::Sids::Guests();
  ATL::CSid additional_sid2 = ATL::Sids::Batch();
  if (additional_sid_required) {
    token.AddDefaultDaclSid(
        *base::win::Sid::FromPSID(const_cast<SID*>(additional_sid.GetPSID())),
        SecurityAccessMode::kGrant, READ_CONTROL);
    token.AddDefaultDaclSid(
        *base::win::Sid::FromPSID(const_cast<SID*>(additional_sid2.GetPSID())),
        SecurityAccessMode::kDeny, GENERIC_ALL);
  }

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSid(base::win::WellKnownSid::kWorld));

  base::win::ScopedHandle handle;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(handle.Take());

  ATL::CDacl dacl;
  ASSERT_TRUE(restricted_token.GetDefaultDacl(&dacl));

  ATL::CSid logon_sid;
  ASSERT_TRUE(restricted_token.GetLogonSid(&logon_sid));

  bool restricted_found = false;
  bool logon_sid_found = false;
  bool additional_sid_found = false;
  bool additional_sid2_found = false;

  unsigned int ace_count = dacl.GetAceCount();
  for (unsigned int i = 0; i < ace_count; ++i) {
    ATL::CSid sid;
    ACCESS_MASK mask = 0;
    BYTE ace_type = 0;
    dacl.GetAclEntry(i, &sid, &mask, &ace_type);
    if (sid == ATL::Sids::RestrictedCode() && mask == GENERIC_ALL) {
      restricted_found = true;
    } else if (sid == logon_sid) {
      logon_sid_found = true;
    } else if (sid == additional_sid && mask == READ_CONTROL &&
               ace_type == ACCESS_ALLOWED_ACE_TYPE) {
      additional_sid_found = true;
    } else if (sid == additional_sid2 && mask == GENERIC_ALL &&
               ace_type == ACCESS_DENIED_ACE_TYPE) {
      additional_sid2_found = true;
    }
  }

  ASSERT_EQ(restricted_required, restricted_found);
  ASSERT_EQ(additional_sid_required, additional_sid_found);
  ASSERT_EQ(additional_sid_required, additional_sid2_found);
  if (!restricted_required)
    ASSERT_FALSE(logon_sid_found);
}

void CheckDaclForPackageSid(const base::win::ScopedHandle& token,
                            PSECURITY_CAPABILITIES security_capabilities,
                            bool package_sid_required) {
  DWORD length_needed = 0;
  ::GetKernelObjectSecurity(token.Get(), DACL_SECURITY_INFORMATION, nullptr, 0,
                            &length_needed);
  ASSERT_EQ(::GetLastError(), DWORD{ERROR_INSUFFICIENT_BUFFER});

  std::vector<char> security_desc_buffer(length_needed);
  SECURITY_DESCRIPTOR* security_desc =
      reinterpret_cast<SECURITY_DESCRIPTOR*>(security_desc_buffer.data());

  ASSERT_TRUE(::GetKernelObjectSecurity(token.Get(), DACL_SECURITY_INFORMATION,
                                        security_desc, length_needed,
                                        &length_needed));

  ATL::CSecurityDesc token_sd(*security_desc);
  ATL::CDacl dacl;
  ASSERT_TRUE(token_sd.GetDacl(&dacl));

  base::win::Sid package_sid =
      *base::win::Sid::FromPSID(security_capabilities->AppContainerSid);
  base::win::Sid all_package_sid(
      base::win::WellKnownSid::kAllApplicationPackages);

  unsigned int ace_count = dacl.GetAceCount();
  for (unsigned int i = 0; i < ace_count; ++i) {
    ATL::CSid sid;
    ACCESS_MASK mask = 0;
    BYTE type = 0;
    dacl.GetAclEntry(i, &sid, &mask, &type);
    if (mask != TOKEN_ALL_ACCESS || type != ACCESS_ALLOWED_ACE_TYPE)
      continue;
    PSID psid = const_cast<SID*>(sid.GetPSID());
    if (package_sid.Equal(psid))
      EXPECT_TRUE(package_sid_required);
    else if (all_package_sid.Equal(psid))
      EXPECT_FALSE(package_sid_required);
  }
}

void CheckLowBoxToken(const base::win::ScopedHandle& lowbox_token,
                      bool impersonation,
                      PSECURITY_CAPABILITIES security_capabilities) {
  auto token = base::win::AccessToken::FromToken(lowbox_token.Get());
  ASSERT_TRUE(token);
  EXPECT_TRUE(token->IsAppContainer());
  EXPECT_EQ(impersonation, token->IsImpersonation());
  EXPECT_FALSE(token->IsIdentification());
  auto package_sid = token->AppContainerSid();
  ASSERT_TRUE(package_sid);
  EXPECT_TRUE(package_sid->Equal(security_capabilities->AppContainerSid));

  auto capabilities = token->Capabilities();
  ASSERT_EQ(capabilities.size(), security_capabilities->CapabilityCount);
  for (size_t index = 0; index < capabilities.size(); ++index) {
    EXPECT_EQ(capabilities[index].GetAttributes(),
              security_capabilities->Capabilities[index].Attributes);
    EXPECT_TRUE(capabilities[index].GetSid().Equal(
        security_capabilities->Capabilities[index].Sid));
  }

  CheckDaclForPackageSid(lowbox_token, security_capabilities, true);
}

// Checks if a sid is in the restricting list of the restricted token.
// Asserts if it's not the case. If count is a positive number, the number of
// elements in the restricting sids list has to be equal.
void CheckRestrictingSid(const base::win::AccessToken& token,
                         const base::win::Sid& sid,
                         int count) {
  auto restricted_sids = token.RestrictedSids();
  if (count >= 0)
    ASSERT_EQ(static_cast<unsigned>(count), restricted_sids.size());

  bool present = false;
  for (const base::win::AccessToken::Group& group : restricted_sids) {
    if (group.GetSid() == sid) {
      present = true;
      break;
    }
  }

  ASSERT_TRUE(present);
}

void CheckRestrictingSid(const base::win::AccessToken& token,
                         base::win::WellKnownSid known_sid,
                         int count) {
  CheckRestrictingSid(token, base::win::Sid(known_sid), count);
}

void CheckRestrictingSid(HANDLE restricted_token,
                         base::win::WellKnownSid known_sid,
                         int count) {
  auto token = base::win::AccessToken::FromToken(restricted_token);
  ASSERT_TRUE(token);
  CheckRestrictingSid(*token, known_sid, count);
}

}  // namespace

// Tests the initializatioin with an invalid token handle.
TEST(RestrictedTokenTest, InvalidHandle) {
  RestrictedToken token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_INVALID_HANDLE),
            token.Init(reinterpret_cast<HANDLE>(0x5555)));
}

// Tests the initialization with nullptr as parameter.
TEST(RestrictedTokenTest, DefaultInit) {
  // Get the current process token.
  absl::optional<base::win::AccessToken> access_token =
      base::win::AccessToken::FromCurrentProcess();
  ASSERT_TRUE(access_token);

  // Create the token using the current token.
  RestrictedToken token_default;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token_default.Init(nullptr));

  // Get the handle to the restricted token.
  base::win::ScopedHandle restricted_token_handle;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token_default.GetRestrictedToken(&restricted_token_handle));

  auto restricted_token =
      base::win::AccessToken::FromToken(restricted_token_handle.Get());
  ASSERT_TRUE(restricted_token);
  // Check if both token have the same owner and user.
  EXPECT_EQ(restricted_token->User(), access_token->User());
  EXPECT_EQ(restricted_token->Owner(), access_token->Owner());
}

// Tests the initialization with a custom token as parameter.
TEST(RestrictedTokenTest, CustomInit) {
  CAccessToken access_token;
  ASSERT_TRUE(access_token.GetProcessToken(TOKEN_ALL_ACCESS));
  // Change the primary group.
  access_token.SetPrimaryGroup(ATL::Sids::World());

  // Create the token using the current token.
  RestrictedToken token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.Init(access_token.GetHandle()));
  base::win::ScopedHandle restricted_token_handle;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&restricted_token_handle));
  auto restricted_token =
      base::win::AccessToken::FromToken(restricted_token_handle.Get());
  ASSERT_TRUE(restricted_token);

  ATL::CSid sid_default;
  ASSERT_TRUE(access_token.GetPrimaryGroup(&sid_default));
  // Check if both token have the same primary grou.
  ASSERT_TRUE(restricted_token->PrimaryGroup().Equal(
      const_cast<SID*>(sid_default.GetPSID())));
}

// Verifies that the token created by the object are valid.
TEST(RestrictedTokenTest, ResultToken) {
  RestrictedToken token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSid(base::win::WellKnownSid::kWorld));

  base::win::ScopedHandle restricted_token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&restricted_token));

  auto primary = base::win::AccessToken::FromToken(restricted_token.Get());
  ASSERT_TRUE(primary);
  EXPECT_TRUE(primary->IsRestricted());
  EXPECT_FALSE(primary->IsImpersonation());

  base::win::ScopedHandle impersonation_token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedTokenForImpersonation(&impersonation_token));
  auto impersonation =
      base::win::AccessToken::FromToken(impersonation_token.Get());
  ASSERT_TRUE(impersonation);
  ASSERT_TRUE(impersonation->IsRestricted());
  ASSERT_TRUE(impersonation->IsImpersonation());
  ASSERT_FALSE(impersonation->IsIdentification());
}

// Verifies that the token created has "Restricted" in its default dacl.
TEST(RestrictedTokenTest, DefaultDacl) {
  TestDefaultDalc(true, false);
}

// Verifies that the token created does not have "Restricted" in its default
// dacl.
TEST(RestrictedTokenTest, DefaultDaclLockdown) {
  TestDefaultDalc(false, false);
}

// Verifies that the token created has an additional SID in its default dacl.
TEST(RestrictedTokenTest, DefaultDaclWithAddition) {
  TestDefaultDalc(true, true);
}

// Verifies that the token created does not have "Restricted" in its default
// dacl and also has an additional SID.
TEST(RestrictedTokenTest, DefaultDaclLockdownWithAddition) {
  TestDefaultDalc(false, true);
}

// Tests the method "AddSidForDenyOnly".
TEST(RestrictedTokenTest, DenySid) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddSidForDenyOnly(base::win::WellKnownSid::kWorld));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));
  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  auto sid = base::win::Sid(base::win::WellKnownSid::kWorld);
  bool found_sid = false;
  for (const auto& group : restricted_token->Groups()) {
    if (sid == group.GetSid()) {
      ASSERT_TRUE(group.IsDenyOnly());
      found_sid = true;
    }
  }
  ASSERT_TRUE(found_sid);
}

// Tests the method "AddAllSidsForDenyOnly".
TEST(RestrictedTokenTest, DenySids) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.AddAllSidsForDenyOnly({}));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));
  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  bool found_sid = false;
  // Verify that all sids are really gone.
  for (const auto& group : restricted_token->Groups()) {
    if (group.IsLogonId() || group.IsIntegrity())
      continue;
    ASSERT_TRUE(group.IsDenyOnly());
    found_sid = true;
  }
  // Check we at least found one SID.
  ASSERT_TRUE(found_sid);
}

// Tests the method "AddAllSidsForDenyOnly" using an exception list.
TEST(RestrictedTokenTest, DenySidsException) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  std::vector<base::win::Sid> sids_exception =
      base::win::Sid::FromKnownSidVector({base::win::WellKnownSid::kWorld});

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddAllSidsForDenyOnly(sids_exception));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);

  bool found_sid = false;
  // Verify that all sids are really gone.
  for (const auto& group : restricted_token->Groups()) {
    if (group.IsLogonId() || group.IsIntegrity())
      continue;
    if (sids_exception[0] == group.GetSid()) {
      ASSERT_FALSE(group.IsDenyOnly());
      // Check we at least found one SID.
      found_sid = true;
    } else {
      ASSERT_TRUE(group.IsDenyOnly());
    }
  }
  ASSERT_TRUE(found_sid);
}

// Tests test method AddOwnerSidForDenyOnly.
TEST(RestrictedTokenTest, DenyOwnerSid) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.AddUserSidForDenyOnly());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));
  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  EXPECT_TRUE(restricted_token->UserGroup().IsDenyOnly());
}

// Tests test method AddOwnerSidForDenyOnly with a custom effective token.
TEST(RestrictedTokenTest, DenyOwnerSidCustom) {
  CAccessToken access_token;
  ASSERT_TRUE(access_token.GetProcessToken(TOKEN_ALL_ACCESS));
  RestrictedToken token;
  base::win::ScopedHandle token_handle;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.Init(access_token.GetHandle()));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.AddUserSidForDenyOnly());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));
  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  EXPECT_TRUE(restricted_token->UserGroup().IsDenyOnly());
}

// Tests the method DeleteAllPrivileges.
TEST(RestrictedTokenTest, DeleteAllPrivileges) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.DeleteAllPrivileges(/*remove_traversal_privilege=*/true));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));
  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  EXPECT_TRUE(restricted_token->Privileges().empty());
}

// Tests the method DeleteAllPrivileges with an exception list.
TEST(RestrictedTokenTest, DeleteAllPrivilegesException) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.DeleteAllPrivileges(/*remove_traversal_privilege=*/false));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  auto privileges = restricted_token->Privileges();
  ASSERT_EQ(1U, privileges.size());
  EXPECT_EQ(privileges[0].GetName(), SE_CHANGE_NOTIFY_NAME);
}

// Tests the method AddRestrictingSid.
TEST(RestrictedTokenTest, AddRestrictingSid) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSid(base::win::WellKnownSid::kWorld));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  CheckRestrictingSid(*restricted_token, base::win::WellKnownSid::kWorld, 1);
}

// Tests the method AddRestrictingSidCurrentUser.
TEST(RestrictedTokenTest, AddRestrictingSidCurrentUser) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSidCurrentUser());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  CheckRestrictingSid(*restricted_token, restricted_token->User(), 1);
}

// Tests the method AddRestrictingSidCurrentUser with a custom effective token.
TEST(RestrictedTokenTest, AddRestrictingSidCurrentUserCustom) {
  CAccessToken access_token;
  ASSERT_TRUE(access_token.GetProcessToken(TOKEN_ALL_ACCESS));
  RestrictedToken token;
  base::win::ScopedHandle token_handle;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.Init(access_token.GetHandle()));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSidCurrentUser());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  CheckRestrictingSid(*restricted_token, restricted_token->User(), 1);
}

// Tests the method AddRestrictingSidLogonSession.
TEST(RestrictedTokenTest, AddRestrictingSidLogonSession) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSidLogonSession());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  auto session = restricted_token->LogonId();
  if (!session) {
    ASSERT_EQ(static_cast<DWORD>(ERROR_NOT_FOUND), ::GetLastError());
    return;
  }

  CheckRestrictingSid(*restricted_token, *session, 1);
}

// Tests adding a lot of restricting sids.
TEST(RestrictedTokenTest, AddMultipleRestrictingSids) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSidCurrentUser());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSidLogonSession());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSid(base::win::WellKnownSid::kWorld));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  ASSERT_EQ(3u, restricted_token->RestrictedSids().size());
}

// Tests the method "AddRestrictingSidAllSids".
TEST(RestrictedTokenTest, AddAllSidToRestrictingSids) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSidAllSids());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  auto restricted_token = base::win::AccessToken::FromToken(token_handle.Get());
  ASSERT_TRUE(restricted_token);
  auto groups = restricted_token->Groups();

  // Verify that all group sids are in the restricting sid list.
  for (const auto& group : groups) {
    if (!group.IsIntegrity())
      CheckRestrictingSid(*restricted_token, group.GetSid(), -1);
  }

  CheckRestrictingSid(*restricted_token, restricted_token->User(), -1);
}

// Checks the error code when the object is initialized twice.
TEST(RestrictedTokenTest, DoubleInit) {
  RestrictedToken token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));

  ASSERT_EQ(static_cast<DWORD>(ERROR_ALREADY_INITIALIZED), token.Init(nullptr));
}

TEST(RestrictedTokenTest, LockdownDefaultDaclNoLogonSid) {
  ATL::CAccessToken anonymous_token;
  ASSERT_TRUE(::ImpersonateAnonymousToken(::GetCurrentThread()));
  ASSERT_TRUE(anonymous_token.GetThreadToken(TOKEN_ALL_ACCESS));
  ::RevertToSelf();
  ATL::CSid logon_sid;
  // Verify that the anonymous token doesn't have the logon sid.
  ASSERT_FALSE(anonymous_token.GetLogonSid(&logon_sid));

  RestrictedToken token;
  ASSERT_EQ(DWORD{ERROR_SUCCESS}, token.Init(anonymous_token.GetHandle()));
  token.SetLockdownDefaultDacl();

  base::win::ScopedHandle handle;
  ASSERT_EQ(DWORD{ERROR_SUCCESS}, token.GetRestrictedToken(&handle));
}

TEST(RestrictedTokenTest, LowBoxToken) {
  base::win::ScopedHandle token;

  auto package_sid = base::win::Sid::FromSddlString(L"S-1-15-2-1-2-3-4-5-6-7");
  ASSERT_TRUE(package_sid);
  SecurityCapabilities caps_no_capabilities(*package_sid);

  ASSERT_EQ(
      DWORD{ERROR_INVALID_PARAMETER},
      CreateLowBoxToken(nullptr, PRIMARY, &caps_no_capabilities, nullptr));
  ASSERT_EQ(DWORD{ERROR_SUCCESS},
            CreateLowBoxToken(nullptr, PRIMARY, &caps_no_capabilities, &token));
  ASSERT_TRUE(token.IsValid());
  CheckLowBoxToken(token, false, &caps_no_capabilities);

  ASSERT_TRUE(ReplacePackageSidInDacl(token.Get(), SecurityObjectType::kKernel,
                                      *package_sid, TOKEN_ALL_ACCESS));
  CheckDaclForPackageSid(token, &caps_no_capabilities, false);

  ASSERT_EQ(
      DWORD{ERROR_SUCCESS},
      CreateLowBoxToken(nullptr, IMPERSONATION, &caps_no_capabilities, &token));
  ASSERT_TRUE(token.IsValid());
  CheckLowBoxToken(token, true, &caps_no_capabilities);

  auto capabilities = base::win::Sid::FromKnownCapabilityVector(
      {base::win::WellKnownCapability::kInternetClient,
       base::win::WellKnownCapability::kPrivateNetworkClientServer});
  SecurityCapabilities caps_with_capabilities(*package_sid, capabilities);
  ASSERT_EQ(
      DWORD{ERROR_SUCCESS},
      CreateLowBoxToken(nullptr, PRIMARY, &caps_with_capabilities, &token));
  ASSERT_TRUE(token.IsValid());
  CheckLowBoxToken(token, false, &caps_with_capabilities);

  RestrictedToken restricted_token;
  base::win::ScopedHandle token_handle;
  ASSERT_EQ(DWORD{ERROR_SUCCESS}, restricted_token.Init(nullptr));
  ASSERT_EQ(DWORD{ERROR_SUCCESS}, restricted_token.AddRestrictingSid(
                                      base::win::WellKnownSid::kWorld));
  ASSERT_EQ(DWORD{ERROR_SUCCESS},
            restricted_token.GetRestrictedToken(&token_handle));

  ASSERT_EQ(DWORD{ERROR_SUCCESS},
            CreateLowBoxToken(token_handle.Get(), PRIMARY,
                              &caps_with_capabilities, &token));
  ASSERT_TRUE(token.IsValid());
  CheckLowBoxToken(token, false, &caps_with_capabilities);
  CheckRestrictingSid(token.Get(), base::win::WellKnownSid::kWorld, 1);
}

// Checks the functionality of CanLowIntegrityAccessDesktop
TEST(RestrictedTokenTest, MediumIlDesktop) {
  ASSERT_TRUE(CanLowIntegrityAccessDesktop());

  // Create a desktop using the default security descriptor (the last parameter)
  // which doesn't allow low IL to access it in practice.
  HDESK hdesk = ::CreateDesktopW(L"medium_il_desktop", nullptr, nullptr, 0,
                                 GENERIC_ALL, nullptr);
  ASSERT_TRUE(hdesk);

  HDESK old_hdesk = ::GetThreadDesktop(::GetCurrentThreadId());
  ASSERT_TRUE(hdesk);
  ASSERT_TRUE(::SetThreadDesktop(hdesk));
  ASSERT_FALSE(CanLowIntegrityAccessDesktop());
  ASSERT_TRUE(::SetThreadDesktop(old_hdesk));
  ASSERT_TRUE(::CloseDesktop(hdesk));
}

}  // namespace sandbox
