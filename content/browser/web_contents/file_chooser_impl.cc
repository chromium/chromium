// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/file_chooser_impl.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

// Removes any file that is a symlink or is inside a directory symlink.
// For |kUploadFolder| mode only. |base_dir| is the folder being uploaded.
std::vector<blink::mojom::FileChooserFileInfoPtr> RemoveSymlinks(
    std::vector<blink::mojom::FileChooserFileInfoPtr> files,
    base::FilePath base_dir) {
  DCHECK(!base_dir.empty());
  auto new_end = base::ranges::remove_if(
      files,
      [&base_dir](const base::FilePath& file_path) {
        if (base::IsLink(file_path))
          return true;
        for (base::FilePath path = file_path.DirName(); base_dir.IsParent(path);
             path = path.DirName()) {
          if (base::IsLink(path))
            return true;
        }
        return false;
      },
      [](const auto& file) { return file->get_native_file()->file_path; });
  files.erase(new_end, files.end());
  return files;
}

}  // namespace

FileChooserImpl::FileSelectListenerImpl::FileSelectListenerImpl(
    FileChooserImpl* owner)
    : owner_(owner ? owner->GetWeakPtr() : nullptr) {}

FileChooserImpl::FileSelectListenerImpl::~FileSelectListenerImpl() {
#if DCHECK_IS_ON()
  if (!was_file_select_listener_function_called_) {
    LOG(ERROR) << "Must call either FileSelectListener::FileSelected() or "
                  "FileSelectListener::FileSelectionCanceled()";
  }
  // TODO(avi): Turn on the DCHECK on the following line. This cannot yet be
  // done because I can't say for sure that I know who all the callers who bind
  // blink::mojom::FileChooser are. https://crbug.com/1054811
  /* DCHECK(was_fullscreen_block_set_) << "The fullscreen block was not set"; */
#endif
}

void FileChooserImpl::FileSelectListenerImpl::SetFullscreenBlock(
    base::ScopedClosureRunner fullscreen_block) {
#if DCHECK_IS_ON()
  DCHECK(!was_fullscreen_block_set_)
      << "Fullscreen block must only be set once";
  was_fullscreen_block_set_ = true;
#endif
  fullscreen_block_ = std::move(fullscreen_block);
}

void FileChooserImpl::FileSelectListenerImpl::FileSelected(
    std::vector<blink::mojom::FileChooserFileInfoPtr> files,
    const base::FilePath& base_dir,
    blink::mojom::FileChooserParams::Mode mode) {
#if DCHECK_IS_ON()
  DCHECK(!was_file_select_listener_function_called_)
      << "Must not call both of FileSelectListener::FileSelected() and "
         "FileSelectListener::FileSelectionCanceled()";
  was_file_select_listener_function_called_ = true;
#endif
  if (!owner_)
    return;

  if (mode != blink::mojom::FileChooserParams::Mode::kUploadFolder) {
    owner_->FileSelected(base_dir, mode, std::move(files));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&RemoveSymlinks, std::move(files), base_dir),
      base::BindOnce(&FileChooserImpl::FileSelected, owner_, base_dir, mode));
}

void FileChooserImpl::FileSelectListenerImpl::FileSelectionCanceled() {
#if DCHECK_IS_ON()
  DCHECK(!was_file_select_listener_function_called_)
      << "Should not call both of FileSelectListener::FileSelected() and "
         "FileSelectListener::FileSelectionCanceled()";
  was_file_select_listener_function_called_ = true;
#endif
  if (owner_)
    owner_->FileSelectionCanceled();
}

void FileChooserImpl::FileSelectListenerImpl::
    SetListenerFunctionCalledTrueForTesting() {
#if DCHECK_IS_ON()
  was_file_select_listener_function_called_ = true;
#endif
}

// static
void FileChooserImpl::Create(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingReceiver<blink::mojom::FileChooser> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new FileChooserImpl(render_frame_host)),
      std::move(receiver));
}

// static
mojo::Remote<blink::mojom::FileChooser> FileChooserImpl::CreateBoundForTesting(
    RenderFrameHostImpl* render_frame_host) {
  mojo::Remote<blink::mojom::FileChooser> chooser;
  Create(render_frame_host, chooser.BindNewPipeAndPassReceiver());
  return chooser;
}

