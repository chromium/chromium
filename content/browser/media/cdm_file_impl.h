// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_FILE_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_FILE_IMPL_H_

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
class MediaLicenseStorageHost;
class CdmStorageManager;

// This class implements the media::mojom::CdmFile interface.
class CdmFileImpl final : public media::mojom::CdmFile {
 public:
  // Check whether |name| is valid as a usable file name. Returns true if it is,
  // false otherwise.
  static bool IsValidName(const std::string& name);

  // This "file" is actually just an entry in a custom backend for CDM, uniquely
  // identified by a storage key, CDM type, and file name. File operations are
  // routed through `host` which is owned by the storage partition.
  CdmFileImpl(
      MediaLicenseStorageHost* host,
      const media::CdmType& cdm_type,
      const std::string& file_name,
      mojo::PendingAssociatedReceiver<media::mojom::CdmFile> pending_receiver);

  // As Above. This constructor is used by CdmStorageManager.
  CdmFileImpl(
      CdmStorageManager* manager,
      const blink::StorageKey& storage_key,
      const media::CdmType& cdm_type,
      const std::string& file_name,
      mojo::PendingAssociatedReceiver<media::mojom::CdmFile> pending_receiver);

  CdmFileImpl(const CdmFileImpl&) = delete;
  CdmFileImpl& operator=(const CdmFileImpl&) = delete;

  ~CdmFileImpl() final;

  // media::mojom::CdmFile implementation.
  void Read(ReadCallback callback) final;
  void Write(const std::vector<uint8_t>& data, WriteCallback callback) final;

 private:
  void DidRead(std::optional<std::vector<uint8_t>> data);
  void DidWrite(bool success);

  // Deletes |file_name_| asynchronously.
  void DeleteFile();
  void DidDeleteFile(bool success);

  // Report file operation result and time to UMA.
  void ReportFileOperationUMA(bool success, const std::string& operation);

  void OnReceiverDisconnect();

  // This receiver is associated with the MediaLicenseStorageHost which creates
  // it.
  mojo::AssociatedReceiver<media::mojom::CdmFile> receiver_{this};

  const std::string file_name_;
  const media::CdmType cdm_type_;
  const blink::StorageKey storage_key_;

  // Each of these callbacks is only valid while there is an in-progress read
  // or write operation, respectively.
  ReadCallback read_callback_;
  WriteCallback write_callback_;

  // Time when the read or write operation starts.
  base::TimeTicks start_time_;

  // Backing store which CDM file operations are routed through.
  // Owned by MediaLicenseManager.
  const raw_ptr<MediaLicenseStorageHost> host_ = nullptr;

  // New backing store which CDM file operations are routed through.
  // CdmStorageManager owns the lifetime of this object and will outlive it.
  const raw_ptr<CdmStorageManager> cdm_storage_manager_ = nullptr;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<CdmFileImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_FILE_IMPL_H_
