// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_FILE_SYSTEM_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_FILE_SYSTEM_FILE_SYSTEM_URL_LOADER_FACTORY_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "storage/browser/file_system/file_system_context.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

// Creates a URLLoaderFactory to serve filesystem: requests from the given
// `file_system_context`, `storage_domain`, and `storage_key`.
//
// The factory is self-owned - it will delete itself once there are no more
// receivers (including the receiver associated with the returned
// mojo::PendingRemote and the receivers bound by the Clone method).
//
// `render_process_host_id` is the ID of the RenderProcessHost where the
// requests are issued.
// - For a factory created for a browser-initiated navigation request:
//   ChildProcessHost::kInvalidUniqueID (there is no process yet).
// - For a factory created to pass to the renderer for subresource requests from
//   the frame: that renderer process's ID.
// - For a factory created for a browser-initiated worker main script request:
//   the ID of the process the worker will run in.
//   TODO(crbug.com/41471904): We should specify kInvalidUniqueID for this
//   worker main script case like the browser-initiated navigation case.
// - For a factory created to pass to the renderer for subresource requests from
//   the worker: that renderer process's ID.
//
// `frame_tree_node_id` is the ID of the FrameTreeNode where the requests are
// associated.
// - For a factory created for a browser-initiated navigation request, or for a
//   factory created for subresource requests from the frame: that frame's ID.
// - For a factory created for workers (which don't have frames):
//   an invalid FrameTreeNodeId.
CONTENT_EXPORT
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateFileSystemURLLoaderFactory(
    int render_process_host_id,
    FrameTreeNodeId frame_tree_node_id,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const std::string& storage_domain,
    const blink::StorageKey& storage_key);

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_FILE_SYSTEM_URL_LOADER_FACTORY_H_
