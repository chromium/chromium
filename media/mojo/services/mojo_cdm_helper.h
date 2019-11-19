// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_HELPER_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_HELPER_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/cdm/cdm_auxiliary_helper.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "media/mojo/mojom/output_protection.mojom.h"
#include "media/mojo/mojom/platform_verification.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_cdm_file_io.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(ENABLE_CDM_PROXY)
#include "media/mojo/mojom/cdm_proxy.mojom.h"
#include "media/mojo/services/mojo_cdm_proxy.h"
#endif

namespace service_manager {
namespace mojom {
class InterfaceProvider;
}
}  // namespace service_manager

namespace media {

// Helper class that connects the CDM to various auxiliary services. All
// additional services (FileIO, memory allocation, output protection, and
// platform verification) are lazily created.
class MEDIA_MOJO_EXPORT MojoCdmHelper final : public CdmAuxiliaryHelper,
                                              public MojoCdmFileIO::Delegate {
 public:
  explicit MojoCdmHelper(
      service_manager::mojom::InterfaceProvider* interface_provider);
  ~MojoCdmHelper() final;

  // CdmAuxiliaryHelper implementation.
  void SetFileReadCB(FileReadCB file_read_cb) final;
  cdm::FileIO* CreateCdmFileIO(cdm::FileIOClient* client) final;
#if BUILDFLAG(ENABLE_CDM_PROXY)
  cdm::CdmProxy* CreateCdmProxy(cdm::CdmProxyClient* client) final;
  int GetCdmProxyCdmId() final;
#endif
  cdm::Buffer* CreateCdmBuffer(size_t capacity) final;
  std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() final;
  void QueryStatus(QueryStatusCB callback) final;
  void EnableProtection(uint32_t desired_protection_mask,
                        EnableProtectionCB callback) final;
  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         ChallengePlatformCB callback) final;
  void GetStorageId(uint32_t version, StorageIdCB callback) final;

  // MojoCdmFileIO::Delegate implementation.
  void CloseCdmFileIO(MojoCdmFileIO* cdm_file_io) final;
  void ReportFileReadSize(int file_size_bytes) final;

 private:
  // All services are created lazily.
  void ConnectToCdmStorage();
  CdmAllocator* GetAllocator();
  void ConnectToOutputProtection();
  void ConnectToPlatformVerification();

  // Provides interfaces when needed.
  service_manager::mojom::InterfaceProvider* interface_provider_;

  // Connections to the additional services. For the mojom classes, if a
  // connection error occurs, we will not be able to reconnect to the
  // service as the document has been destroyed (see FrameServiceBase) or
  // the browser crashed, so there's no point in trying to reconnect.
  mojo::Remote<mojom::CdmStorage> cdm_storage_remote_;
  std::unique_ptr<CdmAllocator> allocator_;
  mojo::Remote<mojom::OutputProtection> output_protection_;
  mojo::Remote<mojom::PlatformVerification> platform_verification_;

  FileReadCB file_read_cb_;

  // A list of open cdm::FileIO objects.
  // TODO(xhwang): Switch to use UniquePtrComparator.
  std::vector<std::unique_ptr<MojoCdmFileIO>> cdm_file_io_set_;

#if BUILDFLAG(ENABLE_CDM_PROXY)
  std::unique_ptr<MojoCdmProxy> cdm_proxy_;
#endif

  base::WeakPtrFactory<MojoCdmHelper> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MojoCdmHelper);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_HELPER_H_
