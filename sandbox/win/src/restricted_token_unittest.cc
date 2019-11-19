// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for the RestrictedToken.

#include "sandbox/win/src/restricted_token.h"

#include <vector>

#include "base/win/atl.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/acl.h"
#include "sandbox/win/src/security_capabilities.h"
#include "sandbox/win/src/sid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

void TestDefaultDalc(bool restricted_required) {
  RestrictedToken token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  if (!restricted_required)
    token.SetLockdownDefaultDacl();

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSid(ATL::Sids::World().GetPSID()));

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

  unsigned int ace_count = dacl.GetAceCount();
  for (unsigned int i = 0; i < ace_count; ++i) {
    ATL::CSid sid;
    ACCESS_MASK mask = 0;
    dacl.GetAclEntry(i, &sid, &mask);
    if (sid == ATL::Sids::RestrictedCode() && mask == GENERIC_ALL) {
      restricted_found = true;
    } else if (sid == logon_sid) {
      logon_sid_found = true;
    }
  }

  ASSERT_EQ(restricted_required, restricted_found);
  if (!restricted_required)
    ASSERT_FALSE(logon_sid_found);
}

bool GetVariableTokenInformation(HANDLE token,
                                 TOKEN_INFORMATION_CLASS information_class,
                                 std::vector<char>* information) {
  DWORD return_length;
  if (!::GetTokenInformation(token, information_class, nullptr, 0,
                             &return_length)) {
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      return false;
    }
  }

  information->resize(return_length);
  return !!::GetTokenInformation(token, information_class, information->data(),
                                 return_length, &return_length);
}

bool GetVariableTokenInformation(const base::win::ScopedHandle& token,
                                 TOKEN_INFORMATION_CLASS information_class,
                                 std::vector<char>* information) {
  return GetVariableTokenInformation(token.Get(), information_class,
                                     information);
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

  ATL::CSid package_sid(
      static_cast<SID*>(security_capabilities->AppContainerSid));
  ATL::CSid all_package_sid(
      static_cast<SID*>(sandbox::Sid(::WinBuiltinAnyPackageSid).GetPSID()));

  unsigned int ace_count = dacl.GetAceCount();
  for (unsigned int i = 0; i < ace_count; ++i) {
    ATL::CSid sid;
    ACCESS_MASK mask = 0;
    BYTE type = 0;
    dacl.GetAclEntry(i, &sid, &mask, &type);
    if (mask != TOKEN_ALL_ACCESS || type != ACCESS_ALLOWED_ACE_TYPE)
      continue;
    if (sid == package_sid)
      EXPECT_TRUE(package_sid_required);
    else if (sid == all_package_sid)
      EXPECT_FALSE(package_sid_required);
  }
}

void CheckLowBoxToken(const base::win::ScopedHandle& token,
                      TOKEN_TYPE token_type,
                      PSECURITY_CAPABILITIES security_capabilities) {
  DWORD appcontainer;
  DWORD return_length;
  ASSERT_TRUE(::GetTokenInformation(token.Get(), ::TokenIsAppContainer,
                                    &appcontainer, sizeof(appcontainer),
                                    &return_length));
  ASSERT_TRUE(appcontainer);
  TOKEN_TYPE token_type_real;
  ASSERT_TRUE(::GetTokenInformation(token.Get(), ::TokenType, &token_type_real,
                                    sizeof(token_type_real), &return_length));
  ASSERT_EQ(token_type_real, token_type);
  if (token_type == ::TokenImpersonation) {
    SECURITY_IMPERSONATION_LEVEL imp_level;
    ASSERT_TRUE(::GetTokenInformation(token.Get(), ::TokenImpersonationLevel,
                                      &imp_level, sizeof(imp_level),
                                      &return_length));
    ASSERT_EQ(imp_level, ::SecurityImpersonation);
  }

  std::vector<char> package_sid_buf;
  ASSERT_TRUE(GetVariableTokenInformation(token, ::TokenAppContainerSid,
                                          &package_sid_buf));
  PTOKEN_APPCONTAINER_INFORMATION package_sid =
      reinterpret_cast<PTOKEN_APPCONTAINER_INFORMATION>(package_sid_buf.data());
  EXPECT_TRUE(::EqualSid(security_capabilities->AppContainerSid,
                         package_sid->TokenAppContainer));

  std::vector<char> capabilities_buf;
  ASSERT_TRUE(GetVariableTokenInformation(token, ::TokenCapabilities,
                                          &capabilities_buf));
  PTOKEN_GROUPS capabilities =
      reinterpret_cast<PTOKEN_GROUPS>(capabilities_buf.data());
  ASSERT_EQ(capabilities->GroupCount, security_capabilities->CapabilityCount);
  for (DWORD index = 0; index < capabilities->GroupCount; ++index) {
    EXPECT_EQ(capabilities->Groups[index].Attributes,
              security_capabilities->Capabilities[index].Attributes);
    EXPECT_TRUE(::EqualSid(capabilities->Groups[index].Sid,
                           security_capabilities->Capabilities[index].Sid));
  }

  CheckDaclForPackageSid(token, security_capabilities, true);
}

