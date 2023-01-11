// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PROVISION_FETCHER_H_
#define MEDIA_BASE_PROVISION_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace media {

// The interface to retrieve provision information for CDM.
class ProvisionFetcher {
 public:
  // After provision information is retrieved this callback will be called
  // with the status flag (success/failure) and the provision response in
  // case of success.
  using ResponseCB =
      base::OnceCallback<void(bool success, const std::string& response)>;

  virtual ~ProvisionFetcher() {}

  // Requests the provision information with |default_url| and |request_data|
  // and calls |response_cb| callback with the response. On Android, the input
  // parameters |default_url| and |request_data| corresponds to Java class
  // MediaDrm.ProvisionRequest.
  // The implementation must call |response_cb| asynchronously on the same
  // thread that this method is called.
  virtual void Retrieve(const GURL& default_url,
                        const std::string& request_data,
                        ResponseCB response_cb) = 0;
};

using CreateFetcherCB =
    base::RepeatingCallback<std::unique_ptr<ProvisionFetcher>()>;

}  // namespace media

#endif  // MEDIA_BASE_PROVISION_FETCHER_H_
