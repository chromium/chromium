// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_security_helper.h"

#include "content/browser/child_process_security_policy_impl.h"
#include "ppapi/c/ppb_file_io.h"

namespace content {

namespace {

template <typename CanRead,
          typename CanWrite,
          typename CanCreate,
          typename CanCreateReadWrite,
          typename FileID>
bool CanOpenFileWithPepperFlags(CanRead can_read,
                                CanWrite can_write,
                                CanCreate can_create,
                                CanCreateReadWrite can_create_read_write,
                                int pp_open_flags,
                                int child_id,
                                const FileID& file) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  bool pp_read = !!(pp_open_flags & PP_FILEOPENFLAG_READ);
  bool pp_write = !!(pp_open_flags & PP_FILEOPENFLAG_WRITE);
  bool pp_create = !!(pp_open_flags & PP_FILEOPENFLAG_CREATE);
  bool pp_truncate = !!(pp_open_flags & PP_FILEOPENFLAG_TRUNCATE);
  bool pp_exclusive = !!(pp_open_flags & PP_FILEOPENFLAG_EXCLUSIVE);
  bool pp_append = !!(pp_open_flags & PP_FILEOPENFLAG_APPEND);

  if (pp_read && !(policy->*can_read)(child_id, file))
    return false;

  if (pp_write && !(policy->*can_write)(child_id, file))
    return false;

  // TODO(tommycli): Maybe tighten up required permission. crbug.com/284792
  if (pp_append && !(policy->*can_create_read_write)(child_id, file))
    return false;

  if (pp_truncate && !pp_write)
    return false;

  if (pp_create) {
    if (pp_exclusive) {
      return (policy->*can_create)(child_id, file);
    } else {
      // Asks for too much, but this is the only grant that allows overwrite.
      return (policy->*can_create_read_write)(child_id, file);
    }
  } else if (pp_truncate) {
    return (policy->*can_create_read_write)(child_id, file);
  }

  return true;
}
}

bool CanOpenWithPepperFlags(int pp_open_flags,
                            int child_id,
                            const base::FilePath& file) {
  return CanOpenFileWithPepperFlags(
      &ChildProcessSecurityPolicyImpl::CanReadFile,
      &ChildProcessSecurityPolicyImpl::CanCreateReadWriteFile,
      &ChildProcessSecurityPolicyImpl::CanCreateReadWriteFile,
      &ChildProcessSecurityPolicyImpl::CanCreateReadWriteFile,
      pp_open_flags,
      child_id,
      file);
}

bool CanOpenFileSystemURLWithPepperFlags(int pp_open_flags,
                                         int child_id,
                                         const storage::FileSystemURL& url) {
  return CanOpenFileWithPepperFlags(
      &ChildProcessSecurityPolicyImpl::CanReadFileSystemFile,
      &ChildProcessSecurityPolicyImpl::CanWriteFileSystemFile,
      &ChildProcessSecurityPolicyImpl::CanCreateFileSystemFile,
      &ChildProcessSecurityPolicyImpl::CanCreateReadWriteFileSystemFile,
      pp_open_flags,
      child_id,
      url);
}

}  // namespace content
