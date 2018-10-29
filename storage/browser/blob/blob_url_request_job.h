// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_H_
#define STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_job.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/storage_browser_export.h"

namespace net {
class HttpResponseHeaders;
class IOBuffer;
}

namespace storage {

class BlobDataHandle;

// A request job that handles reading blob URLs.
class STORAGE_EXPORT BlobURLRequestJob
    : public net::URLRequestJob {
 public:
  BlobURLRequestJob(net::URLRequest* request,
                    net::NetworkDelegate* network_delegate,
                    BlobDataHandle* blob_handle);

  // net::URLRequestJob methods.
  void Start() override;
  void Kill() override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  bool GetMimeType(std::string* mime_type) const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  void SetExtraRequestHeaders(const net::HttpRequestHeaders& headers) override;

  // Helper method to create the HTTP headers for the response.
  // |blob_handles|, |total_size|, |byte_range| and |content_size| are only
  // used if status_code isn't an error.
  static scoped_refptr<net::HttpResponseHeaders> GenerateHeaders(
      net::HttpStatusCode status_code,
      BlobDataHandle* blob_handle,
      net::HttpByteRange* byte_range,
      uint64_t total_size,
      uint64_t content_size);

 protected:
  ~BlobURLRequestJob() override;

 private:
  // For preparing for read: get the size, apply the range and perform seek.
  void DidStart();
  void DidCalculateSize(int result);
  void DidReadMetadata(BlobReader::Status result);
  void DidReadRawData(int result);

  void NotifyFailure(int);
  void HeadersCompleted(net::HttpStatusCode status_code);

  // Is set when NotifyFailure() is called and reset when DidStart is called.
  bool error_;

  bool byte_range_set_;
  net::HttpByteRange byte_range_;

  std::unique_ptr<BlobDataHandle> blob_handle_;
  std::unique_ptr<BlobReader> blob_reader_;
  std::unique_ptr<net::HttpResponseInfo> response_info_;

  base::WeakPtrFactory<BlobURLRequestJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlobURLRequestJob);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_H_
