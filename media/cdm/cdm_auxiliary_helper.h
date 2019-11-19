// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_AUXILIARY_HELPER_H_
#define MEDIA_CDM_CDM_AUXILIARY_HELPER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/cdm/cdm_allocator.h"
#include "media/cdm/output_protection.h"
#include "media/cdm/platform_verification.h"
#include "media/media_buildflags.h"

namespace cdm {
class FileIO;
class FileIOClient;
class CdmProxy;
class CdmProxyClient;
}  // namespace cdm

namespace media {

// Provides a wrapper on the auxiliary functions (CdmAllocator, CdmFileIO,
// OutputProtection, PlatformVerification) needed by the library CDM. The
// default implementation does nothing -- it simply returns nullptr, false, 0,
// etc. as required to meet the interface.
class MEDIA_EXPORT CdmAuxiliaryHelper : public CdmAllocator,
                                        public OutputProtection,
                                        public PlatformVerification {
 public:
  CdmAuxiliaryHelper();
  ~CdmAuxiliaryHelper() override;

  // Callback to report the size of file read by cdm::FileIO created by |this|.
  using FileReadCB = base::RepeatingCallback<void(int)>;
  virtual void SetFileReadCB(FileReadCB file_read_cb);

  // Given |client|, creates a cdm::FileIO object and returns it.
  // The caller does not own the returned object and should not delete it
  // directly. Instead, it should call cdm::FileIO::Close() after it's not
  // needed anymore.
  virtual cdm::FileIO* CreateCdmFileIO(cdm::FileIOClient* client);

#if BUILDFLAG(ENABLE_CDM_PROXY)
  // Creates a cdm::CdmProxy object and returns it.
  // The caller does not own the returned object and should not delete it
  // directly. Instead, it should call cdm::CdmProxy::Destroy() after it's not
  // needed anymore.
  virtual cdm::CdmProxy* CreateCdmProxy(cdm::CdmProxyClient* client);

  // Returns a CDM ID associated with the last returned CdmProxy. Should only
  // be called after the CdmProxy has been initialized.
  virtual int GetCdmProxyCdmId();
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

  // CdmAllocator implementation.
  cdm::Buffer* CreateCdmBuffer(size_t capacity) override;
  std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() override;

  // OutputProtection implementation.
  void QueryStatus(QueryStatusCB callback) override;
  void EnableProtection(uint32_t desired_protection_mask,
                        EnableProtectionCB callback) override;

  // PlatformVerification implementation.
  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         ChallengePlatformCB callback) override;
  void GetStorageId(uint32_t version, StorageIdCB callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmAuxiliaryHelper);
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_AUXILIARY_HELPER_H_
