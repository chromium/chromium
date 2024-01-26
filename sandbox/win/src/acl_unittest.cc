// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/acl.h"

#include <windows.h>

#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

void CheckGetIntegrityLevelSid(IntegrityLevel integrity_level,
                               const wchar_t* sddl) {
  std::optional<base::win::Sid> sddl_sid = base::win::Sid::FromSddlString(sddl);
  ASSERT_TRUE(sddl_sid);
  std::optional<DWORD> integrity_value = GetIntegrityLevelRid(integrity_level);
  ASSERT_TRUE(integrity_value);
  EXPECT_EQ(*sddl_sid, base::win::Sid::FromIntegrityLevel(*integrity_value));
}

void CheckSetObjectIntegrityLabel(DWORD mandatory_policy,
                                  IntegrityLevel integrity_level,
                                  DWORD expected_error = ERROR_SUCCESS) {
  base::win::ScopedHandle job(::CreateJobObject(nullptr, nullptr));
  DWORD result =
      SetObjectIntegrityLabel(job.get(), base::win::SecurityObjectType::kKernel,
                              mandatory_policy, integrity_level);
  EXPECT_EQ(result, expected_error);
  if (result != ERROR_SUCCESS)
    return;
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          job.get(), base::win::SecurityObjectType::kKernel,
          LABEL_SECURITY_INFORMATION);
  ASSERT_TRUE(sd);
  PACL sacl = sd->sacl()->get();
  ASSERT_EQ(sacl->AceCount, 1);
  LPVOID ace_ptr;
  ASSERT_TRUE(::GetAce(sacl, 0, &ace_ptr));
  PSYSTEM_MANDATORY_LABEL_ACE ace =
      static_cast<PSYSTEM_MANDATORY_LABEL_ACE>(ace_ptr);
  ASSERT_EQ(ace->Header.AceType, SYSTEM_MANDATORY_LABEL_ACE_TYPE);
  EXPECT_EQ(ace->Header.AceFlags, 0);
  EXPECT_EQ(ace->Mask, mandatory_policy);
  std::optional<DWORD> rid = GetIntegrityLevelRid(integrity_level);
  base::win::Sid sid = base::win::Sid::FromIntegrityLevel(*rid);
  ASSERT_TRUE(::IsValidSid(&ace->SidStart));
  EXPECT_TRUE(sid.Equal(&ace->SidStart));
}

}  // namespace

// Checks the functionality of GetIntegrityLevelRid.
TEST(AclTest, GetIntegrityLevelRid) {
  EXPECT_FALSE(GetIntegrityLevelRid(INTEGRITY_LEVEL_LAST));
  CheckGetIntegrityLevelSid(INTEGRITY_LEVEL_SYSTEM, L"S-1-16-16384");
  CheckGetIntegrityLevelSid(INTEGRITY_LEVEL_HIGH, L"S-1-16-12288");
  CheckGetIntegrityLevelSid(INTEGRITY_LEVEL_MEDIUM, L"S-1-16-8192");
  CheckGetIntegrityLevelSid(INTEGRITY_LEVEL_MEDIUM_LOW, L"S-1-16-6144");
  CheckGetIntegrityLevelSid(INTEGRITY_LEVEL_LOW, L"S-1-16-4096");
  CheckGetIntegrityLevelSid(INTEGRITY_LEVEL_BELOW_LOW, L"S-1-16-2048");
  CheckGetIntegrityLevelSid(INTEGRITY_LEVEL_UNTRUSTED, L"S-1-16-0");
}

// Checks the functionality of SetObjectIntegrityLabel.
TEST(AclTest, SetObjectIntegrityLabel) {
  EXPECT_EQ(
      SetObjectIntegrityLabel(nullptr, base::win::SecurityObjectType::kKernel,
                              0, INTEGRITY_LEVEL_LAST),
      DWORD{ERROR_INVALID_SID});
  EXPECT_EQ(
      SetObjectIntegrityLabel(nullptr, base::win::SecurityObjectType::kKernel,
                              0, INTEGRITY_LEVEL_LOW),
      DWORD{ERROR_INVALID_HANDLE});
  // This assumes that the caller doesn't have SeRelabelPrivilege or is running
  // as a service process.
  CheckSetObjectIntegrityLabel(0, INTEGRITY_LEVEL_SYSTEM, ERROR_INVALID_LABEL);
  CheckSetObjectIntegrityLabel(0, INTEGRITY_LEVEL_LOW);
  CheckSetObjectIntegrityLabel(0, INTEGRITY_LEVEL_UNTRUSTED);
  CheckSetObjectIntegrityLabel(SYSTEM_MANDATORY_LABEL_NO_WRITE_UP,
                               INTEGRITY_LEVEL_LOW);
  CheckSetObjectIntegrityLabel(SYSTEM_MANDATORY_LABEL_NO_READ_UP,
                               INTEGRITY_LEVEL_LOW);
  CheckSetObjectIntegrityLabel(SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP,
                               INTEGRITY_LEVEL_LOW);
  CheckSetObjectIntegrityLabel(SYSTEM_MANDATORY_LABEL_NO_WRITE_UP |
                                   SYSTEM_MANDATORY_LABEL_NO_READ_UP |
                                   SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP,
                               INTEGRITY_LEVEL_LOW);
}

}  // namespace sandbox
