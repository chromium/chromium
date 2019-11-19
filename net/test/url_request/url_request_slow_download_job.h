// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This class simulates a slow download. Requests to |kUnknownSizeUrl| and
// |kKnownSizeUrl| start downloads that pause after the first N bytes, to be
// completed by sending a request to |kFinishDownloadUrl|.

#ifndef NET_TEST_URL_REQUEST_URL_REQUEST_SLOW_DOWNLOAD_JOB_H_
#define NET_TEST_URL_REQUEST_URL_REQUEST_SLOW_DOWNLOAD_JOB_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>

#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_job.h"

namespace net {

class URLRequestSlowDownloadJob : public URLRequestJob {
 public:
  // Test URLs.
  static const char kUnknownSizeUrl[];
  static const char kKnownSizeUrl[];
  static const char kFinishDownloadUrl[];
  static const char kErrorDownloadUrl[];

  // Download sizes.
  static const int kFirstDownloadSize;
  static const int kSecondDownloadSize;

  // Timer callback, used to check to see if we should finish our download and
  // send the second chunk.
  void CheckDoneStatus();

  // URLRequestJob methods
  void Start() override;
  int64_t GetTotalReceivedBytes() const override;
  bool GetMimeType(std::string* mime_type) const override;
  void GetResponseInfo(HttpResponseInfo* info) override;
  int ReadRawData(IOBuffer* buf, int buf_size) override;

  // Returns the current number of URLRequestSlowDownloadJobs that have
  // not yet completed.
  static size_t NumberOutstandingRequests();

  // Adds the testing URLs to the URLRequestFilter.
  static void AddUrlHandler();

 private:
  class Interceptor;

  // Enum indicating where we are in the read after a call to
  // FillBufferHelper.
  enum ReadStatus {
    // The buffer was filled with data and may be returned.
    BUFFER_FILLED,

    // No data was added to the buffer because kFinishDownloadUrl has
    // not yet been seen and we've already returned the first chunk.
    REQUEST_BLOCKED,

    // No data was added to the buffer because we've already returned
    // all the data.
    REQUEST_COMPLETE
  };

  URLRequestSlowDownloadJob(URLRequest* request,
                            NetworkDelegate* network_delegate);
  ~URLRequestSlowDownloadJob() override;

  ReadStatus FillBufferHelper(IOBuffer* buf, int buf_size, int* bytes_written);

  void GetResponseInfoConst(HttpResponseInfo* info) const;

  // Mark all pending requests to be finished.  We keep track of pending
  // requests in |pending_requests_|.
  static void FinishPendingRequests();
  static void ErrorPendingRequests();
  typedef std::set<URLRequestSlowDownloadJob*> SlowJobsSet;
  static base::LazyInstance<SlowJobsSet>::Leaky pending_requests_;

  void StartAsync();

  void set_should_finish_download() { should_finish_download_ = true; }
  void set_should_error_download() { should_error_download_ = true; }

  int bytes_already_sent_;
  bool should_error_download_;
  bool should_finish_download_;
  scoped_refptr<IOBuffer> buffer_;
  int buffer_size_;

  base::WeakPtrFactory<URLRequestSlowDownloadJob> weak_factory_{this};
};

}  // namespace net

#endif  // NET_TEST_URL_REQUEST_URL_REQUEST_SLOW_DOWNLOAD_JOB_H_
