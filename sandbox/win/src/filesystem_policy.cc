// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/filesystem_policy.h"

#include <stdint.h>

#include <string>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/ipc_tags.h"
#include "sandbox/win/src/policy_engine_opcodes.h"
#include "sandbox/win/src/policy_params.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/sandbox_utils.h"
#include "sandbox/win/src/win_utils.h"

namespace {

NTSTATUS NtCreateFileInTarget(HANDLE* target_file_handle,
                              ACCESS_MASK desired_access,
                              OBJECT_ATTRIBUTES* obj_attributes,
                              IO_STATUS_BLOCK* io_status_block,
                              ULONG file_attributes,
                              ULONG share_access,
                              ULONG create_disposition,
                              ULONG create_options,
                              PVOID ea_buffer,
                              ULONG ea_length,
                              HANDLE target_process) {
  NtCreateFileFunction NtCreateFile = nullptr;
  ResolveNTFunctionPtr("NtCreateFile", &NtCreateFile);

  HANDLE local_handle = INVALID_HANDLE_VALUE;
  NTSTATUS status =
      NtCreateFile(&local_handle, desired_access, obj_attributes,
                   io_status_block, nullptr, file_attributes, share_access,
                   create_disposition, create_options, ea_buffer, ea_length);
  if (!NT_SUCCESS(status)) {
    return status;
  }

  if (!sandbox::SameObject(local_handle, obj_attributes->ObjectName->Buffer)) {
    // The handle points somewhere else. Fail the operation.
    ::CloseHandle(local_handle);
    return STATUS_ACCESS_DENIED;
  }

  if (!::DuplicateHandle(::GetCurrentProcess(), local_handle, target_process,
                         target_file_handle, 0, false,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    return STATUS_ACCESS_DENIED;
  }
  return STATUS_SUCCESS;
}

// Get an initialized anonymous level Security QOS.
SECURITY_QUALITY_OF_SERVICE GetAnonymousQOS() {
  SECURITY_QUALITY_OF_SERVICE security_qos = {0};
  security_qos.Length = sizeof(security_qos);
  security_qos.ImpersonationLevel = SecurityAnonymous;
  // Set dynamic tracking so that a pipe doesn't capture the broker's token
  security_qos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
  security_qos.EffectiveOnly = true;

  return security_qos;
}

}  // namespace.

namespace sandbox {

bool FileSystemPolicy::GenerateRules(const wchar_t* name,
                                     TargetPolicy::Semantics semantics,
                                     LowLevelPolicy* policy) {
  std::wstring mod_name(name);
  if (mod_name.empty()) {
    return false;
  }

  if (!PreProcessName(&mod_name)) {
    // The path to be added might contain a reparse point.
    NOTREACHED();
    return false;
  }

  // TODO(cpu) bug 32224: This prefix add is a hack because we don't have the
  // infrastructure to normalize names. In any case we need to escape the
  // question marks.
  if (_wcsnicmp(mod_name.c_str(), kNTDevicePrefix, kNTDevicePrefixLen)) {
    mod_name = FixNTPrefixForMatch(mod_name);
    name = mod_name.c_str();
  }

  EvalResult result = ASK_BROKER;

  // List of supported calls for the filesystem.
  const unsigned kCallNtCreateFile = 0x1;
  const unsigned kCallNtOpenFile = 0x2;
  const unsigned kCallNtQueryAttributesFile = 0x4;
  const unsigned kCallNtQueryFullAttributesFile = 0x8;
  const unsigned kCallNtSetInfoRename = 0x10;

  DWORD rule_to_add = kCallNtOpenFile | kCallNtCreateFile |
                      kCallNtQueryAttributesFile |
                      kCallNtQueryFullAttributesFile | kCallNtSetInfoRename;

  PolicyRule create(result);
  PolicyRule open(result);
  PolicyRule query(result);
  PolicyRule query_full(result);
  PolicyRule rename(result);

  switch (semantics) {
    case TargetPolicy::FILES_ALLOW_DIR_ANY: {
      open.AddNumberMatch(IF, OpenFile::OPTIONS, FILE_DIRECTORY_FILE, AND);
      create.AddNumberMatch(IF, OpenFile::OPTIONS, FILE_DIRECTORY_FILE, AND);
      break;
    }
    case TargetPolicy::FILES_ALLOW_READONLY: {
      // We consider all flags that are not known to be readonly as potentially
      // used for write.
      DWORD allowed_flags = FILE_READ_DATA | FILE_READ_ATTRIBUTES |
                            FILE_READ_EA | SYNCHRONIZE | FILE_EXECUTE |
                            GENERIC_READ | GENERIC_EXECUTE | READ_CONTROL;
      DWORD restricted_flags = ~allowed_flags;
      open.AddNumberMatch(IF_NOT, OpenFile::ACCESS, restricted_flags, AND);
      open.AddNumberMatch(IF, OpenFile::DISPOSITION, FILE_OPEN, EQUAL);
      create.AddNumberMatch(IF_NOT, OpenFile::ACCESS, restricted_flags, AND);
      create.AddNumberMatch(IF, OpenFile::DISPOSITION, FILE_OPEN, EQUAL);

      // Read only access don't work for rename.
      rule_to_add &= ~kCallNtSetInfoRename;
      break;
    }
    case TargetPolicy::FILES_ALLOW_QUERY: {
      // Here we don't want to add policy for the open or the create.
      rule_to_add &=
          ~(kCallNtOpenFile | kCallNtCreateFile | kCallNtSetInfoRename);
      break;
    }
    case TargetPolicy::FILES_ALLOW_ANY: {
      break;
    }
    default: {
      NOTREACHED();
      return false;
    }
  }

  if ((rule_to_add & kCallNtCreateFile) &&
      (!create.AddStringMatch(IF, OpenFile::NAME, name, CASE_INSENSITIVE) ||
       !policy->AddRule(IpcTag::NTCREATEFILE, &create))) {
    return false;
  }

  if ((rule_to_add & kCallNtOpenFile) &&
      (!open.AddStringMatch(IF, OpenFile::NAME, name, CASE_INSENSITIVE) ||
       !policy->AddRule(IpcTag::NTOPENFILE, &open))) {
    return false;
  }

  if ((rule_to_add & kCallNtQueryAttributesFile) &&
      (!query.AddStringMatch(IF, FileName::NAME, name, CASE_INSENSITIVE) ||
       !policy->AddRule(IpcTag::NTQUERYATTRIBUTESFILE, &query))) {
    return false;
  }

  if ((rule_to_add & kCallNtQueryFullAttributesFile) &&
      (!query_full.AddStringMatch(IF, FileName::NAME, name, CASE_INSENSITIVE) ||
       !policy->AddRule(IpcTag::NTQUERYFULLATTRIBUTESFILE, &query_full))) {
    return false;
  }

  if ((rule_to_add & kCallNtSetInfoRename) &&
      (!rename.AddStringMatch(IF, FileName::NAME, name, CASE_INSENSITIVE) ||
       !policy->AddRule(IpcTag::NTSETINFO_RENAME, &rename))) {
    return false;
  }

  return true;
}

// Right now we insert two rules, to be evaluated before any user supplied rule:
// - go to the broker if the path doesn't look like the paths that we push on
//    the policy (namely \??\something).
// - go to the broker if it looks like this is a short-name path.
//
// It is possible to add a rule to go to the broker in any case; it would look
// something like:
//    rule = new PolicyRule(ASK_BROKER);
//    rule->AddNumberMatch(IF_NOT, FileName::BROKER, true, AND);
//    policy->AddRule(service, rule);
bool FileSystemPolicy::SetInitialRules(LowLevelPolicy* policy) {
  PolicyRule format(ASK_BROKER);
  PolicyRule short_name(ASK_BROKER);

  bool rv = format.AddNumberMatch(IF_NOT, FileName::BROKER, BROKER_TRUE, AND);
  rv &= format.AddStringMatch(IF_NOT, FileName::NAME, L"\\/?/?\\*",
                              CASE_SENSITIVE);

  rv &= short_name.AddNumberMatch(IF_NOT, FileName::BROKER, BROKER_TRUE, AND);
  rv &= short_name.AddStringMatch(IF, FileName::NAME, L"*~*", CASE_SENSITIVE);

  if (!rv || !policy->AddRule(IpcTag::NTCREATEFILE, &format))
    return false;

  if (!policy->AddRule(IpcTag::NTCREATEFILE, &short_name))
    return false;

  if (!policy->AddRule(IpcTag::NTOPENFILE, &format))
    return false;

  if (!policy->AddRule(IpcTag::NTOPENFILE, &short_name))
    return false;

  if (!policy->AddRule(IpcTag::NTQUERYATTRIBUTESFILE, &format))
    return false;

  if (!policy->AddRule(IpcTag::NTQUERYATTRIBUTESFILE, &short_name))
    return false;

  if (!policy->AddRule(IpcTag::NTQUERYFULLATTRIBUTESFILE, &format))
    return false;

  if (!policy->AddRule(IpcTag::NTQUERYFULLATTRIBUTESFILE, &short_name))
    return false;

  if (!policy->AddRule(IpcTag::NTSETINFO_RENAME, &format))
    return false;

  if (!policy->AddRule(IpcTag::NTSETINFO_RENAME, &short_name))
    return false;

  return true;
}

bool FileSystemPolicy::CreateFileAction(EvalResult eval_result,
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
                                        ULONG_PTR* io_information) {
  *handle = nullptr;
  // The only action supported is ASK_BROKER which means create the requested
  // file as specified.
  if (ASK_BROKER != eval_result) {
    *nt_status = STATUS_ACCESS_DENIED;
    return false;
  }
  IO_STATUS_BLOCK io_block = {};
  UNICODE_STRING uni_name = {};
  OBJECT_ATTRIBUTES obj_attributes = {};
  SECURITY_QUALITY_OF_SERVICE security_qos = GetAnonymousQOS();

  InitObjectAttribs(file, attributes, nullptr, &obj_attributes, &uni_name,
                    IsPipe(file) ? &security_qos : nullptr);
  *nt_status =
      NtCreateFileInTarget(handle, desired_access, &obj_attributes, &io_block,
                           file_attributes, share_access, create_disposition,
                           create_options, nullptr, 0, client_info.process);

  *io_information = io_block.Information;
  return true;
}

bool FileSystemPolicy::OpenFileAction(EvalResult eval_result,
                                      const ClientInfo& client_info,
                                      const std::wstring& file,
                                      uint32_t attributes,
                                      uint32_t desired_access,
                                      uint32_t share_access,
                                      uint32_t open_options,
                                      HANDLE* handle,
                                      NTSTATUS* nt_status,
                                      ULONG_PTR* io_information) {
  *handle = nullptr;
  // The only action supported is ASK_BROKER which means open the requested
  // file as specified.
  if (ASK_BROKER != eval_result) {
    *nt_status = STATUS_ACCESS_DENIED;
    return false;
  }
  // An NtOpen is equivalent to an NtCreate with FileAttributes = 0 and
  // CreateDisposition = FILE_OPEN.
  IO_STATUS_BLOCK io_block = {};
  UNICODE_STRING uni_name = {};
  OBJECT_ATTRIBUTES obj_attributes = {};
  SECURITY_QUALITY_OF_SERVICE security_qos = GetAnonymousQOS();

  InitObjectAttribs(file, attributes, nullptr, &obj_attributes, &uni_name,
                    IsPipe(file) ? &security_qos : nullptr);
  *nt_status = NtCreateFileInTarget(
      handle, desired_access, &obj_attributes, &io_block, 0, share_access,
      FILE_OPEN, open_options, nullptr, 0, client_info.process);

  *io_information = io_block.Information;
  return true;
}

bool FileSystemPolicy::QueryAttributesFileAction(
    EvalResult eval_result,
    const ClientInfo& client_info,
    const std::wstring& file,
    uint32_t attributes,
    FILE_BASIC_INFORMATION* file_info,
    NTSTATUS* nt_status) {
  // The only action supported is ASK_BROKER which means query the requested
  // file as specified.
  if (ASK_BROKER != eval_result) {
    *nt_status = STATUS_ACCESS_DENIED;
    return false;
  }

  NtQueryAttributesFileFunction NtQueryAttributesFile = nullptr;
  ResolveNTFunctionPtr("NtQueryAttributesFile", &NtQueryAttributesFile);

  UNICODE_STRING uni_name = {0};
  OBJECT_ATTRIBUTES obj_attributes = {0};
  SECURITY_QUALITY_OF_SERVICE security_qos = GetAnonymousQOS();

  InitObjectAttribs(file, attributes, nullptr, &obj_attributes, &uni_name,
                    IsPipe(file) ? &security_qos : nullptr);
  *nt_status = NtQueryAttributesFile(&obj_attributes, file_info);

  return true;
}

bool FileSystemPolicy::QueryFullAttributesFileAction(
    EvalResult eval_result,
    const ClientInfo& client_info,
    const std::wstring& file,
    uint32_t attributes,
    FILE_NETWORK_OPEN_INFORMATION* file_info,
    NTSTATUS* nt_status) {
  // The only action supported is ASK_BROKER which means query the requested
  // file as specified.
  if (ASK_BROKER != eval_result) {
    *nt_status = STATUS_ACCESS_DENIED;
    return false;
  }

  NtQueryFullAttributesFileFunction NtQueryFullAttributesFile = nullptr;
  ResolveNTFunctionPtr("NtQueryFullAttributesFile", &NtQueryFullAttributesFile);

  UNICODE_STRING uni_name = {0};
  OBJECT_ATTRIBUTES obj_attributes = {0};
  SECURITY_QUALITY_OF_SERVICE security_qos = GetAnonymousQOS();

  InitObjectAttribs(file, attributes, nullptr, &obj_attributes, &uni_name,
                    IsPipe(file) ? &security_qos : nullptr);
  *nt_status = NtQueryFullAttributesFile(&obj_attributes, file_info);

  return true;
}

bool FileSystemPolicy::SetInformationFileAction(EvalResult eval_result,
                                                const ClientInfo& client_info,
                                                HANDLE target_file_handle,
                                                void* file_info,
                                                uint32_t length,
                                                uint32_t info_class,
                                                IO_STATUS_BLOCK* io_block,
                                                NTSTATUS* nt_status) {
  // The only action supported is ASK_BROKER which means open the requested
  // file as specified.
  if (ASK_BROKER != eval_result) {
    *nt_status = STATUS_ACCESS_DENIED;
    return false;
  }

  NtSetInformationFileFunction NtSetInformationFile = nullptr;
  ResolveNTFunctionPtr("NtSetInformationFile", &NtSetInformationFile);

  HANDLE local_handle = nullptr;
  if (!::DuplicateHandle(client_info.process, target_file_handle,
                         ::GetCurrentProcess(), &local_handle, 0, false,
                         DUPLICATE_SAME_ACCESS)) {
    *nt_status = STATUS_ACCESS_DENIED;
    return false;
  }

  base::win::ScopedHandle handle(local_handle);

  FILE_INFORMATION_CLASS file_info_class =
      static_cast<FILE_INFORMATION_CLASS>(info_class);
  *nt_status = NtSetInformationFile(local_handle, io_block, file_info, length,
                                    file_info_class);

  return true;
}

bool PreProcessName(std::wstring* path) {
  ConvertToLongPath(path);

  if (ERROR_NOT_A_REPARSE_POINT == IsReparsePoint(*path))
    return true;

  // We can't process a reparsed file.
  return false;
}

std::wstring FixNTPrefixForMatch(const std::wstring& name) {
  std::wstring mod_name = name;

  // NT prefix escaped for rule matcher
  const wchar_t kNTPrefixEscaped[] = L"\\/?/?\\";
  const int kNTPrefixEscapedLen = base::size(kNTPrefixEscaped) - 1;

  if (0 != mod_name.compare(0, kNTPrefixLen, kNTPrefix)) {
    if (0 != mod_name.compare(0, kNTPrefixEscapedLen, kNTPrefixEscaped)) {
      // TODO(nsylvain): Find a better way to do name resolution. Right now we
      // take the name and we expand it.
      mod_name.insert(0, kNTPrefixEscaped);
    }
  } else {
    // Start of name matches NT prefix, replace with escaped format
    // Fixes bug: 334882
    mod_name.replace(0, kNTPrefixLen, kNTPrefixEscaped);
  }

  return mod_name;
}

}  // namespace sandbox