// Checks if a sid is in the restricting list of the restricted token.
// Asserts if it's not the case. If count is a positive number, the number of
// elements in the restricting sids list has to be equal.
void CheckRestrictingSid(HANDLE restricted_token, ATL::CSid sid, int count) {
  std::vector<char> memory;
  ASSERT_TRUE(GetVariableTokenInformation(restricted_token,
                                          ::TokenRestrictedSids, &memory));
  PTOKEN_GROUPS groups = reinterpret_cast<PTOKEN_GROUPS>(memory.data());
  ATL::CTokenGroups atl_groups(*groups);

  if (count >= 0)
    ASSERT_EQ(static_cast<unsigned>(count), atl_groups.GetCount());

  ATL::CSid::CSidArray sids;
  ATL::CAtlArray<DWORD> attributes;
  atl_groups.GetSidsAndAttributes(&sids, &attributes);

  bool present = false;
  for (unsigned int i = 0; i < sids.GetCount(); ++i) {
    if (sids[i] == sid) {
      present = true;
      break;
    }
  }

  ASSERT_TRUE(present);
}

void CheckRestrictingSid(const ATL::CAccessToken& restricted_token,
                         ATL::CSid sid,
                         int count) {
  CheckRestrictingSid(restricted_token.GetHandle(), sid, count);
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
  HANDLE token_handle = INVALID_HANDLE_VALUE;
  ASSERT_TRUE(::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                                 &token_handle));

  ASSERT_NE(INVALID_HANDLE_VALUE, token_handle);

  ATL::CAccessToken access_token;
  access_token.Attach(token_handle);

  // Create the token using the current token.
  RestrictedToken token_default;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token_default.Init(nullptr));

  // Get the handle to the restricted token.

  base::win::ScopedHandle restricted_token_handle;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token_default.GetRestrictedToken(&restricted_token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(restricted_token_handle.Take());

  ATL::CSid sid_user_restricted;
  ATL::CSid sid_user_default;
  ATL::CSid sid_owner_restricted;
  ATL::CSid sid_owner_default;
  ASSERT_TRUE(restricted_token.GetUser(&sid_user_restricted));
  ASSERT_TRUE(access_token.GetUser(&sid_user_default));
  ASSERT_TRUE(restricted_token.GetOwner(&sid_owner_restricted));
  ASSERT_TRUE(access_token.GetOwner(&sid_owner_default));

  // Check if both token have the same owner and user.
  ASSERT_EQ(sid_user_restricted, sid_user_default);
  ASSERT_EQ(sid_owner_restricted, sid_owner_default);
}

