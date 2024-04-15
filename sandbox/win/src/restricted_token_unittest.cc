// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for the RestrictedToken.

#include "sandbox/win/src/restricted_token.h"

#include <windows.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/win/access_control_list.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "base/win/security_util.h"
#include "base/win/sid.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/restricted_token_utils.h"
#include "sandbox/win/tests/common/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

void TestDefaultDacl(bool restricted_required, bool additional_sid_required) {
  RestrictedToken token;

  if (!restricted_required)
    token.SetLockdownDefaultDacl();
  base::win::Sid additional_sid(base::win::WellKnownSid::kBuiltinGuests);
  base::win::Sid additional_sid2(base::win::WellKnownSid::kBatch);
  if (additional_sid_required) {
    token.AddDefaultDaclSid(
        additional_sid, base::win::SecurityAccessMode::kGrant, READ_CONTROL);
    token.AddDefaultDaclSid(additional_sid2,
                            base::win::SecurityAccessMode::kDeny, GENERIC_ALL);
  }

  token.AddRestrictingSid(base::win::WellKnownSid::kWorld);
  auto restricted_token = *token.GetRestrictedToken();
  auto dacl = *restricted_token.DefaultDacl();

  EXPECT_EQ(restricted_required,
            IsSidInDacl(dacl, true, GENERIC_ALL,
                        base::win::Sid(base::win::WellKnownSid::kRestricted)));
  EXPECT_EQ(additional_sid_required,
            IsSidInDacl(dacl, true, READ_CONTROL, additional_sid));
  EXPECT_EQ(additional_sid_required,
            IsSidInDacl(dacl, false, GENERIC_ALL, additional_sid2));
  auto logon_sid = restricted_token.LogonId();
  if (logon_sid) {
    EXPECT_EQ(restricted_required,
              IsSidInDacl(dacl, true, std::nullopt, *logon_sid));
  }
}

// Checks if a sid is or is not in the restricting list of the restricted token.
// Asserts if it's not the case. If count is a positive number, the number of
// elements in the restricting sids list has to be equal.
void CheckRestrictingSid(const base::win::AccessToken& token,
                         const base::win::Sid& sid,
                         int count,
                         bool check_present = true) {
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

  ASSERT_EQ(present, check_present);
}

void CheckRestrictingSid(const base::win::AccessToken& token,
                         base::win::WellKnownSid known_sid,
                         int count) {
  CheckRestrictingSid(token, base::win::Sid(known_sid), count);
}

DWORD GetMandatoryPolicy(const base::win::AccessToken& token) {
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          token.get(), base::win::SecurityObjectType::kKernel,
          LABEL_SECURITY_INFORMATION);
  CHECK(sd);
  PACL sacl = sd->sacl()->get();
  for (DWORD ace_index = 0; ace_index < sacl->AceCount; ++ace_index) {
    PSYSTEM_MANDATORY_LABEL_ACE ace;

    if (::GetAce(sacl, ace_index, reinterpret_cast<LPVOID*>(&ace)) &&
        ace->Header.AceType == SYSTEM_MANDATORY_LABEL_ACE_TYPE) {
      return ace->Mask;
    }
  }
  return 0;
}

base::win::AccessToken GetPrimaryToken(ACCESS_MASK desired_access) {
  return *base::win::AccessToken::FromCurrentProcess(false, TOKEN_DUPLICATE)
              ->DuplicatePrimary(desired_access);
}

void CheckUniqueSid(TokenLevel level, bool check_present) {
  std::optional<base::win::Sid> random_sid =
      base::win::Sid::GenerateRandomSid();
  auto token = *CreateRestrictedToken(level, INTEGRITY_LEVEL_LAST,
                                      TokenType::kPrimary, false, random_sid);
  CheckRestrictingSid(token, *random_sid, -1, check_present);
  auto dacl = *token.DefaultDacl();
  EXPECT_TRUE(IsSidInDacl(dacl, true, GENERIC_ALL, *random_sid));
  EXPECT_TRUE(IsSidInDacl(
      dacl, true, READ_CONTROL,
      base::win::Sid(base::win::WellKnownSid::kCreatorOwnerRights)));
}

