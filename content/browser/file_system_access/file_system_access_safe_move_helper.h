// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_SAFE_MOVE_HELPER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_SAFE_MOVE_HELPER_H_

#include "base/files/file.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/download/public/common/quarantine_connection.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/common/content_export.h"
#include "storage/browser/file_system/file_system_url.h"

namespace content {

// TODO(crbug.com/40198034): Support safely moving directories. For now, this
// class only supports moving files. Moving directories will require running
// safe browsing checks on all files before moving.
//
// Helper class which moves files (and eventually directories). Safe browsing
// checks are performed and the mark of the web is added for certain file system
// types, as appropriate.
class CONTENT_EXPORT FileSystemAccessSafeMoveHelper {
 public:
  using FileSystemAccessSafeMoveHelperCallback =
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>;

  FileSystemAccessSafeMoveHelper(
      base::WeakPtr<FileSystemAccessManagerImpl> manager,
      const FileSystemAccessManagerImpl::BindingContext& context,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& dest_url,
      storage::FileSystemOperation::CopyOrMoveOptionSet options,
      download::QuarantineConnectionCallback quarantine_connection_callback,
      bool has_transient_user_activation);
  FileSystemAccessSafeMoveHelper(const FileSystemAccessSafeMoveHelper&) =
      delete;
  FileSystemAccessSafeMoveHelper& operator=(
      const FileSystemAccessSafeMoveHelper&) = delete;
  ~FileSystemAccessSafeMoveHelper();

  void Start(FileSystemAccessSafeMoveHelperCallback callback);

  const storage::FileSystemURL& source_url() const { return source_url_; }
  const storage::FileSystemURL& dest_url() const { return dest_url_; }

  using HashCallback = base::OnceCallback<
      void(base::File::Error error, const std::string& hash, int64_t size)>;
  void ComputeHashForSourceFileForTesting(HashCallback callback) {
    ComputeHashForSourceFile(std::move(callback));
  }

  bool RequireAfterWriteChecksForTesting() const {
    return RequireAfterWriteChecks();
  }
  bool RequireQuarantineForTesting() const { return RequireQuarantine(); }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  void DoAfterWriteCheck(base::File::Error hash_result,
                         const std::string& hash,
                         int64_t size);
  void DidAfterWriteCheck(
      FileSystemAccessPermissionContext::AfterWriteCheckResult result);
  void DidFileSkipQuarantine(base::File::Error result);
  void DidFileDoQuarantine(
      const storage::FileSystemURL& target_url,
      const GURL& referrer_url,
      mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
      base::File::Error result);
  void DidAnnotateFile(
      mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
      quarantine::mojom::QuarantineFileResult result);

  void ComputeHashForSourceFile(HashCallback callback);

  // Safe browsing should apply to paths on all filesystems
  // except temporary file systems, or for same-file-system moves in which the
  // extension does not change.
  bool RequireAfterWriteChecks() const;
  // Quarantine checks should apply to paths on all filesystems except temporary
  // file systems.
  bool RequireQuarantine() const;

  base::WeakPtr<FileSystemAccessManagerImpl> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);
  FileSystemAccessManagerImpl::BindingContext context_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const storage::FileSystemURL source_url_;
  const storage::FileSystemURL dest_url_;

  const storage::FileSystemOperation::CopyOrMoveOptionSet options_;

  download::QuarantineConnectionCallback quarantine_connection_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  bool has_transient_user_activation_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  FileSystemAccessSafeMoveHelperCallback callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessSafeMoveHelper> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_SAFE_MOVE_HELPER_H_