// Tests the initialization with a custom token as parameter.
TEST(RestrictedTokenTest, CustomInit) {
  // Get the current process token.
  HANDLE token_handle = INVALID_HANDLE_VALUE;
  ASSERT_TRUE(::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                                 &token_handle));

  ASSERT_NE(INVALID_HANDLE_VALUE, token_handle);

  ATL::CAccessToken access_token;
  access_token.Attach(token_handle);

  // Change the primary group.
  access_token.SetPrimaryGroup(ATL::Sids::World());

  // Create the token using the current token.
  RestrictedToken token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.Init(access_token.GetHandle()));

  // Get the handle to the restricted token.

  base::win::ScopedHandle restricted_token_handle;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&restricted_token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(restricted_token_handle.Take());

  ATL::CSid sid_restricted;
  ATL::CSid sid_default;
  ASSERT_TRUE(restricted_token.GetPrimaryGroup(&sid_restricted));
  ASSERT_TRUE(access_token.GetPrimaryGroup(&sid_default));

  // Check if both token have the same owner.
  ASSERT_EQ(sid_restricted, sid_default);
}

// Verifies that the token created by the object are valid.
TEST(RestrictedTokenTest, ResultToken) {
  RestrictedToken token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSid(ATL::Sids::World().GetPSID()));

  base::win::ScopedHandle restricted_token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&restricted_token));

  ASSERT_TRUE(::IsTokenRestricted(restricted_token.Get()));

  DWORD length = 0;
  TOKEN_TYPE type;
  ASSERT_TRUE(::GetTokenInformation(restricted_token.Get(), ::TokenType, &type,
                                    sizeof(type), &length));

  ASSERT_EQ(type, TokenPrimary);

  base::win::ScopedHandle impersonation_token;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedTokenForImpersonation(&impersonation_token));

  ASSERT_TRUE(::IsTokenRestricted(impersonation_token.Get()));

  ASSERT_TRUE(::GetTokenInformation(impersonation_token.Get(), ::TokenType,
                                    &type, sizeof(type), &length));

  ASSERT_EQ(type, TokenImpersonation);
}

// Verifies that the token created has "Restricted" in its default dacl.
TEST(RestrictedTokenTest, DefaultDacl) {
  TestDefaultDalc(true);
}

// Verifies that the token created does not have "Restricted" in its default
// dacl.
TEST(RestrictedTokenTest, DefaultDaclLockdown) {
  TestDefaultDalc(false);
}

// Tests the method "AddSidForDenyOnly".
TEST(RestrictedTokenTest, DenySid) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddSidForDenyOnly(Sid(WinWorldSid)));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());

  ATL::CTokenGroups groups;
  ASSERT_TRUE(restricted_token.GetGroups(&groups));

  ATL::CSid::CSidArray sids;
  ATL::CAtlArray<DWORD> attributes;
  groups.GetSidsAndAttributes(&sids, &attributes);

  for (unsigned int i = 0; i < sids.GetCount(); i++) {
    if (ATL::Sids::World() == sids[i]) {
      ASSERT_EQ(static_cast<DWORD>(SE_GROUP_USE_FOR_DENY_ONLY),
                attributes[i] & SE_GROUP_USE_FOR_DENY_ONLY);
    }
  }
}

// Tests the method "AddAllSidsForDenyOnly".
TEST(RestrictedTokenTest, DenySids) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddAllSidsForDenyOnly(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());

  ATL::CTokenGroups groups;
  ASSERT_TRUE(restricted_token.GetGroups(&groups));

  ATL::CSid::CSidArray sids;
  ATL::CAtlArray<DWORD> attributes;
  groups.GetSidsAndAttributes(&sids, &attributes);

  // Verify that all sids are really gone.
  for (unsigned int i = 0; i < sids.GetCount(); i++) {
    if ((attributes[i] & SE_GROUP_LOGON_ID) == 0 &&
        (attributes[i] & SE_GROUP_INTEGRITY) == 0) {
      ASSERT_EQ(static_cast<DWORD>(SE_GROUP_USE_FOR_DENY_ONLY),
                attributes[i] & SE_GROUP_USE_FOR_DENY_ONLY);
    }
  }
}

