// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_cloud_binary_upload_service.h"

#import "base/notimplemented.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_connectors {

IOSCloudBinaryUploadService::IOSCloudBinaryUploadService(ProfileIOS* profile)
    : weakptr_factory_(this) {}

IOSCloudBinaryUploadService::~IOSCloudBinaryUploadService() = default;

void IOSCloudBinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<BinaryUploadRequest> request) {
  NOTIMPLEMENTED();
}

void IOSCloudBinaryUploadService::MaybeAcknowledge(
    std::unique_ptr<BinaryUploadAck> ack) {
  // Nothing to do for cloud upload service.
}

void IOSCloudBinaryUploadService::MaybeCancelRequests(
    std::unique_ptr<BinaryUploadCancelRequests> cancel) {
  // Nothing to do for cloud upload service.
}

base::WeakPtr<BinaryUploadService> IOSCloudBinaryUploadService::AsWeakPtr() {
  return weakptr_factory_.GetWeakPtr();
}

}  // namespace enterprise_connectors
