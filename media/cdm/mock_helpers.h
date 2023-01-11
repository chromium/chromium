// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_MOCK_HELPERS_H_
#define MEDIA_CDM_MOCK_HELPERS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "media/cdm/cdm_allocator.h"
#include "media/cdm/cdm_auxiliary_helper.h"
#include "media/cdm/cdm_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockCdmAuxiliaryHelper : public CdmAuxiliaryHelper {
 public:
  // `allocator` is optional; can be null if no need to create buffers/frames.
  explicit MockCdmAuxiliaryHelper(std::unique_ptr<CdmAllocator> allocator);

  MockCdmAuxiliaryHelper(const MockCdmAuxiliaryHelper&) = delete;
  MockCdmAuxiliaryHelper& operator=(const MockCdmAuxiliaryHelper&) = delete;

  ~MockCdmAuxiliaryHelper() override;

  // CdmAuxiliaryHelper implementation.
  void SetFileReadCB(FileReadCB file_read_cb) override;
  MOCK_METHOD1(CreateCdmFileIO, cdm::FileIO*(cdm::FileIOClient* client));

  cdm::Buffer* CreateCdmBuffer(size_t capacity) override;
  std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() override;

  // Trampoline methods to workaround GMOCK problems with std::unique_ptr<>
  // parameters.
  MOCK_METHOD0(QueryStatusCalled, bool());
  void QueryStatus(QueryStatusCB callback) override;

  MOCK_METHOD1(EnableProtectionCalled, bool(uint32_t desired_protection_mask));
  void EnableProtection(uint32_t desired_protection_mask,
                        EnableProtectionCB callback) override;

  MOCK_METHOD2(ChallengePlatformCalled,
               bool(const std::string& service_id,
                    const std::string& challenge));
  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         ChallengePlatformCB callback) override;

  MOCK_METHOD1(GetStorageIdCalled, std::vector<uint8_t>(uint32_t version));
  void GetStorageId(uint32_t version, StorageIdCB callback) override;

#if BUILDFLAG(IS_WIN)
  MOCK_METHOD(void,
              GetMediaFoundationCdmData,
              (GetMediaFoundationCdmDataCB callback),
              (override));
#endif  // BUILDFLAG(IS_WIN)

 private:
  std::unique_ptr<CdmAllocator> allocator_;
};

}  // namespace media

#endif  // MEDIA_CDM_MOCK_HELPERS_H_