// Tests the method "AddAllSidsForDenyOnly" using an exception list.
TEST(RestrictedTokenTest, DenySidsException) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  std::vector<Sid> sids_exception;
  sids_exception.push_back(Sid(WinWorldSid));

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddAllSidsForDenyOnly(&sids_exception));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());

  ATL::CTokenGroups groups;
  ASSERT_TRUE(restricted_token.GetGroups(&groups));

  ATL::CSid::CSidArray sids;
  ATL::CAtlArray<DWORD> attributes;
  groups.GetSidsAndAttributes(&sids, &attributes);

  // Verify that all sids are really gone.
  for (unsigned int i = 0; i < sids.GetCount(); i++) {
    if ((attributes[i] & SE_GROUP_LOGON_ID) == 0 &&
        (attributes[i] & SE_GROUP_INTEGRITY) == 0) {
      if (ATL::Sids::World() == sids[i]) {
        ASSERT_EQ(0u, attributes[i] & SE_GROUP_USE_FOR_DENY_ONLY);
      } else {
        ASSERT_EQ(static_cast<DWORD>(SE_GROUP_USE_FOR_DENY_ONLY),
                  attributes[i] & SE_GROUP_USE_FOR_DENY_ONLY);
      }
    }
  }
}

// Tests test method AddOwnerSidForDenyOnly.
TEST(RestrictedTokenTest, DenyOwnerSid) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.AddUserSidForDenyOnly());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());

  ATL::CTokenGroups groups;
  ASSERT_TRUE(restricted_token.GetGroups(&groups));

  ATL::CSid::CSidArray sids;
  ATL::CAtlArray<DWORD> attributes;
  groups.GetSidsAndAttributes(&sids, &attributes);

  ATL::CSid user_sid;
  ASSERT_TRUE(restricted_token.GetUser(&user_sid));

  for (unsigned int i = 0; i < sids.GetCount(); ++i) {
    if (user_sid == sids[i]) {
      ASSERT_EQ(static_cast<DWORD>(SE_GROUP_USE_FOR_DENY_ONLY),
                attributes[i] & SE_GROUP_USE_FOR_DENY_ONLY);
    }
  }
}

// Tests test method AddOwnerSidForDenyOnly with a custom effective token.
TEST(RestrictedTokenTest, DenyOwnerSidCustom) {
  // Get the current process token.
  HANDLE access_handle = INVALID_HANDLE_VALUE;
  ASSERT_TRUE(::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                                 &access_handle));

  ASSERT_NE(INVALID_HANDLE_VALUE, access_handle);

  ATL::CAccessToken access_token;
  access_token.Attach(access_handle);

  RestrictedToken token;
  base::win::ScopedHandle token_handle;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.Init(access_token.GetHandle()));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.AddUserSidForDenyOnly());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());

  ATL::CTokenGroups groups;
  ASSERT_TRUE(restricted_token.GetGroups(&groups));

  ATL::CSid::CSidArray sids;
  ATL::CAtlArray<DWORD> attributes;
  groups.GetSidsAndAttributes(&sids, &attributes);

  ATL::CSid user_sid;
  ASSERT_TRUE(restricted_token.GetUser(&user_sid));

  for (unsigned int i = 0; i < sids.GetCount(); ++i) {
    if (user_sid == sids[i]) {
      ASSERT_EQ(static_cast<DWORD>(SE_GROUP_USE_FOR_DENY_ONLY),
                attributes[i] & SE_GROUP_USE_FOR_DENY_ONLY);
    }
  }
}

// Tests the method DeleteAllPrivileges.
TEST(RestrictedTokenTest, DeleteAllPrivileges) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.DeleteAllPrivileges(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());

  ATL::CTokenPrivileges privileges;
  ASSERT_TRUE(restricted_token.GetPrivileges(&privileges));

  ASSERT_EQ(0u, privileges.GetCount());
}

// Tests the method DeleteAllPrivileges with an exception list.
TEST(RestrictedTokenTest, DeleteAllPrivilegesException) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  std::vector<std::wstring> exceptions;
  exceptions.push_back(SE_CHANGE_NOTIFY_NAME);

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.DeleteAllPrivileges(&exceptions));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());

  ATL::CTokenPrivileges privileges;
  ASSERT_TRUE(restricted_token.GetPrivileges(&privileges));

  ATL::CTokenPrivileges::CNames privilege_names;
  ATL::CTokenPrivileges::CAttributes privilege_name_attributes;
  privileges.GetNamesAndAttributes(&privilege_names,
                                   &privilege_name_attributes);

  ASSERT_EQ(1u, privileges.GetCount());

  for (unsigned int i = 0; i < privileges.GetCount(); ++i) {
    ASSERT_EQ(privilege_names[i], SE_CHANGE_NOTIFY_NAME);
  }
}

