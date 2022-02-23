// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_STORAGE_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CDM_STORAGE_IMPL_H_

#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "media/cdm/cdm_type.h"
#include "media/mojo/mojom/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"

namespace storage {
class FileSystemContext;
}

namespace content {
class CdmFileImpl;
class RenderFrameHost;

// This class implements the media::mojom::CdmStorage using the
// PluginPrivateFileSystem for backwards compatibility with CDMs running
// as a pepper plugin.
class CONTENT_EXPORT CdmStorageImpl final
    : public content::DocumentService<media::mojom::CdmStorage> {
 public:
  // Create a CdmStorageImpl object for |cdm_type| and bind it to |request|.
  static void Create(RenderFrameHost* render_frame_host,
                     const media::CdmType& cdm_type,
                     mojo::PendingReceiver<media::mojom::CdmStorage> receiver);

  CdmStorageImpl(const CdmStorageImpl&) = delete;
  CdmStorageImpl& operator=(const CdmStorageImpl&) = delete;

  // media::mojom::CdmStorage implementation.
  void Open(const std::string& file_name, OpenCallback callback) final;

 private:
  // TODO(crbug.com/1231162): Consider reimagining this design once we migrate
  // off of the PluginPrivateFileSystem.
  // File system should only be opened once, so keep track if it has already
  // been opened (or is in the process of opening). State is kError if an error
  // happens while opening the file system.
  enum class FileSystemState { kUnopened, kOpening, kOpened, kError };

  CdmStorageImpl(RenderFrameHost* render_frame_host,
                 const media::CdmType& cdm_type,
                 scoped_refptr<storage::FileSystemContext> file_system_context,
                 mojo::PendingReceiver<media::mojom::CdmStorage> receiver);
  ~CdmStorageImpl() final;

  // Called when the file system is opened.
  void OnFileSystemOpened(base::File::Error error);

  // After the file system is opened, called to create a CdmFile object.
  void CreateCdmFile(const std::string& file_name, OpenCallback callback);

  // Called after the CdmFileImpl object has opened the file.
  void OnCdmFileInitialized(std::unique_ptr<CdmFileImpl> cdm_file_impl,
                            OpenCallback callback,
                            bool success);

  // TODO(crbug.com/1231162): Update this comment once we migrate off of the
  // PluginPrivateFileSystem. CdmType contains a `legacy_file_system_id` used to
  // access the PluginPrivateFileSystem, which will be removed once all data has
  // been migrated.
  // Files are stored in the PluginPrivateFileSystem, so keep track of the CDM
  // file system ID in order to open the files in the correct context.
  const media::CdmType cdm_type_;
  // TODO(crbug.com/1231162): Remove this once we migrate off of the
  // PluginPrivateFileSystem.
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // TODO(crbug.com/1231162): Update this comment and consider removing this
  // member once we migrate off of the PluginPrivateFileSystem.
  // The PluginPrivateFileSystem only needs to be opened once.
  FileSystemState file_system_state_ = FileSystemState::kUnopened;

  // As multiple calls to Open() could happen while the file system is being
  // opened asynchronously, keep track of the requests so they can be
  // processed once the file system is open.
  using PendingOpenData = std::pair<std::string, OpenCallback>;
  std::vector<PendingOpenData> pending_open_calls_;

  // TODO(crbug.com/1231162): Remove this once we migrate off of the
  // PluginPrivateFileSystem.
  // Once the PluginPrivateFileSystem is opened, keep track of the URI that
  // refers to it.
  std::string file_system_root_uri_;

  // This is the child process that will actually read and write the file(s)
  // returned, and it needs permission to access the file(s).
  const int child_process_id_;

  // Keep track of all media::mojom::CdmFile receivers, as each CdmFileImpl
  // object keeps a reference to |this|. If |this| goes away unexpectedly,
  // all remaining CdmFile receivers will be closed.
  mojo::UniqueAssociatedReceiverSet<media::mojom::CdmFile> cdm_file_receivers_;

  base::WeakPtrFactory<CdmStorageImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_STORAGE_IMPL_H_
