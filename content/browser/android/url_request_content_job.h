// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_URL_REQUEST_CONTENT_JOB_H_
#define CONTENT_BROWSER_ANDROID_URL_REQUEST_CONTENT_JOB_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace base {
class TaskRunner;
}

namespace net {
class FileStream;
}

namespace content {

// A request job that handles reading content URIs
//
// Note that when the Network Service is enabled, ContentUrlLoaderFactory is
// used instead.
class CONTENT_EXPORT URLRequestContentJob : public net::URLRequestJob {
 public:
  URLRequestContentJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const base::FilePath& content_path,
      const scoped_refptr<base::TaskRunner>& content_task_runner);

  // net::URLRequestJob:
  void Start() override;
  void Kill() override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  bool IsRedirectResponse(GURL* location,
                          int* http_status_code,
                          bool* insecure_scheme_was_upgraded) override;
  bool GetMimeType(std::string* mime_type) const override;
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;

 protected:
  ~URLRequestContentJob() override;

 private:
  // Meta information about the content URI. It's used as a member in the
  // URLRequestContentJob and also passed between threads because disk access is
  // necessary to obtain it.
  struct ContentMetaInfo {
    ContentMetaInfo();
    // Flag showing whether the content URI exists.
    bool content_exists;
    // Size of the content URI.
    int64_t content_size;
    // Mime type associated with the content URI.
    std::string mime_type;
  };

  // Fetches content URI info on a background thread.
  static void FetchMetaInfo(const base::FilePath& content_path,
                            ContentMetaInfo* meta_info);

  // Callback after fetching content URI info on a background thread.
  void DidFetchMetaInfo(const ContentMetaInfo* meta_info);

  // Callback after opening content URI on a background thread.
  void DidOpen(int result);

  // Callback after seeking to the beginning of |byte_range_| in the content URI
  // on a background thread.
  void DidSeek(int64_t result);

  // Callback after data is asynchronously read from the content URI into |buf|.
  void DidRead(int result);

  // The full path of the content URI.
  base::FilePath content_path_;

  std::unique_ptr<net::FileStream> stream_;
  ContentMetaInfo meta_info_;
  std::string mime_type_from_intent_;
  const scoped_refptr<base::TaskRunner> content_task_runner_;

  net::HttpByteRange byte_range_;
  net::Error range_parse_result_;
  int64_t remaining_bytes_;

  bool io_pending_;

  base::WeakPtrFactory<URLRequestContentJob> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContentJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_URL_REQUEST_CONTENT_JOB_H_
