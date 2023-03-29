// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains unit tests for the job object.

#include "sandbox/win/src/job.h"

#include "base/win/scoped_process_information.h"
#include "sandbox/win/src/security_level.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Tests the creation and destruction of the job.
TEST(JobTest, TestCreation) {
  // Create the job.
  Job job;
  ASSERT_FALSE(job.IsValid());
  ASSERT_EQ(nullptr, job.GetHandle());
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job.Init(JobLevel::kLockdown, 0, 0));
  EXPECT_TRUE(job.IsValid());
  EXPECT_NE(nullptr, job.GetHandle());
}

// Tests the ui exceptions
TEST(JobTest, TestExceptions) {
  HANDLE job_handle;
  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(
        static_cast<DWORD>(ERROR_SUCCESS),
        job.Init(JobLevel::kLockdown, JOB_OBJECT_UILIMIT_READCLIPBOARD, 0));

    job_handle = job.GetHandle();
    ASSERT_NE(nullptr, job_handle);

    JOBOBJECT_BASIC_UI_RESTRICTIONS jbur = {0};
    DWORD size = sizeof(jbur);
    ASSERT_TRUE(::QueryInformationJobObject(
        job_handle, JobObjectBasicUIRestrictions, &jbur, size, &size));

    ASSERT_EQ(0u, jbur.UIRestrictionsClass & JOB_OBJECT_UILIMIT_READCLIPBOARD);
  }

  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
              job.Init(JobLevel::kLockdown, 0, 0));

    job_handle = job.GetHandle();
    ASSERT_NE(nullptr, job_handle);

    JOBOBJECT_BASIC_UI_RESTRICTIONS jbur = {0};
    DWORD size = sizeof(jbur);
    ASSERT_TRUE(::QueryInformationJobObject(
        job_handle, JobObjectBasicUIRestrictions, &jbur, size, &size));

    ASSERT_EQ(static_cast<DWORD>(JOB_OBJECT_UILIMIT_READCLIPBOARD),
              jbur.UIRestrictionsClass & JOB_OBJECT_UILIMIT_READCLIPBOARD);
  }
}

// Tests the error case when the job is initialized twice.
TEST(JobTest, DoubleInit) {
  // Create the job.
  Job job;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job.Init(JobLevel::kLockdown, 0, 0));
  ASSERT_EQ(static_cast<DWORD>(ERROR_ALREADY_INITIALIZED),
            job.Init(JobLevel::kLockdown, 0, 0));
}

// Tests the initialization of the job with different security levels.
TEST(JobTest, SecurityLevel) {
  Job job_lockdown;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job_lockdown.Init(JobLevel::kLockdown, 0, 0));

  Job job_limited_user;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job_limited_user.Init(JobLevel::kLimitedUser, 0, 0));

  Job job_interactive;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job_interactive.Init(JobLevel::kInteractive, 0, 0));

  Job job_unprotected;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job_unprotected.Init(JobLevel::kUnprotected, 0, 0));
}

}  // namespace sandbox
