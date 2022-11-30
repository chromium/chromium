// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/mock_helpers.h"

namespace media {

MockCdmAuxiliaryHelper::MockCdmAuxiliaryHelper(
    std::unique_ptr<CdmAllocator> allocator)
    : allocator_(std::move(allocator)) {}

MockCdmAuxiliaryHelper::~MockCdmAuxiliaryHelper() = default;

void MockCdmAuxiliaryHelper::SetFileReadCB(FileReadCB file_read_cb) {}

cdm::Buffer* MockCdmAuxiliaryHelper::CreateCdmBuffer(size_t capacity) {
  if (!allocator_)
    return nullptr;

  return allocator_->CreateCdmBuffer(capacity);
}

std::unique_ptr<VideoFrameImpl> MockCdmAuxiliaryHelper::CreateCdmVideoFrame() {
  if (!allocator_)
    return nullptr;

  return allocator_->CreateCdmVideoFrame();
}

void MockCdmAuxiliaryHelper::QueryStatus(QueryStatusCB callback) {
  std::move(callback).Run(QueryStatusCalled(), 0, 0);
}

void MockCdmAuxiliaryHelper::EnableProtection(uint32_t desired_protection_mask,
                                              EnableProtectionCB callback) {
  std::move(callback).Run(EnableProtectionCalled(desired_protection_mask));
}

void MockCdmAuxiliaryHelper::ChallengePlatform(const std::string& service_id,
                                               const std::string& challenge,
                                               ChallengePlatformCB callback) {
  std::move(callback).Run(ChallengePlatformCalled(service_id, challenge), "",
                          "", "");
}

void MockCdmAuxiliaryHelper::GetStorageId(uint32_t version,
                                          StorageIdCB callback) {
  std::move(callback).Run(version, GetStorageIdCalled(version));
}

}  // namespace media
