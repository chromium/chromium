// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_AUXILIARY_HELPER_H_
#define MEDIA_CDM_CDM_AUXILIARY_HELPER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/cdm/cdm_allocator.h"
#include "media/cdm/cdm_document_service.h"
#include "media/cdm/output_protection.h"
#include "media/media_buildflags.h"
#include "url/origin.h"

namespace cdm {
class FileIO;
class FileIOClient;
}  // namespace cdm

namespace media {

// Provides a wrapper on the auxiliary functions (CdmAllocator, CdmFileIO,
// OutputProtection, CdmDocumentService) needed by the library CDM. The
// default implementation does nothing -- it simply returns nullptr, false, 0,
// etc. as required to meet the interface.
class MEDIA_EXPORT CdmAuxiliaryHelper : public CdmAllocator,
                                        public OutputProtection,
                                        public CdmDocumentService {
 public:
  CdmAuxiliaryHelper();
  CdmAuxiliaryHelper(const CdmAuxiliaryHelper&) = delete;
  CdmAuxiliaryHelper& operator=(const CdmAuxiliaryHelper&) = delete;
  ~CdmAuxiliaryHelper() override;

  // Callback to report the size of file read by cdm::FileIO created by |this|.
  using FileReadCB = base::RepeatingCallback<void(int)>;
  virtual void SetFileReadCB(FileReadCB file_read_cb);

  // Given |client|, creates a cdm::FileIO object and returns it.
  // The caller does not own the returned object and should not delete it
  // directly. Instead, it should call cdm::FileIO::Close() after it's not
  // needed anymore.
  virtual cdm::FileIO* CreateCdmFileIO(cdm::FileIOClient* client);

  // Gets the origin of the frame associated with the CDM, which could be empty
  // if the origin is unavailable or if error happened.
  virtual url::Origin GetCdmOrigin();

  // CdmAllocator implementation.
  cdm::Buffer* CreateCdmBuffer(size_t capacity) override;
  std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() override;

  // OutputProtection implementation.
  void QueryStatus(QueryStatusCB callback) override;
  void EnableProtection(uint32_t desired_protection_mask,
                        EnableProtectionCB callback) override;

  // CdmDocumentService implementation.
  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         ChallengePlatformCB callback) override;
  void GetStorageId(uint32_t version, StorageIdCB callback) override;

#if BUILDFLAG(IS_WIN)
  void GetMediaFoundationCdmData(GetMediaFoundationCdmDataCB callback) override;
  void SetCdmClientToken(const std::vector<uint8_t>& client_token) override;
  void OnCdmEvent(CdmEvent event, HRESULT hresult) override;
#endif  // BUILDFLAG(IS_WIN)
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_AUXILIARY_HELPER_H_