void CheckIntegrityLevel(IntegrityLevel integrity_level) {
  std::optional<base::win::AccessToken> token = CreateRestrictedToken(
      USER_LOCKDOWN, integrity_level, TokenType::kPrimary, false, std::nullopt);
  ASSERT_TRUE(token);
  std::optional<DWORD> rid = GetIntegrityLevelRid(integrity_level);
  if (rid) {
    EXPECT_EQ(token->IntegrityLevel(), *rid);
  } else {
    EXPECT_EQ(token->IntegrityLevel(), GetPrimaryToken(0).IntegrityLevel());
  }
}

void CheckPrivileges(TokenLevel level, bool delete_all, bool remove_traversal) {
  std::optional<base::win::AccessToken> token = CreateRestrictedToken(
      level, INTEGRITY_LEVEL_LAST, TokenType::kPrimary, false, std::nullopt);
  ASSERT_TRUE(token);
  std::vector<base::win::AccessToken::Privilege> privs = token->Privileges();
  if (remove_traversal) {
    EXPECT_EQ(privs.size(), 0U);
  } else if (delete_all) {
    EXPECT_EQ(privs.size(), 1U);
    EXPECT_EQ(privs[0].GetName(), SE_CHANGE_NOTIFY_NAME);
  } else {
    std::vector<base::win::AccessToken::Privilege> primary_privs =
        GetPrimaryToken(0).Privileges();
    ASSERT_EQ(privs.size(), primary_privs.size());
    for (size_t i = 0; i < privs.size(); ++i) {
      EXPECT_EQ(privs[i].GetLuid(), primary_privs[i].GetLuid());
    }
  }
}

void CheckRestricted(const base::win::AccessToken& token,
                     const std::vector<base::win::Sid>& sids) {
  std::vector<base::win::AccessToken::Group> restricted =
      token.RestrictedSids();
  ASSERT_EQ(sids.size(), restricted.size());
  for (size_t i = 0; i < sids.size(); ++i) {
    EXPECT_EQ(sids[i], restricted[i].GetSid())
        << *sids[i].ToSddlString() + L" != " +
               *restricted[i].GetSid().ToSddlString();
  }
}

void CheckRestricted(TokenLevel level,
                     const std::vector<base::win::WellKnownSid>& known_sids,
                     bool user,
                     bool logon) {
  std::optional<base::win::AccessToken> token = CreateRestrictedToken(
      level, INTEGRITY_LEVEL_LAST, TokenType::kPrimary, false, std::nullopt);
  std::vector<base::win::Sid> sids =
      base::win::Sid::FromKnownSidVector(known_sids);
  if (user) {
    sids.push_back(token->User());
  }
  if (logon) {
    std::optional<base::win::Sid> logon_sid = token->LogonId();
    if (logon_sid) {
      sids.push_back(logon_sid->Clone());
    }
  }
  CheckRestricted(*token, sids);
}

void CompareDenyOnly(
    TokenLevel level,
    const std::vector<base::win::WellKnownSid>& known_exceptions,
    bool allow_all,
    bool user) {
  std::optional<base::win::AccessToken> token = CreateRestrictedToken(
      level, INTEGRITY_LEVEL_LAST, TokenType::kPrimary, false, std::nullopt);
  ASSERT_TRUE(token);
  std::vector<base::win::Sid> exceptions =
      base::win::Sid::FromKnownSidVector(known_exceptions);
  std::vector<base::win::AccessToken::Group> groups =
      GetPrimaryToken(0).Groups();
  std::vector<base::win::AccessToken::Group> compare_groups = token->Groups();
  ASSERT_EQ(groups.size(), compare_groups.size());
  for (size_t i = 0; i < groups.size(); ++i) {
    const base::win::Sid& group_sid = groups[i].GetSid();
    ASSERT_EQ(group_sid, compare_groups[i].GetSid());
    if (groups[i].IsLogonId() || groups[i].IsIntegrity() ||
        groups[i].IsDenyOnly() || compare_groups[i].IsDenyOnly() || allow_all) {
      continue;
    }
    EXPECT_NE(base::ranges::find(exceptions, group_sid), exceptions.end());
  }
  EXPECT_EQ(user, token->UserGroup().IsDenyOnly());
}

}  // namespace

