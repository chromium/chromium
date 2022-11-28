// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/save_as_web_bundle_job.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/download/public/common/download_task_runner.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"

namespace content {

// static
void SaveAsWebBundleJob::Start(
    WebContentsImpl* web_contents,
    const base::FilePath& file_path,
    base::OnceCallback<void(uint64_t /* file_size */,
                            data_decoder::mojom::WebBundlerError)> callback) {
  std::vector<
      mojo::PendingRemote<data_decoder::mojom::ResourceSnapshotForWebBundle>>
      snapshots;
  // Only include nodes from the primary frame tree, since a Web Bundle
  // would not be able to load inner frame trees (e.g. fenced frames).
  // TODO(1321102): Consider supporting inner frame trees.
  for (FrameTreeNode* node : web_contents->GetPrimaryFrameTree().Nodes()) {
    if (node->current_frame_host()->inner_tree_main_frame_tree_node_id() !=
        FrameTreeNode::kFrameTreeNodeInvalidId) {
      // Skip inner tree placeholder nodes.
      continue;
    }
    mojo::Remote<data_decoder::mojom::ResourceSnapshotForWebBundle> snapshot;
    node->current_frame_host()
        ->GetAssociatedLocalFrame()
        ->GetResourceSnapshotForWebBundle(
            snapshot.BindNewPipeAndPassReceiver());
    snapshots.push_back(snapshot.Unbind());
  }

  new SaveAsWebBundleJob(file_path, std::move(snapshots), std::move(callback));
}

SaveAsWebBundleJob::SaveAsWebBundleJob(
    const base::FilePath& file_path,
    std::vector<
        mojo::PendingRemote<data_decoder::mojom::ResourceSnapshotForWebBundle>>
        snapshots,
    base::OnceCallback<void(uint64_t /* file_size */,
                            data_decoder::mojom::WebBundlerError)> callback)
    : data_decoder_(std::make_unique<data_decoder::DataDecoder>()),
      snapshots_(std::move(snapshots)),
      callback_(std::move(callback)) {
  download::GetDownloadTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& file_path) {
            const uint32_t file_flags =
                base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;
            return base::File(file_path, file_flags);
          },
          file_path),
      base::BindOnce(&SaveAsWebBundleJob::OnFileAvailable,
                     base::Unretained(this)));
}

SaveAsWebBundleJob::~SaveAsWebBundleJob() = default;

void SaveAsWebBundleJob::OnFileAvailable(base::File file) {
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to create file to save page as WebBundle.";
    OnFinished(0, data_decoder::mojom::WebBundlerError::kFileOpenFailed);
    return;
  }
  data_decoder_->GetService()->BindWebBundler(
      bundler_.BindNewPipeAndPassReceiver());
  bundler_.set_disconnect_handler(base::BindOnce(
      &SaveAsWebBundleJob::OnConnectionError, base::Unretained(this)));
  bundler_->Generate(
      std::move(snapshots_), std::move(file),
      base::BindOnce(&SaveAsWebBundleJob::OnGenerated, base::Unretained(this)));
}

void SaveAsWebBundleJob::OnConnectionError() {
  OnFinished(0,
             data_decoder::mojom::WebBundlerError::kWebBundlerConnectionError);
}

void SaveAsWebBundleJob::OnGenerated(
    uint64_t file_size,
    data_decoder::mojom::WebBundlerError error) {
  OnFinished(file_size, error);
}

void SaveAsWebBundleJob::OnFinished(
    uint64_t file_size,
    data_decoder::mojom::WebBundlerError error) {
  DCHECK(callback_);
  std::move(callback_).Run(file_size, error);
  delete this;  // This is the last time the SaveAsWebBundleJob is referenced.
}

}  // namespace content
