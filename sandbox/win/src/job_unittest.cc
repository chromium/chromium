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
  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
              job.Init(JobLevel::kLockdown, L"my_test_job_name", 0, 0));

    // check if the job exists.
    HANDLE job_handle =
        ::OpenJobObjectW(GENERIC_ALL, false, L"my_test_job_name");
    ASSERT_TRUE(job_handle);

    if (job_handle)
      CloseHandle(job_handle);
  }

  // Check if the job is destroyed when the object goes out of scope.
  HANDLE job_handle = ::OpenJobObjectW(GENERIC_ALL, false, L"my_test_job_name");
  ASSERT_TRUE(!job_handle);
  ASSERT_EQ(static_cast<DWORD>(ERROR_FILE_NOT_FOUND), ::GetLastError());
}

// Tests the ui exceptions
TEST(JobTest, TestExceptions) {
  HANDLE job_handle;
  // Scope the creation of Job.
  {
    // Create the job.
    Job job;
    ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
              job.Init(JobLevel::kLockdown, L"my_test_job_name",
                       JOB_OBJECT_UILIMIT_READCLIPBOARD, 0));

    job_handle = job.GetHandle();
    ASSERT_TRUE(job_handle != INVALID_HANDLE_VALUE);

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
              job.Init(JobLevel::kLockdown, L"my_test_job_name", 0, 0));

    job_handle = job.GetHandle();
    ASSERT_TRUE(job_handle != INVALID_HANDLE_VALUE);

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
            job.Init(JobLevel::kLockdown, L"my_test_job_name", 0, 0));
  ASSERT_EQ(static_cast<DWORD>(ERROR_ALREADY_INITIALIZED),
            job.Init(JobLevel::kLockdown, L"test", 0, 0));
}

// Tests the error case when we use a method and the object is not yet
// initialized.
TEST(JobTest, NoInit) {
  Job job;
  ASSERT_EQ(static_cast<DWORD>(ERROR_NO_DATA),
            job.UserHandleGrantAccess(nullptr));
  ASSERT_EQ(static_cast<DWORD>(ERROR_NO_DATA), job.AssignProcessToJob(nullptr));
  ASSERT_FALSE(job.GetHandle() == INVALID_HANDLE_VALUE);
}

// Tests the initialization of the job with different security levels.
TEST(JobTest, SecurityLevel) {
  Job job_lockdown;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job_lockdown.Init(JobLevel::kLockdown, L"job_lockdown", 0, 0));

  Job job_limited_user;
  ASSERT_EQ(
      static_cast<DWORD>(ERROR_SUCCESS),
      job_limited_user.Init(JobLevel::kLimitedUser, L"job_limited_user", 0, 0));

  Job job_interactive;
  ASSERT_EQ(
      static_cast<DWORD>(ERROR_SUCCESS),
      job_interactive.Init(JobLevel::kInteractive, L"job_interactive", 0, 0));

  Job job_unprotected;
  ASSERT_EQ(
      static_cast<DWORD>(ERROR_SUCCESS),
      job_unprotected.Init(JobLevel::kUnprotected, L"job_unprotected", 0, 0));

  // JobLevel::kNone means we run without a job object so Init should fail.
  Job job_none;
  ASSERT_EQ(static_cast<DWORD>(ERROR_BAD_ARGUMENTS),
            job_none.Init(JobLevel::kNone, L"job_none", 0, 0));
}

// Tests the method "AssignProcessToJob".
TEST(JobTest, ProcessInJob) {
  // Create the job.
  Job job;
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job.Init(JobLevel::kUnprotected, L"job_test_process", 0, 0));

  wchar_t notepad[] = L"notepad";
  STARTUPINFO si = {sizeof(si)};
  PROCESS_INFORMATION temp_process_info = {};
  ASSERT_TRUE(::CreateProcess(nullptr, notepad, nullptr, nullptr, false, 0,
                              nullptr, nullptr, &si, &temp_process_info));
  base::win::ScopedProcessInformation pi(temp_process_info);
  ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
            job.AssignProcessToJob(pi.process_handle()));

  // Get the job handle.
  HANDLE job_handle = job.GetHandle();

  // Check if the process is in the job.
  JOBOBJECT_BASIC_PROCESS_ID_LIST jbpidl = {0};
  DWORD size = sizeof(jbpidl);
  EXPECT_TRUE(::QueryInformationJobObject(
      job_handle, JobObjectBasicProcessIdList, &jbpidl, size, &size));

  EXPECT_EQ(1u, jbpidl.NumberOfAssignedProcesses);
  EXPECT_EQ(1u, jbpidl.NumberOfProcessIdsInList);
  EXPECT_EQ(pi.process_id(), jbpidl.ProcessIdList[0]);

  EXPECT_TRUE(::TerminateProcess(pi.process_handle(), 0));
}

}  // namespace sandbox
