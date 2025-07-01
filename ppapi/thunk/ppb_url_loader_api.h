// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_URL_LOADER_API_H_
#define PPAPI_THUNK_PPB_URL_LOADER_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"

namespace ppapi {

class TrackedCallback;
struct URLRequestInfoData;

namespace thunk {

class PPB_URLLoader_API {
 public:
  virtual ~PPB_URLLoader_API() {}

  // Open given the resource ID of a PPB_URLRequestInfo resource.
  virtual int32_t Open(PP_Resource request_id,
                       scoped_refptr<TrackedCallback> callback) = 0;

  // Internal open given a URLRequestInfoData and requestor_pid, which
  // indicates the process that requested and will consume the data.
  // Pass 0 for requestor_pid to indicate the current process.
  virtual int32_t Open(const URLRequestInfoData& data,
                       int requestor_pid,
                       scoped_refptr<TrackedCallback> callback) = 0;

  virtual int32_t FollowRedirect(scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Bool GetUploadProgress(int64_t* bytes_sent,
                                    int64_t* total_bytes_to_be_sent) = 0;
  virtual PP_Bool GetDownloadProgress(int64_t* bytes_received,
                                      int64_t* total_bytes_to_be_received) = 0;
  virtual PP_Resource GetResponseInfo() = 0;
  virtual int32_t ReadResponseBody(void* buffer,
                                   int32_t bytes_to_read,
                                   scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t FinishStreamingToFile(
      scoped_refptr<TrackedCallback> callback) = 0;
  virtual void Close() = 0;

  // Trusted API.
  virtual void GrantUniversalAccess() = 0;
  virtual void RegisterStatusCallback(
      PP_URLLoaderTrusted_StatusCallback cb) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_URL_LOADER_API_H_
