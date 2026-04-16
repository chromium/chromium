// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_STREAM_INFO_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_STREAM_INFO_H_

#include <cstdint>
#include <memory>
#include <string>

#include "content/public/browser/frame_tree_node_id.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace extensions {

class MimeHandlerStreamDelegate;
class StreamContainer;

// Information about a single stream navigation. Stores the
// `StreamContainer` and tracks the state of extension and content
// frame navigations for that stream.
class StreamInfo {
 public:
  StreamInfo(const std::string& embed_internal_id,
             std::unique_ptr<StreamContainer> stream_container,
             std::unique_ptr<MimeHandlerStreamDelegate> delegate);

  StreamInfo(const StreamInfo&) = delete;
  StreamInfo& operator=(const StreamInfo&) = delete;
  StreamInfo(StreamInfo&&) = delete;
  StreamInfo& operator=(StreamInfo&&) = delete;

  ~StreamInfo();

  const std::string& internal_id() const { return internal_id_; }

  StreamContainer* stream() { return stream_.get(); }

  MimeHandlerStreamDelegate* delegate() { return delegate_.get(); }
  const MimeHandlerStreamDelegate* delegate() const { return delegate_.get(); }

  bool did_extension_finish_navigation() const {
    return did_extension_finish_navigation_;
  }

  const mojo::AssociatedRemote<
      extensions::mojom::MimeHandlerViewContainerManager>&
  mime_handler_view_container_manager() const {
    return container_manager_;
  }

  void set_mime_handler_view_container_manager(
      mojo::AssociatedRemote<extensions::mojom::MimeHandlerViewContainerManager>
          container_manager) {
    container_manager_ = std::move(container_manager);
  }

  int32_t instance_id() const { return instance_id_; }

  void SetDidExtensionFinishNavigation();

  bool DidPdfExtensionStartNavigation() const;

  bool DidPdfContentNavigate() const;

  content::FrameTreeNodeId extension_host_frame_tree_node_id() const {
    return extension_host_frame_tree_node_id_;
  }

  void set_extension_host_frame_tree_node_id(
      content::FrameTreeNodeId frame_tree_node_id) {
    extension_host_frame_tree_node_id_ = frame_tree_node_id;
  }

  content::FrameTreeNodeId content_host_frame_tree_node_id() const {
    return content_host_frame_tree_node_id_;
  }

  void set_content_host_frame_tree_node_id(
      content::FrameTreeNodeId frame_tree_node_id) {
    content_host_frame_tree_node_id_ = frame_tree_node_id;
  }

 private:
  // A unique ID for the viewer instance. Used to set up postMessage
  // support for the full-page viewer.
  const std::string internal_id_;

  // A container for the stream. Holds data needed to load the content
  // in the viewer.
  const std::unique_ptr<StreamContainer> stream_;

  // Non-null. MIME-type-specific delegate for this stream.
  const std::unique_ptr<MimeHandlerStreamDelegate> delegate_;

  // True if the extension host has finished navigating to the
  // extension URL.
  bool did_extension_finish_navigation_ = false;

  // The container manager used to provide postMessage support.
  mojo::AssociatedRemote<extensions::mojom::MimeHandlerViewContainerManager>
      container_manager_;

  // The frame tree node ID of the extension host. Initialized when
  // the initial about:blank navigation commits in the extension
  // frame.
  content::FrameTreeNodeId extension_host_frame_tree_node_id_;

  // The frame tree node ID of the content host. Initialized when the
  // navigation to the stream URL starts.
  content::FrameTreeNodeId content_host_frame_tree_node_id_;

  // A unique ID for this instance. Used for postMessage support to
  // identify `extensions::MimeHandlerViewFrameContainer` objects.
  int32_t instance_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_STREAM_INFO_H_
