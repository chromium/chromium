// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/job.h"

#include <windows.h>

#include <stddef.h>

#include <utility>

#include "sandbox/win/src/restricted_token.h"

namespace sandbox {

Job::Job() = default;
Job::~Job() = default;

DWORD Job::Init(JobLevel security_level,
                DWORD ui_exceptions,
                size_t memory_limit) {
  if (job_handle_.is_valid())
    return ERROR_ALREADY_INITIALIZED;

  job_handle_.Set(::CreateJobObject(nullptr, nullptr));
  if (!job_handle_.is_valid())
    return ::GetLastError();

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
  JOBOBJECT_BASIC_UI_RESTRICTIONS jbur = {};

  // Set the settings for the different security levels. Note: The higher levels
  // inherit from the lower levels.
  switch (security_level) {
    case JobLevel::kLockdown: {
      jeli.BasicLimitInformation.LimitFlags |=
          JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_WRITECLIPBOARD;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_READCLIPBOARD;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_HANDLES;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_GLOBALATOMS;
      [[fallthrough]];
    }
    case JobLevel::kLimitedUser: {
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_DISPLAYSETTINGS;
      jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
      jeli.BasicLimitInformation.ActiveProcessLimit = 1;
      [[fallthrough]];
    }
    case JobLevel::kInteractive: {
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_DESKTOP;
      jbur.UIRestrictionsClass |= JOB_OBJECT_UILIMIT_EXITWINDOWS;
      [[fallthrough]];
    }
    case JobLevel::kUnprotected: {
      if (memory_limit) {
        jeli.BasicLimitInformation.LimitFlags |=
            JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        jeli.ProcessMemoryLimit = memory_limit;
      }

      jeli.BasicLimitInformation.LimitFlags |=
          JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      break;
    }
  }

  if (!::SetInformationJobObject(job_handle_.get(),
                                 JobObjectExtendedLimitInformation, &jeli,
                                 sizeof(jeli))) {
    return ::GetLastError();
  }

  jbur.UIRestrictionsClass = jbur.UIRestrictionsClass & (~ui_exceptions);
  if (!::SetInformationJobObject(job_handle_.get(),
                                 JobObjectBasicUIRestrictions, &jbur,
                                 sizeof(jbur))) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

bool Job::IsValid() {
  return job_handle_.is_valid();
}

HANDLE Job::GetHandle() {
  return job_handle_.get();
}

DWORD Job::SetActiveProcessLimit(DWORD processes) {
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};

  if (!job_handle_.is_valid())
    return ERROR_NO_DATA;

  if (!::QueryInformationJobObject(job_handle_.get(),
                                   JobObjectExtendedLimitInformation, &jeli,
                                   sizeof(jeli), nullptr)) {
    return ::GetLastError();
  }
  jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
  jeli.BasicLimitInformation.ActiveProcessLimit = processes;

  if (!::SetInformationJobObject(job_handle_.get(),
                                 JobObjectExtendedLimitInformation, &jeli,
                                 sizeof(jeli))) {
    return ::GetLastError();
  }

  return ERROR_SUCCESS;
}

}  // namespace sandbox
