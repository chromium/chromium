// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CLOUD_BINARY_UPLOAD_SERVICE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CLOUD_BINARY_UPLOAD_SERVICE_H_

#import "base/memory/weak_ptr.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_cancel_requests.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"

class ProfileIOS;

namespace enterprise_connectors {

// This class encapsulates the process of uploading a file for deep scanning,
// and asynchronously retrieving a verdict.
//
// TODO(crbug.com/482050525): Implement this class.
class IOSCloudBinaryUploadService : public BinaryUploadService {
 public:
  explicit IOSCloudBinaryUploadService(ProfileIOS* profile);
  ~IOSCloudBinaryUploadService() override;

  // BinaryUploadService overrides:
  void MaybeUploadForDeepScanning(
      std::unique_ptr<BinaryUploadRequest> request) override;
  void MaybeAcknowledge(std::unique_ptr<BinaryUploadAck> ack) override;
  void MaybeCancelRequests(
      std::unique_ptr<BinaryUploadCancelRequests> cancel) override;
  base::WeakPtr<BinaryUploadService> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<IOSCloudBinaryUploadService> weakptr_factory_;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLOUD_CONTENT_SCANNING_MODEL_IOS_CLOUD_BINARY_UPLOAD_SERVICE_H_