// Tests default initialization of the class.
TEST(RestrictedTokenTest, DefaultInit) {
  RestrictedToken token_default;
  std::optional<base::win::AccessToken> restricted_token =
      token_default.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);

  // Get the current process token.
  std::optional<base::win::AccessToken> access_token =
      base::win::AccessToken::FromCurrentProcess();
  ASSERT_TRUE(access_token);
  // Check if both token have the same owner and user.
  EXPECT_EQ(restricted_token->User(), access_token->User());
  EXPECT_EQ(restricted_token->Owner(), access_token->Owner());
  EXPECT_FALSE(restricted_token->IsImpersonation());
  EXPECT_FALSE(restricted_token->IsRestricted());
  EXPECT_EQ(DWORD{TOKEN_ALL_ACCESS},
            base::win::GetGrantedAccess(restricted_token->get()));
}

// Verifies that the token created is valid.
TEST(RestrictedTokenTest, ResultToken) {
  RestrictedToken token;
  token.AddRestrictingSid(base::win::WellKnownSid::kWorld);

  std::optional<base::win::AccessToken> restricted_token =
      token.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);
  EXPECT_TRUE(restricted_token->IsRestricted());
  EXPECT_FALSE(restricted_token->IsImpersonation());
}

// Verifies that the token created has "Restricted" in its default dacl.
TEST(RestrictedTokenTest, DefaultDacl) {
  TestDefaultDacl(true, false);
}

// Verifies that the token created does not have "Restricted" in its default
// dacl.
TEST(RestrictedTokenTest, DefaultDaclLockdown) {
  TestDefaultDacl(false, false);
}

// Verifies that the token created has an additional SID in its default dacl.
TEST(RestrictedTokenTest, DefaultDaclWithAddition) {
  TestDefaultDacl(true, true);
}

// Verifies that the token created does not have "Restricted" in its default
// dacl and also has an additional SID.
TEST(RestrictedTokenTest, DefaultDaclLockdownWithAddition) {
  TestDefaultDacl(false, true);
}

// Tests the method "AddSidForDenyOnly".
TEST(RestrictedTokenTest, DenySid) {
  RestrictedToken token;

  token.AddSidForDenyOnly(base::win::WellKnownSid::kWorld);
  std::optional<base::win::AccessToken> restricted_token =
      token.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);
  base::win::Sid sid(base::win::WellKnownSid::kWorld);
  bool found_sid = false;
  for (const base::win::AccessToken::Group& group :
       restricted_token->Groups()) {
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

  token.AddAllSidsForDenyOnly({});
  auto restricted_token = token.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);
  bool found_sid = false;
  // Verify that all sids are really gone.
  for (const base::win::AccessToken::Group& group :
       restricted_token->Groups()) {
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

  std::vector<base::win::Sid> sids_exception =
      base::win::Sid::FromKnownSidVector({base::win::WellKnownSid::kWorld});
  token.AddAllSidsForDenyOnly(sids_exception);

  auto restricted_token = token.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);

  bool found_sid = false;
  // Verify that all sids are really gone.
  for (const base::win::AccessToken::Group& group :
       restricted_token->Groups()) {
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
  token.AddUserSidForDenyOnly();
  auto restricted_token = token.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);
  EXPECT_TRUE(restricted_token->UserGroup().IsDenyOnly());
}

// Tests the method DeleteAllPrivileges.
TEST(RestrictedTokenTest, DeleteAllPrivileges) {
  RestrictedToken token;
  token.DeleteAllPrivileges(/*remove_traversal_privilege=*/true);
  auto restricted_token = token.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);
  EXPECT_TRUE(restricted_token->Privileges().empty());
}

// Tests the method DeleteAllPrivileges with an exception list.
TEST(RestrictedTokenTest, DeleteAllPrivilegesException) {
  RestrictedToken token;
  token.DeleteAllPrivileges(/*remove_traversal_privilege=*/false);
  auto restricted_token = token.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);
  auto privileges = restricted_token->Privileges();
  ASSERT_EQ(1U, privileges.size());
  EXPECT_EQ(privileges[0].GetName(), SE_CHANGE_NOTIFY_NAME);
}

// Tests the method AddRestrictingSid.
TEST(RestrictedTokenTest, AddRestrictingSid) {
  RestrictedToken token;
  token.AddRestrictingSid(base::win::WellKnownSid::kWorld);
  auto restricted_token = token.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);
  CheckRestrictingSid(*restricted_token, base::win::WellKnownSid::kWorld, 1);
}

