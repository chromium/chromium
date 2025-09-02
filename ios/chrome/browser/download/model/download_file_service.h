// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILE_SERVICE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILE_SERVICE_H_

#import <string>

#import "base/files/file_path.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "components/keyed_service/core/keyed_service.h"

class DownloadRecordService;

// Service that manages download file operations independently of UI components.
// Ensures reliable file operations even if UI elements are destroyed during
// async operations.
class DownloadFileService : public KeyedService {
 public:
  using MoveCompleteCallback =
      base::OnceCallback<void(bool success,
                              const std::string& download_id,
                              const base::FilePath& source_path,
                              const base::FilePath& final_path)>;

  // Constructor accepts DownloadRecordService pointer (may be nullptr).
  explicit DownloadFileService(DownloadRecordService* download_record_service);

  DownloadFileService(const DownloadFileService&) = delete;
  DownloadFileService& operator=(const DownloadFileService&) = delete;

  ~DownloadFileService() override;

  // Asynchronously moves a download file from source to destination.
  // Must be called on the main thread. Callback runs on the main thread.
  void MoveDownloadFile(const std::string& download_id,
                        const base::FilePath& source_path,
                        const base::FilePath& destination_path,
                        MoveCompleteCallback callback);

  // Asynchronously resolves an available file path in the target directory.
  // Handles file name conflicts by generating unique names if needed.
  // Must be called on the main thread. Callback runs on the main thread.
  void ResolveAvailableFilePath(
      const base::FilePath& target_directory,
      const base::FilePath& suggested_filename,
      base::OnceCallback<void(base::FilePath)> callback);

  // Asynchronously checks if a file exists.
  // Must be called on the main thread. Callback runs on the main thread.
  void CheckFileExists(const base::FilePath& file_path,
                       base::OnceCallback<void(bool)> callback);

 private:
  // Called on the main thread when the file move operation completes.
  void OnFileMoveComplete(const std::string& download_id,
                          const base::FilePath& source_path,
                          const base::FilePath& destination_path,
                          MoveCompleteCallback callback,
                          bool move_success);

  // Performs the actual file move on the background thread.
  static bool DoMoveFileOnBackgroundThread(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      const base::FilePath& source_path,
      const base::FilePath& destination_path);

  // Finds an available file path in the target directory on the background
  // thread. If the file already exists, a new file name will be generated.
  static base::FilePath FindAvailableDownloadFilePath(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      const base::FilePath& download_dir,
      const base::FilePath& file_name);

  // Task runner for file operations.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Download record service for persistent storage operations.
  raw_ptr<DownloadRecordService> download_record_service_ = nullptr;

  // Sequence checker for main thread operations.
  SEQUENCE_CHECKER(main_sequence_checker_);

  // Weak pointer factory for callbacks.
  base::WeakPtrFactory<DownloadFileService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_FILE_SERVICE_H_