// static
std::pair<FileChooserImpl*, mojo::Remote<blink::mojom::FileChooser>>
FileChooserImpl::CreateForTesting(RenderFrameHostImpl* render_frame_host) {
  mojo::Remote<blink::mojom::FileChooser> chooser;
  FileChooserImpl* impl = new FileChooserImpl(render_frame_host);
  mojo::MakeSelfOwnedReceiver(base::WrapUnique(impl),
                              chooser.BindNewPipeAndPassReceiver());
  return std::make_pair(impl, std::move(chooser));
}

FileChooserImpl::FileChooserImpl(RenderFrameHostImpl* render_frame_host)
    : render_frame_host_id_(render_frame_host->GetGlobalId()) {}

FileChooserImpl::~FileChooserImpl() = default;

void FileChooserImpl::OpenFileChooser(blink::mojom::FileChooserParamsPtr params,
                                      OpenFileChooserCallback callback) {
  if (listener_impl_ || !render_frame_host()) {
    std::move(callback).Run(nullptr);
    return;
  }
  callback_ = std::move(callback);
  auto listener = base::MakeRefCounted<FileSelectListenerImpl>(this);
  listener_impl_ = listener.get();
  // Do not allow messages with absolute paths in them as this can permit a
  // renderer to coerce the browser to perform I/O on a renderer controlled
  // path.
  if (params->default_file_name != params->default_file_name.BaseName()) {
    mojo::ReportBadMessage(
        "FileChooser: The default file name must not be an absolute path.");
    listener->FileSelectionCanceled();
    return;
  }

  // Don't allow page with open FileChooser to enter BackForwardCache to avoid
  // any unexpected behaviour from BackForwardCache.
  BackForwardCache::DisableForRenderFrameHost(
      render_frame_host(),
      BackForwardCacheDisable::DisabledReason(
          BackForwardCacheDisable::DisabledReasonId::kFileChooser));

  WebContentsImpl::FromRenderFrameHostImpl(render_frame_host())
      ->RunFileChooser(GetWeakPtr(), render_frame_host(), std::move(listener),
                       *params);
}

void FileChooserImpl::EnumerateChosenDirectory(
    const base::FilePath& directory_path,
    EnumerateChosenDirectoryCallback callback) {
  if (listener_impl_ || !render_frame_host()) {
    std::move(callback).Run(nullptr);
    return;
  }
  callback_ = std::move(callback);
  auto listener = base::MakeRefCounted<FileSelectListenerImpl>(this);
  listener_impl_ = listener.get();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->CanReadFile(render_frame_host()->GetProcess()->GetID(),
                          directory_path)) {
    WebContentsImpl::FromRenderFrameHostImpl(render_frame_host())
        ->EnumerateDirectory(GetWeakPtr(), render_frame_host(),
                             std::move(listener), directory_path);
  } else {
    listener->FileSelectionCanceled();
  }
}

void FileChooserImpl::FileSelected(
    const base::FilePath& base_dir,
    blink::mojom::FileChooserParams::Mode mode,
    std::vector<blink::mojom::FileChooserFileInfoPtr> files) {
  listener_impl_ = nullptr;
  if (!render_frame_host()) {
    std::move(callback_).Run(nullptr);
    return;
  }
  storage::FileSystemContext* file_system_context = nullptr;
  const int pid = render_frame_host()->GetProcess()->GetID();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  // Grant the security access requested to the given files.
  for (const auto& file : files) {
    if (mode == blink::mojom::FileChooserParams::Mode::kSave) {
      policy->GrantCreateReadWriteFile(pid, file->get_native_file()->file_path);
    } else {
      if (file->is_file_system()) {
        if (!file_system_context) {
          file_system_context = render_frame_host()
                                    ->GetStoragePartition()
                                    ->GetFileSystemContext();
        }
        policy->GrantReadFileSystem(
            pid, file_system_context
                     ->CrackURLInFirstPartyContext(file->get_file_system()->url)
                     .mount_filesystem_id());
      } else {
        policy->GrantReadFile(pid, file->get_native_file()->file_path);
      }
    }
  }
  std::move(callback_).Run(FileChooserResult::New(std::move(files), base_dir));
}

void FileChooserImpl::FileSelectionCanceled() {
  listener_impl_ = nullptr;
  std::move(callback_).Run(nullptr);
}

RenderFrameHostImpl* FileChooserImpl::render_frame_host() {
  RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(render_frame_host_id_);
  if (rfh && rfh->IsRenderFrameLive()) {
    return rfh;
  }
  return nullptr;
}

}  // namespace content
