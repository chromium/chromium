// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CLOUD_BINARY_UPLOAD_SERVICE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CLOUD_BINARY_UPLOAD_SERVICE_H_

#import "base/memory/weak_ptr.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_cancel_requests.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/cloud_binary_upload_service_base.h"
#import "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"

class ProfileIOS;

namespace enterprise_connectors {

// This class encapsulates the process of uploading a file for deep scanning,
// and asynchronously retrieving a verdict.
class IOSCloudBinaryUploadService
    : public CloudBinaryUploadServiceBase::Delegate {
 public:
  explicit IOSCloudBinaryUploadService(ProfileIOS* profile);
  ~IOSCloudBinaryUploadService() override;

 private:
  // CloudBinaryUploadServiceBase::Delegate overrides:
  void MaybeGetAccessToken(BinaryUploadRequest* request,
                           base::OnceCallback<void(const std::string&)>
                               access_token_callback) override;
  BinaryUploadRequest::BrowserPolicyConnectorGetter
  BrowserPolicyConnectorGetter() override;
  bool IsAdvancedProtection() override;
  bool IsEnhancedProtection() override;

  const raw_ptr<ProfileIOS> profile_;

  // Used to obtain an access token to attach to requests.
  std::unique_ptr<safe_browsing::SafeBrowsingTokenFetcher> token_fetcher_;

  base::WeakPtrFactory<IOSCloudBinaryUploadService> weakptr_factory_;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CLOUD_BINARY_UPLOAD_SERVICE_H_
