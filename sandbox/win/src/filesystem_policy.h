// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_SRC_FILESYSTEM_POLICY_H__
#define SANDBOX_SRC_FILESYSTEM_POLICY_H__

#include <stdint.h>

#include <string>

#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/policy_low_level.h"
#include "sandbox/win/src/sandbox_policy.h"

namespace sandbox {

enum IsBroker { BROKER_FALSE, BROKER_TRUE };

// This class centralizes most of the knowledge related to file system policy
class FileSystemPolicy {
 public:
  // Creates the required low-level policy rules to evaluate a high-level
  // policy rule for File IO, in particular open or create actions.
  // 'name' is the file or directory name.
  // 'semantics' is the desired semantics for the open or create.
  // 'policy' is the policy generator to which the rules are going to be added.
  static bool GenerateRules(const wchar_t* name,
                            TargetPolicy::Semantics semantics,
                            LowLevelPolicy* policy);

  // Add basic file system rules.
  static bool SetInitialRules(LowLevelPolicy* policy);

  // Performs the desired policy action on a create request with an
  // API that is compatible with the IPC-received parameters.
  // 'client_info' : the target process that is making the request.
  // 'eval_result' : The desired policy action to accomplish.
  // 'file' : The target file or directory.
  static bool CreateFileAction(EvalResult eval_result,
                               const ClientInfo& client_info,
                               const std::wstring& file,
                               uint32_t attributes,
                               uint32_t desired_access,
                               uint32_t file_attributes,
                               uint32_t share_access,
                               uint32_t create_disposition,
                               uint32_t create_options,
                               HANDLE* handle,
                               NTSTATUS* nt_status,
                               ULONG_PTR* io_information);

  // Performs the desired policy action on an open request with an
  // API that is compatible with the IPC-received parameters.
  // 'client_info' : the target process that is making the request.
  // 'eval_result' : The desired policy action to accomplish.
  // 'file' : The target file or directory.
  static bool OpenFileAction(EvalResult eval_result,
                             const ClientInfo& client_info,
                             const std::wstring& file,
                             uint32_t attributes,
                             uint32_t desired_access,
                             uint32_t share_access,
                             uint32_t open_options,
                             HANDLE* handle,
                             NTSTATUS* nt_status,
                             ULONG_PTR* io_information);

  // Performs the desired policy action on a query request with an
  // API that is compatible with the IPC-received parameters.
  static bool QueryAttributesFileAction(EvalResult eval_result,
                                        const ClientInfo& client_info,
                                        const std::wstring& file,
                                        uint32_t attributes,
                                        FILE_BASIC_INFORMATION* file_info,
                                        NTSTATUS* nt_status);

  // Performs the desired policy action on a query request with an
  // API that is compatible with the IPC-received parameters.
  static bool QueryFullAttributesFileAction(
      EvalResult eval_result,
      const ClientInfo& client_info,
      const std::wstring& file,
      uint32_t attributes,
      FILE_NETWORK_OPEN_INFORMATION* file_info,
      NTSTATUS* nt_status);

  // Performs the desired policy action on a set_info request with an
  // API that is compatible with the IPC-received parameters.
  static bool SetInformationFileAction(EvalResult eval_result,
                                       const ClientInfo& client_info,
                                       HANDLE target_file_handle,
                                       void* file_info,
                                       uint32_t length,
                                       uint32_t info_class,
                                       IO_STATUS_BLOCK* io_block,
                                       NTSTATUS* nt_status);
};

// Expands the path and check if it's a reparse point. Returns false if the path
// cannot be trusted.
bool PreProcessName(std::wstring* path);

// Corrects global paths to have a correctly escaped NT prefix at the
// beginning. If the name has no NT prefix (either normal or escaped)
// add the escaped form to the string
std::wstring FixNTPrefixForMatch(const std::wstring& name);

}  // namespace sandbox

#endif  // SANDBOX_SRC_FILESYSTEM_POLICY_H__
