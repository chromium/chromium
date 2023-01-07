// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_RESULT_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_RESULT_H_

namespace web {

class DownloadResult {
 public:
  DownloadResult();

  // Constructs a new DownloadResult object. `error_code` value of net::OK
  // indicates success while other values indicate a failure. `can_retry`
  // indicates if a download can be retried or not and depends on if a
  // WKDownload can return resumed data.
  explicit DownloadResult(int error_code, bool can_retry = true);

  ~DownloadResult();

  // Returns whether the download can be retried. Only meaningful
  // if `is_successful()` is false.
  bool can_retry() const;

  // Returns whether the download was a success.
  bool is_successful() const;

  // Returns error code in object
  int error_code() const;

 private:
  int error_code_ = 0;
  bool can_retry_ = true;
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_RESULT_H_
