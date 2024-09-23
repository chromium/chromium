// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_FILE_DOWNLOADER_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_FILE_DOWNLOADER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"

struct DriveItem;
@protocol SystemIdentity;

// Identifier for a download started from `DriveFileDownloader`.
using DriveFileDownloadID = NSString*;

// Progress reported by the progress block of a file download.
struct DriveFileDownloadProgress {
  // Number of bytes downloaded since last call to the progress callback.
  uint64_t bytes_written;
  // Number of bytes downloaded so far.
  uint64_t total_bytes_written;
  // Number of bytes expected to be downloaded.
  uint64_t total_bytes_expected_to_write;
};

using DriveFileDownloadProgressCallback =
    base::RepeatingCallback<void(DriveFileDownloadID,
                                 const DriveFileDownloadProgress&)>;
using DriveFileDownloadCompletionCallback =
    base::OnceCallback<void(DriveFileDownloadID, BOOL, NSError*)>;

// This interface is used to download items from a user's Drive account.
class DriveFileDownloader {
 public:
  DriveFileDownloader();
  virtual ~DriveFileDownloader();

  // Returns the identity used to perform queries.
  virtual id<SystemIdentity> GetIdentity() const = 0;
  // Returns whether download with ID `download_id` is currently being executed
  // by this downloader.
  virtual bool IsExecutingDownload(DriveFileDownloadID download_id) const = 0;
  // Cancels a download with ID `download_id` currently being executed by this
  // downloader.
  virtual void CancelDownload(DriveFileDownloadID download_id) = 0;

  // Initiates a download of the given `item_to_download` and stores it locally
  // at `file_url`. Progress of download is reported through `progress_callback`
  // and final status, including possible error details, is reported through
  // `completion_callback`. Returns ID of the download.
  virtual DriveFileDownloadID DownloadFile(
      const DriveItem& item_to_download,
      NSURL* file_url,
      DriveFileDownloadProgressCallback progress_callback,
      DriveFileDownloadCompletionCallback completion_callback) = 0;
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_FILE_DOWNLOADER_H_
