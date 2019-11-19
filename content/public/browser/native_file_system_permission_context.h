// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_

#include "base/files/file_path.h"
#include "content/public/browser/native_file_system_permission_grant.h"
#include "content/public/browser/native_file_system_write_item.h"
#include "url/origin.h"

namespace content {

// Entry point to an embedder implemented permission context for the Native File
// System API. Instances of this class can be retrieved via a BrowserContext.
// All these methods must always be called on the UI thread.
class NativeFileSystemPermissionContext {
 public:
  // The type of action a user took that resulted in needing a permission grant
  // for a particular path. This is used to signal to the permission context if
  // the path was the result of a "save" operation, which an implementation can
  // use to automatically grant write access to the path.
  enum class UserAction {
    // The path for which a permission grant is requested was the result of a
    // "open" dialog, and as such the grant should probably not start out as
    // granted.
    kOpen,
    // The path for which a permission grant is requested was the result of a
    // "save" dialog, and as such it could make sense to return a grant that
    // immediately allows write access without needing to request it.
    kSave,
  };

  // Returns the read permission grant to use for a particular path.
  // |process_id| and |frame_id| are the frame in which the handle is used. Once
  // postMessage is implemented this isn't meaningful anymore and should be
  // removed, but until then they can be used for more accurate usage tracking.
  // TODO(https://crbug.com/984769): Eliminate process_id and frame_id from
  // this method when grants stop being scoped to a frame.
  virtual scoped_refptr<NativeFileSystemPermissionGrant> GetReadPermissionGrant(
      const url::Origin& origin,
      const base::FilePath& path,
      bool is_directory,
      int process_id,
      int frame_id) = 0;

  // Returns the permission grant to use for a particular path. This could be a
  // grant that applies to more than just the path passed in, for example if a
  // user has already granted write access to a directory, this method could
  // return that existing grant when figuring the grant to use for a file in
  // that directory.
  // |process_id| and |frame_id| are the frame in which the handle is used. Once
  // postMessage is implemented this isn't meaningful anymore and should be
  // removed, but until then they can be used for more accurate usage tracking.
  // TODO(https://crbug.com/984769): Eliminate process_id and frame_id from
  // this method when grants stop being scoped to a frame.
  virtual scoped_refptr<NativeFileSystemPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const base::FilePath& path,
                          bool is_directory,
                          int process_id,
                          int frame_id,
                          UserAction user_action) = 0;

  // Displays a dialog to confirm that the user intended to give read access to
  // a specific directory.
  using PermissionStatus = blink::mojom::PermissionStatus;
  virtual void ConfirmDirectoryReadAccess(
      const url::Origin& origin,
      const base::FilePath& path,
      int process_id,
      int frame_id,
      base::OnceCallback<void(PermissionStatus)> callback) = 0;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SensitiveDirectoryResult {
    kAllowed = 0,   // Access to directory is okay.
    kTryAgain = 1,  // User should pick a different directory.
    kAbort = 2,     // Abandon entirely, as if picking was cancelled.
    kMaxValue = kAbort
  };
  // Checks if access to the given |paths| should be allowed or blocked. This is
  // used to implement blocks for certain sensitive directories such as the
  // "Windows" system directory, as well as the root of the "home" directory.
  // Calls |callback| with the result of the check, after potentially showing
  // some UI to the user if the path should not be accessed.
  virtual void ConfirmSensitiveDirectoryAccess(
      const url::Origin& origin,
      const std::vector<base::FilePath>& paths,
      bool is_directory,
      int process_id,
      int frame_id,
      base::OnceCallback<void(SensitiveDirectoryResult)> callback) = 0;

  enum class AfterWriteCheckResult { kAllow, kBlock };
  // Runs a recently finished write operation through checks such as malware
  // or other security checks to determine if the write should be allowed.
  virtual void PerformAfterWriteChecks(
      std::unique_ptr<NativeFileSystemWriteItem> item,
      int process_id,
      int frame_id,
      base::OnceCallback<void(AfterWriteCheckResult)> callback) = 0;

  // Returns whether the given |origin| is allowed to ask for write access.
  // This is used to block save file dialogs from being shown
  // if an origin isn't allowed to ask for write access.
  virtual bool CanRequestWritePermission(const url::Origin& origin) = 0;

 protected:
  virtual ~NativeFileSystemPermissionContext() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_CONTEXT_H_
