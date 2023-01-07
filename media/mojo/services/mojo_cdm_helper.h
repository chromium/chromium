// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_HELPER_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_HELPER_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "media/cdm/cdm_auxiliary_helper.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/cdm_document_service.mojom.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/output_protection.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_cdm_file_io.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// Helper class that connects the CDM to various auxiliary services. All
// additional services (FileIO, memory allocation, output protection, and
// platform verification) are lazily created.
class MEDIA_MOJO_EXPORT MojoCdmHelper final : public CdmAuxiliaryHelper,
                                              public MojoCdmFileIO::Delegate {
 public:
  explicit MojoCdmHelper(mojom::FrameInterfaceFactory* frame_interfaces);
  MojoCdmHelper(const MojoCdmHelper&) = delete;
  MojoCdmHelper operator=(const MojoCdmHelper&) = delete;
  ~MojoCdmHelper() final;

  // CdmAuxiliaryHelper implementation.
  void SetFileReadCB(FileReadCB file_read_cb) final;
  cdm::FileIO* CreateCdmFileIO(cdm::FileIOClient* client) final;
  url::Origin GetCdmOrigin() final;
  cdm::Buffer* CreateCdmBuffer(size_t capacity) final;
  std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() final;
  void QueryStatus(QueryStatusCB callback) final;
  void EnableProtection(uint32_t desired_protection_mask,
                        EnableProtectionCB callback) final;
  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         ChallengePlatformCB callback) final;
  void GetStorageId(uint32_t version, StorageIdCB callback) final;
#if BUILDFLAG(IS_WIN)
  void GetMediaFoundationCdmData(GetMediaFoundationCdmDataCB callback) final;
  void SetCdmClientToken(const std::vector<uint8_t>& client_token) final;
  void OnCdmEvent(CdmEvent event, HRESULT hresult) final;
#endif  // BUILDFLAG(IS_WIN)

  // MojoCdmFileIO::Delegate implementation.
  void CloseCdmFileIO(MojoCdmFileIO* cdm_file_io) final;
  void ReportFileReadSize(int file_size_bytes) final;

 private:
  // All services are created lazily.
  void ConnectToOutputProtection();
  void ConnectToCdmDocumentService();

  CdmAllocator* GetAllocator();

  // Provides interfaces when needed.
  raw_ptr<mojom::FrameInterfaceFactory> frame_interfaces_;

  // Connections to the additional services. Will try to reconnect if
  // disconnected, to handle cases like page refresh, where the document is
  // destroyed but RenderFrameHostImpl is not.
  mojo::Remote<mojom::OutputProtection> output_protection_;
  mojo::Remote<mojom::CdmDocumentService> cdm_document_service_;

  std::unique_ptr<CdmAllocator> allocator_;

  FileReadCB file_read_cb_;

  // A list of open cdm::FileIO objects.
  // TODO(xhwang): Switch to use UniquePtrComparator.
  std::vector<std::unique_ptr<MojoCdmFileIO>> cdm_file_io_set_;

  base::WeakPtrFactory<MojoCdmHelper> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_HELPER_H_