// Tests the method DeletePrivilege.
TEST(RestrictedTokenTest, DeletePrivilege) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.DeletePrivilege(SE_CHANGE_NOTIFY_NAME));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());

  ATL::CTokenPrivileges privileges;
  ASSERT_TRUE(restricted_token.GetPrivileges(&privileges));

  ATL::CTokenPrivileges::CNames privilege_names;
  ATL::CTokenPrivileges::CAttributes privilege_name_attributes;
  privileges.GetNamesAndAttributes(&privilege_names,
                                   &privilege_name_attributes);

  for (unsigned int i = 0; i < privileges.GetCount(); ++i) {
    ASSERT_NE(privilege_names[i], SE_CHANGE_NOTIFY_NAME);
  }
}

// Tests the method AddRestrictingSid.
TEST(RestrictedTokenTest, AddRestrictingSid) {
  RestrictedToken token;
  base::win::ScopedHandle token_handle;

  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), token.Init(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSid(ATL::Sids::World().GetPSID()));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());

  CheckRestrictingSid(restricted_token, ATL::Sids::World(), 1);
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

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());
  ATL::CSid user;
  restricted_token.GetUser(&user);

  CheckRestrictingSid(restricted_token, user, 1);
}

// Tests the method AddRestrictingSidCurrentUser with a custom effective token.
TEST(RestrictedTokenTest, AddRestrictingSidCurrentUserCustom) {
  // Get the current process token.
  HANDLE access_handle = INVALID_HANDLE_VALUE;
  ASSERT_TRUE(::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                                 &access_handle));

  ASSERT_NE(INVALID_HANDLE_VALUE, access_handle);

  ATL::CAccessToken access_token;
  access_token.Attach(access_handle);

  RestrictedToken token;
  base::win::ScopedHandle token_handle;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.Init(access_token.GetHandle()));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.AddRestrictingSidCurrentUser());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());
  ATL::CSid user;
  restricted_token.GetUser(&user);

  CheckRestrictingSid(restricted_token, user, 1);
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

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());
  ATL::CSid session;
  restricted_token.GetLogonSid(&session);

  CheckRestrictingSid(restricted_token, session, 1);
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
            token.AddRestrictingSid(ATL::Sids::World().GetPSID()));
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            token.GetRestrictedToken(&token_handle));

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());
  ATL::CSid session;
  restricted_token.GetLogonSid(&session);

  std::vector<char> memory;
  ASSERT_TRUE(GetVariableTokenInformation(restricted_token.GetHandle(),
                                          ::TokenRestrictedSids, &memory));
  PTOKEN_GROUPS groups = reinterpret_cast<PTOKEN_GROUPS>(memory.data());
  ATL::CTokenGroups atl_groups(*groups);
  ASSERT_EQ(3u, atl_groups.GetCount());
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

  ATL::CAccessToken restricted_token;
  restricted_token.Attach(token_handle.Take());

  ATL::CTokenGroups groups;
  ASSERT_TRUE(restricted_token.GetGroups(&groups));

  ATL::CSid::CSidArray sids;
  ATL::CAtlArray<DWORD> attributes;
  groups.GetSidsAndAttributes(&sids, &attributes);

  // Verify that all group sids are in the restricting sid list.
  for (unsigned int i = 0; i < sids.GetCount(); i++) {
    if ((attributes[i] & SE_GROUP_INTEGRITY) == 0) {
      CheckRestrictingSid(restricted_token, sids[i], -1);
    }
  }

  // Verify that the user is in the restricting sid list.
  ATL::CSid user;
  restricted_token.GetUser(&user);
  CheckRestrictingSid(restricted_token, user, -1);
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
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;
  base::win::ScopedHandle token;

  Sid package_sid = Sid::FromSddlString(L"S-1-15-2-1-2-3-4-5-6-7");
  SecurityCapabilities caps_no_capabilities(package_sid);

  ASSERT_EQ(DWORD{ERROR_INVALID_PARAMETER},
            CreateLowBoxToken(nullptr, PRIMARY, &caps_no_capabilities, nullptr,
                              0, nullptr));
  ASSERT_EQ(DWORD{ERROR_SUCCESS},
            CreateLowBoxToken(nullptr, PRIMARY, &caps_no_capabilities, nullptr,
                              0, &token));
  ASSERT_TRUE(token.IsValid());
  CheckLowBoxToken(token, ::TokenPrimary, &caps_no_capabilities);

  ASSERT_TRUE(ReplacePackageSidInDacl(token.Get(), SE_KERNEL_OBJECT,
                                      Sid(caps_no_capabilities.AppContainerSid),
                                      TOKEN_ALL_ACCESS));
  CheckDaclForPackageSid(token, &caps_no_capabilities, false);

  ASSERT_EQ(DWORD{ERROR_SUCCESS},
            CreateLowBoxToken(nullptr, IMPERSONATION, &caps_no_capabilities,
                              nullptr, 0, &token));
  ASSERT_TRUE(token.IsValid());
  CheckLowBoxToken(token, ::TokenImpersonation, &caps_no_capabilities);

  std::vector<Sid> capabilities;
  capabilities.push_back(Sid::FromKnownCapability(kInternetClient));
  capabilities.push_back(Sid::FromKnownCapability(kPrivateNetworkClientServer));
  SecurityCapabilities caps_with_capabilities(package_sid, capabilities);
  ASSERT_EQ(DWORD{ERROR_SUCCESS},
            CreateLowBoxToken(nullptr, PRIMARY, &caps_with_capabilities,
                              nullptr, 0, &token));
  ASSERT_TRUE(token.IsValid());
  CheckLowBoxToken(token, ::TokenPrimary, &caps_with_capabilities);

  RestrictedToken restricted_token;
  base::win::ScopedHandle token_handle;
  ASSERT_EQ(DWORD{ERROR_SUCCESS}, restricted_token.Init(nullptr));
  ASSERT_EQ(DWORD{ERROR_SUCCESS},
            restricted_token.AddRestrictingSid(ATL::Sids::World().GetPSID()));
  ASSERT_EQ(DWORD{ERROR_SUCCESS},
            restricted_token.GetRestrictedToken(&token_handle));

  ASSERT_EQ(DWORD{ERROR_SUCCESS},
            CreateLowBoxToken(token_handle.Get(), PRIMARY,
                              &caps_with_capabilities, nullptr, 0, &token));
  ASSERT_TRUE(token.IsValid());
  CheckLowBoxToken(token, ::TokenPrimary, &caps_with_capabilities);
  CheckRestrictingSid(token.Get(), ATL::Sids::World(), 1);

  SecurityCapabilities caps_for_handles(
      Sid::FromSddlString(L"S-1-15-2-1-2-3-4-5-6-8"));
  base::win::ScopedHandle object_handle;
  ASSERT_EQ(DWORD{ERROR_SUCCESS},
            CreateLowBoxObjectDirectory(caps_for_handles.AppContainerSid, true,
                                        &object_handle));
  HANDLE saved_handles[] = {object_handle.Get()};

  ASSERT_EQ(DWORD{ERROR_SUCCESS},
            CreateLowBoxToken(token_handle.Get(), PRIMARY, &caps_for_handles,
                              saved_handles, 1, &token));
  ASSERT_TRUE(token.IsValid());
  object_handle.Close();
  ASSERT_FALSE(object_handle.IsValid());
  ASSERT_EQ(DWORD{ERROR_ALREADY_EXISTS},
            CreateLowBoxObjectDirectory(caps_for_handles.AppContainerSid, false,
                                        &object_handle));
}

}  // namespace sandbox