// Tests the method AddRestrictingSidCurrentUser.
TEST(RestrictedTokenTest, AddRestrictingSidCurrentUser) {
  RestrictedToken token;

  token.AddRestrictingSidCurrentUser();
  auto restricted_token = token.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);
  CheckRestrictingSid(*restricted_token, restricted_token->User(), 1);
}

// Tests the method AddRestrictingSidLogonSession.
TEST(RestrictedTokenTest, AddRestrictingSidLogonSession) {
  RestrictedToken token;

  token.AddRestrictingSidLogonSession();
  auto restricted_token = token.GetRestrictedToken();
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

  token.AddRestrictingSidCurrentUser();
  token.AddRestrictingSidLogonSession();
  token.AddRestrictingSid(base::win::WellKnownSid::kWorld);
  auto restricted_token = token.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);
  ASSERT_EQ(3u, restricted_token->RestrictedSids().size());
}

// Tests the method "AddRestrictingSidAllSids".
TEST(RestrictedTokenTest, AddAllSidToRestrictingSids) {
  RestrictedToken token;

  token.AddRestrictingSidAllSids();
  auto restricted_token = token.GetRestrictedToken();
  ASSERT_TRUE(restricted_token);

  // Verify that all group sids are in the restricting sid list.
  for (const base::win::AccessToken::Group& group :
       restricted_token->Groups()) {
    if (!group.IsIntegrity())
      CheckRestrictingSid(*restricted_token, group.GetSid(), -1);
  }

  CheckRestrictingSid(*restricted_token, restricted_token->User(), -1);
}

TEST(RestrictedTokenTest, LockdownDefaultDaclNoLogonSid) {
  ASSERT_TRUE(::ImpersonateAnonymousToken(::GetCurrentThread()));
  std::optional<base::win::AccessToken> anonymous_token =
      base::win::AccessToken::FromCurrentThread(/*open_as_self=*/true,
                                                TOKEN_ALL_ACCESS);
  ::RevertToSelf();
  ASSERT_TRUE(anonymous_token);
  // Verify that the anonymous token doesn't have the logon sid.
  ASSERT_FALSE(anonymous_token->LogonId());

  RestrictedToken token;
  token.SetLockdownDefaultDacl();

  ASSERT_TRUE(token.GetRestrictedTokenForTesting(*anonymous_token));
}

TEST(RestrictedTokenTest, HardenProcessIntegrityLevelPolicy) {
  base::win::AccessToken token = GetPrimaryToken(0);
  EXPECT_EQ(HardenTokenIntegrityLevelPolicy(token), DWORD{ERROR_ACCESS_DENIED});
  token = GetPrimaryToken(READ_CONTROL | WRITE_OWNER);
  DWORD current_policy = GetMandatoryPolicy(token);
  EXPECT_EQ(HardenTokenIntegrityLevelPolicy(token), DWORD{ERROR_SUCCESS});
  EXPECT_EQ(GetMandatoryPolicy(token),
            current_policy | SYSTEM_MANDATORY_LABEL_NO_READ_UP |
                SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP);
}

TEST(RestrictedTokenTest, TokenType) {
  std::optional<base::win::AccessToken> token =
      CreateRestrictedToken(USER_LOCKDOWN, INTEGRITY_LEVEL_LAST,
                            TokenType::kPrimary, false, std::nullopt);
  ASSERT_TRUE(token);
  EXPECT_FALSE(token->IsImpersonation());
  EXPECT_EQ(DWORD{TOKEN_ALL_ACCESS}, base::win::GetGrantedAccess(token->get()));
  token = CreateRestrictedToken(USER_LOCKDOWN, INTEGRITY_LEVEL_LAST,
                                TokenType::kImpersonation, false, std::nullopt);
  ASSERT_TRUE(token);
  EXPECT_TRUE(token->IsImpersonation());
  EXPECT_EQ(token->ImpersonationLevel(),
            base::win::SecurityImpersonationLevel::kImpersonation);
  EXPECT_EQ(DWORD{TOKEN_ALL_ACCESS}, base::win::GetGrantedAccess(token->get()));
}

