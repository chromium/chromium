// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_FILE_PERMISSION_POLICY_H_
#define STORAGE_BROWSER_FILE_SYSTEM_FILE_PERMISSION_POLICY_H_

namespace storage {

enum FilePermissionPolicy {
  // Any access should be always denied.
  FILE_PERMISSION_ALWAYS_DENY = 0x0,

  // Access is sandboxed, no extra permission check is necessary.
  FILE_PERMISSION_SANDBOX = 1 << 0,

  // Access should be restricted to read-only.
  FILE_PERMISSION_READ_ONLY = 1 << 1,

  // Access should be examined by per-file permission policy.
  FILE_PERMISSION_USE_FILE_PERMISSION = 1 << 2,
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_FILE_PERMISSION_POLICY_H_
