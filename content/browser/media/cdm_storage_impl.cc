// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_impl.h"

#include <map>
#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "content/browser/media/cdm_file_impl.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ppapi/shared_impl/ppapi_constants.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/origin.h"

// Currently this uses the PluginPrivateFileSystem as the previous CDMs ran
// as pepper plugins and we need to be able to access any existing files.
// TODO(jrummell): Switch to using a separate file system once CDMs no
// longer run as pepper plugins.

namespace content {

// static
void CdmStorageImpl::Create(
    RenderFrameHost* render_frame_host,
    const std::string& cdm_file_system_id,
    mojo::PendingReceiver<media::mojom::CdmStorage> receiver) {
  DVLOG(3) << __func__;
  DCHECK(!render_frame_host->GetLastCommittedOrigin().opaque())
      << "Invalid origin specified for CdmStorageImpl::Create";

  // Take a reference to the FileSystemContext.
  scoped_refptr<storage::FileSystemContext> file_system_context;
  StoragePartition* storage_partition =
      render_frame_host->GetProcess()->GetStoragePartition();
  if (storage_partition)
    file_system_context = storage_partition->GetFileSystemContext();

  // The created object is bound to (and owned by) |receiver|.
  new CdmStorageImpl(render_frame_host, cdm_file_system_id,
                     std::move(file_system_context), std::move(receiver));
}

// static
bool CdmStorageImpl::IsValidCdmFileSystemId(
    const std::string& cdm_file_system_id) {
  // To be compatible with PepperFileSystemBrowserHost::GeneratePluginId(),
  // |cdm_file_system_id| must contain only letters (A-Za-z), digits(0-9),
  // or "._-".
  for (const auto& ch : cdm_file_system_id) {
    if (!base::IsAsciiAlpha(ch) && !base::IsAsciiDigit(ch) && ch != '.' &&
        ch != '_' && ch != '-') {
      return false;
    }
  }

  // Also ensure that |cdm_file_system_id| contains at least 1 character.
  return !cdm_file_system_id.empty();
}

CdmStorageImpl::CdmStorageImpl(
    RenderFrameHost* render_frame_host,
    const std::string& cdm_file_system_id,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    mojo::PendingReceiver<media::mojom::CdmStorage> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)),
      cdm_file_system_id_(cdm_file_system_id),
      file_system_context_(std::move(file_system_context)),
      child_process_id_(render_frame_host->GetProcess()->GetID()) {}

CdmStorageImpl::~CdmStorageImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void CdmStorageImpl::Open(const std::string& file_name, OpenCallback callback) {
  DVLOG(3) << __func__ << " file: " << file_name;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!IsValidCdmFileSystemId(cdm_file_system_id_)) {
    DVLOG(1) << "CdmStorageImpl not initialized properly.";
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  if (file_name.empty()) {
    DVLOG(1) << "No file specified.";
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  // The file system should only be opened once. If it has been attempted and
  // failed, we can't create the CdmFile objects.
  if (file_system_state_ == FileSystemState::kError) {
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  // If the file system is already open, create and initialize the CdmFileImpl
  // object.
  if (file_system_state_ == FileSystemState::kOpened) {
    CreateCdmFile(file_name, std::move(callback));
    return;
  }

  // Save a file name and callback for when the file system is open. If the
  // open is already in progress, nothing more to do until the existing
  // OpenPluginPrivateFileSystem() call completes.
  pending_open_calls_.emplace(pending_open_calls_.end(), file_name,
                              std::move(callback));
  if (file_system_state_ == FileSystemState::kOpening)
    return;

  DCHECK_EQ(FileSystemState::kUnopened, file_system_state_);
  file_system_state_ = FileSystemState::kOpening;

  std::string fsid =
      storage::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          storage::kFileSystemTypePluginPrivate, ppapi::kPluginPrivateRootName,
          base::FilePath());
  if (!storage::ValidateIsolatedFileSystemId(fsid)) {
    DVLOG(1) << "Invalid file system ID.";
    OnFileSystemOpened(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // Grant full access of isolated file system to child process.
  ChildProcessSecurityPolicy::GetInstance()->GrantCreateReadWriteFileSystem(
      child_process_id_, fsid);

  // Keep track of the URI for this instance of the PluginPrivateFileSystem.
  file_system_root_uri_ = storage::GetIsolatedFileSystemRootURIString(
      origin().GetURL(), fsid, ppapi::kPluginPrivateRootName);

  file_system_context_->OpenPluginPrivateFileSystem(
      origin().GetURL(), storage::kFileSystemTypePluginPrivate, fsid,
      cdm_file_system_id_, storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::BindOnce(&CdmStorageImpl::OnFileSystemOpened,
                     weak_factory_.GetWeakPtr()));
}

void CdmStorageImpl::OnFileSystemOpened(base::File::Error error) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(FileSystemState::kOpening, file_system_state_);

  if (error != base::File::FILE_OK) {
    file_system_state_ = FileSystemState::kError;
    // All pending calls will fail.
    for (auto& pending : pending_open_calls_) {
      std::move(pending.second)
          .Run(Status::kFailure, mojo::NullAssociatedRemote());
    }
    pending_open_calls_.clear();
    return;
  }

  // File system successfully opened, so create the CdmFileImpl object for
  // all pending Open() calls.
  file_system_state_ = FileSystemState::kOpened;
  for (auto& pending : pending_open_calls_) {
    CreateCdmFile(pending.first, std::move(pending.second));
  }
  pending_open_calls_.clear();
}

void CdmStorageImpl::CreateCdmFile(const std::string& file_name,
                                   OpenCallback callback) {
  DVLOG(3) << __func__ << " file: " << file_name;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(FileSystemState::kOpened, file_system_state_);

  // File system opened successfully, so create an CdmFileImpl object and
  // initialize it (which only grabs the lock to prevent any other access to the
  // file except through this object).
  if (!CdmFileImpl::IsValidName(file_name)) {
    std::move(callback).Run(Status::kFailure, mojo::NullAssociatedRemote());
    return;
  }

  auto cdm_file_impl = std::make_unique<CdmFileImpl>(
      file_name, origin(), cdm_file_system_id_, file_system_root_uri_,
      file_system_context_);

  if (!cdm_file_impl->Initialize()) {
    // Unable to initialize with the file requested.
    std::move(callback).Run(Status::kInUse, mojo::NullAssociatedRemote());
    return;
  }

  // File was opened successfully, so create the binding and return success.
  mojo::PendingAssociatedRemote<media::mojom::CdmFile> cdm_file;
  cdm_file_receivers_.Add(std::move(cdm_file_impl),
                          cdm_file.InitWithNewEndpointAndPassReceiver());
  std::move(callback).Run(Status::kSuccess, std::move(cdm_file));
}

}  // namespace content