TEST(RestrictedTokenTest, UniqueSid) {
  CheckUniqueSid(USER_UNPROTECTED, false);
  CheckUniqueSid(USER_RESTRICTED_SAME_ACCESS, false);
  CheckUniqueSid(USER_RESTRICTED_NON_ADMIN, true);
  CheckUniqueSid(USER_INTERACTIVE, true);
  CheckUniqueSid(USER_LIMITED, true);
  CheckUniqueSid(USER_LOCKDOWN, true);
}

TEST(RestrictedTokenTest, IntegrityLevel) {
  CheckIntegrityLevel(INTEGRITY_LEVEL_LAST);
  CheckIntegrityLevel(INTEGRITY_LEVEL_MEDIUM);
  CheckIntegrityLevel(INTEGRITY_LEVEL_MEDIUM_LOW);
  CheckIntegrityLevel(INTEGRITY_LEVEL_LOW);
  CheckIntegrityLevel(INTEGRITY_LEVEL_BELOW_LOW);
  CheckIntegrityLevel(INTEGRITY_LEVEL_UNTRUSTED);
}

TEST(RestrictedTokenTest, Privileges) {
  CheckPrivileges(USER_UNPROTECTED, false, false);
  CheckPrivileges(USER_RESTRICTED_SAME_ACCESS, false, false);
  CheckPrivileges(USER_RESTRICTED_NON_ADMIN, true, false);
  CheckPrivileges(USER_INTERACTIVE, true, false);
  CheckPrivileges(USER_LIMITED, true, false);
  CheckPrivileges(USER_LOCKDOWN, true, true);
}

TEST(RestrictedTokenTest, Restricted) {
  CheckRestricted(USER_UNPROTECTED, {}, false, false);
  CheckRestricted(
      USER_RESTRICTED_NON_ADMIN,
      {base::win::WellKnownSid::kBuiltinUsers, base::win::WellKnownSid::kWorld,
       base::win::WellKnownSid::kInteractive,
       base::win::WellKnownSid::kAuthenticatedUser,
       base::win::WellKnownSid::kRestricted},
      true, true);
  CheckRestricted(
      USER_INTERACTIVE,
      {base::win::WellKnownSid::kBuiltinUsers, base::win::WellKnownSid::kWorld,
       base::win::WellKnownSid::kRestricted},
      true, true);
  CheckRestricted(
      USER_LIMITED,
      {base::win::WellKnownSid::kBuiltinUsers, base::win::WellKnownSid::kWorld,
       base::win::WellKnownSid::kRestricted},
      false, true);
  CheckRestricted(USER_LOCKDOWN, {base::win::WellKnownSid::kNull}, false,
                  false);

  std::optional<base::win::AccessToken> token =
      CreateRestrictedToken(USER_RESTRICTED_SAME_ACCESS, INTEGRITY_LEVEL_LAST,
                            TokenType::kPrimary, false, std::nullopt);
  ASSERT_TRUE(token);
  std::vector<base::win::Sid> sids;
  sids.push_back(token->User());
  for (const base::win::AccessToken::Group& group : token->Groups()) {
    if (!group.IsIntegrity()) {
      sids.push_back(group.GetSid().Clone());
    }
  }
  CheckRestricted(*token, sids);
}

TEST(RestrictedTokenTest, DenyOnly) {
  CompareDenyOnly(USER_UNPROTECTED, {}, true, false);
  CompareDenyOnly(USER_RESTRICTED_SAME_ACCESS, {}, true, false);
  CompareDenyOnly(
      USER_RESTRICTED_NON_ADMIN,
      {base::win::WellKnownSid::kBuiltinUsers, base::win::WellKnownSid::kWorld,
       base::win::WellKnownSid::kInteractive,
       base::win::WellKnownSid::kAuthenticatedUser},
      false, false);
  CompareDenyOnly(
      USER_INTERACTIVE,
      {base::win::WellKnownSid::kBuiltinUsers, base::win::WellKnownSid::kWorld,
       base::win::WellKnownSid::kInteractive,
       base::win::WellKnownSid::kAuthenticatedUser},
      false, false);
  CompareDenyOnly(
      USER_LIMITED,
      {base::win::WellKnownSid::kBuiltinUsers, base::win::WellKnownSid::kWorld,
       base::win::WellKnownSid::kInteractive},
      false, false);
  CompareDenyOnly(USER_LOCKDOWN, {}, false, true);
}

}  // namespace sandbox
