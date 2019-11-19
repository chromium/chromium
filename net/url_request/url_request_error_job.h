// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Invalid URLs go through this URLRequestJob class rather than being
// passed to the default job handler.

#ifndef NET_URL_REQUEST_URL_REQUEST_ERROR_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_ERROR_JOB_H_

#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/url_request/url_request_job.h"

namespace net {

class NET_EXPORT URLRequestErrorJob : public URLRequestJob {
 public:
  URLRequestErrorJob(URLRequest* request,
                     NetworkDelegate* network_delegate,
                     int error);

  void Start() override;
  void Kill() override;

 private:
  ~URLRequestErrorJob() override;

  void StartAsync();

  int error_;

  base::WeakPtrFactory<URLRequestErrorJob> weak_factory_{this};
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_ERROR_JOB_H_
