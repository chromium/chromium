// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_H_
#define IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_H_

#import <Foundation/Foundation.h>

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace web {

class DownloadTaskObserver;
class WebState;

// Provides API for a single browser download task. This is the model class that
// stores all the state for a download. Sequence-affine.
class DownloadTask {
 public:
  // Possible state of the Download Task.
  enum class State {
    // Download has not started yet.
    kNotStarted = 0,

    // Download is actively progressing.
    kInProgress,

    // Download is cancelled.
    kCancelled,

    // Download is completely finished.
    kComplete,

    // Download has failed but can be resumed
    kFailed,

    // Downkoad has failed but cannot be resumed
    kFailedNotResumable,
  };

  // Type of the callback invoked when the downloaded data has been read
  // from disk.
  using ResponseDataReadCallback = base::OnceCallback<void(NSData* data)>;

  // Returns WebState which requested this download.
  virtual WebState* GetWebState() = 0;

  // Returns the download task state.
  virtual State GetState() const = 0;

  // Starts the download and save it to `path`. If `path` is null, the data
  // downloaded will be saved to a temporary location and deleted when the
  // task is destroyed. Otherwise, upon success, the ownership of the file
  // will be transferred to the caller. The task will take care of creating
  // the directory structure required to save the file (or will fail with an
  // error if this is not possible).
  virtual void Start(const base::FilePath& path) = 0;

  // Cancels the download.
  virtual void Cancel() = 0;

  // Reads the downloaded data from the saved path, and call `callback` on
  // the calling sequence with it. If the download was done to a temporary
  // location, the read will fail if the `DownloadTask` is deleted before
  // `callback` is called. In that case, you may want to have the `callback`
  // take ownership of the task.
  virtual void GetResponseData(ResponseDataReadCallback callback) const = 0;

  // Returns the path to the downloaded data, if saved to disk.
  virtual const base::FilePath& GetResponsePath() const = 0;

  // Unique indentifier for this task. Also can be used to resume unfinished
  // downloads after the application relaunch (see example in DownloadController
  // class comments).
  virtual NSString* GetIdentifier() const = 0;

  // The URL that the download request originally attempted to fetch. This may
  // differ from the final download URL if there were redirects.
  virtual const GURL& GetOriginalUrl() const = 0;

  // HTTP method for this download task (only @"GET" and @"POST" are currently
  // supported).
  virtual NSString* GetHttpMethod() const = 0;

  // Returns true if the download is in a terminal state. This includes
  // completed downloads, cancelled downloads, and interrupted downloads that
  // can't be resumed.
  virtual bool IsDone() const = 0;

  // Error code for this download task. 0 if the download is still in progress
  // or the download has sucessfully completed. See net_errors.h for the
  // possible error codes.
  virtual int GetErrorCode() const = 0;

  // HTTP response code for this download task. -1 the response has not been
  // received yet or the response not an HTTP response.
  virtual int GetHttpCode() const = 0;

  // Total number of expected bytes (a best-guess upper-bound). Returns -1 if
  // the total size is unknown.
  virtual int64_t GetTotalBytes() const = 0;

  // Total number of bytes that have been received.
  virtual int64_t GetReceivedBytes() const = 0;

  // Rough percent complete. Returns -1 if progress is unknown. 100 if the
  // download is already complete.
  virtual int GetPercentComplete() const = 0;

  // Content-Disposition header value from HTTP response.
  virtual std::string GetContentDisposition() const = 0;

  // MIME type that the download request originally attempted to fetch.
  virtual std::string GetOriginalMimeType() const = 0;

  // Effective MIME type of downloaded content.
  virtual std::string GetMimeType() const = 0;

  // Suggested name for the downloaded file.
  virtual base::FilePath GenerateFileName() const = 0;

  // Returns true if the last download operation was fully or partially
  // performed while the application was not active.
  virtual bool HasPerformedBackgroundDownload() const = 0;

  // Adds and Removes DownloadTaskObserver. Clients must remove self from
  // observers before the task is destroyed.
  virtual void AddObserver(DownloadTaskObserver* observer) = 0;
  virtual void RemoveObserver(DownloadTaskObserver* observer) = 0;

  DownloadTask() = default;

  DownloadTask(const DownloadTask&) = delete;
  DownloadTask& operator=(const DownloadTask&) = delete;

  virtual ~DownloadTask() = default;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_TASK_H_
