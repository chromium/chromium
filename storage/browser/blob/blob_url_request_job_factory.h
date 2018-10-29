// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_FACTORY_H_
#define STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_FACTORY_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory.h"
#include "storage/browser/storage_browser_export.h"

namespace net {
class URLRequestContext;
}  // namespace net

namespace storage {

class BlobDataHandle;
class BlobStorageContext;

class STORAGE_EXPORT BlobProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // A helper to manufacture an URLRequest to retrieve the given blob.
  static std::unique_ptr<net::URLRequest> CreateBlobRequest(
      std::unique_ptr<BlobDataHandle> blob_data_handle,
      const net::URLRequestContext* request_context,
      net::URLRequest::Delegate* request_delegate);

  // This class ignores the request's URL and uses the value given
  // to SetRequestedBlobDataHandle instead.
  static void SetRequestedBlobDataHandle(
      net::URLRequest* request,
      std::unique_ptr<BlobDataHandle> blob_data_handle);

  // This gets the handle on the request if it exists.
  static BlobDataHandle* GetRequestBlobDataHandle(net::URLRequest* request);

  explicit BlobProtocolHandler(BlobStorageContext* context);
  ~BlobProtocolHandler() override;

  // net::URLRequestJobFactory::ProtocolHandler implementation:
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;
  bool IsSafeRedirectTarget(const GURL& location) const override;

 private:
  BlobDataHandle* LookupBlobHandle(net::URLRequest* request) const;

  base::WeakPtr<BlobStorageContext> context_;

  DISALLOW_COPY_AND_ASSIGN(BlobProtocolHandler);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_URL_REQUEST_JOB_FACTORY_H_
