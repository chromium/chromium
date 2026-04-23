// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_MANAGER_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_MANAGER_H_

#include <map>
#include <memory>
#include <optional>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/mime_handler/stream_info.h"

namespace content {
struct GlobalRenderFrameHostId;
class NavigationHandle;
class RenderFrameHost;
class SiteInstance;
class WebContents;
}  // namespace content

namespace extensions {

class MimeHandlerStreamDelegate;
class StreamContainer;

namespace mime_handler {

// `MimeHandlerStreamManager` is used for MIME handler navigation. It tracks all
// MIME handler navigation events in a `content::WebContents`. It handles
// multiple MIME handler instances in a single `content::WebContents`. It is
// responsible for:
// 1. Storing the `extensions::StreamContainer` stream data.
// 2. Observing for the MIME handler frames either navigating or closing
//    (including by crashing). This is necessary to ensure that streams that
//    aren't claimed are not leaked, by deleting the stream if any of those
//    events occur.
// 3. Observing for the RFH created by the embedder to load the MIME handler
//    extension URL.
// 4. Observing for content navigations and dispatching the content-frame
//    handling to the stream delegate.
// `MimeHandlerStreamManager` is scoped to the `content::WebContents` it tracks,
// but it may also delete itself if all streams are no longer used.
// `extensions::StreamContainer` objects are stored from
// `PluginResponseInterceptorURLLoaderThrottle::WillProcessResponse()` until
// the MIME handler is no longer in use.
//
// Use `MimeHandlerStreamManager::Create()` to create an instance.
// Use `MimeHandlerStreamManager::FromWebContents()` to get an instance.
class MimeHandlerStreamManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MimeHandlerStreamManager> {
 public:
  // A factory interface used to generate test stream managers.
  class Factory {
   public:
    // If MimeHandlerStreamManager has a factory set, then
    // `MimeHandlerStreamManager::Create()` will automatically use
    // `CreateMimeHandlerStreamManager()` to create the stream manager if
    // necessary for MIME handler navigations.
    virtual void CreateMimeHandlerStreamManager(
        content::WebContents* contents) = 0;

   protected:
    virtual ~Factory() = default;
  };

  // Information about the embedder RFH needed to store and retrieve stream
  // containers.
  struct EmbedderHostInfo {
    // Need this comparator since this struct is used as a key in the
    // `stream_infos_` map.
    bool operator<(const EmbedderHostInfo& other) const;

    // Using the frame tree node ID to identify the embedder RFH is necessary
    // because entries are added during
    // `PluginResponseInterceptorURLLoaderThrottle::WillProcessResponse()`,
    // before the embedder's frame tree node has swapped from its previous RFH
    // to the embedder RFH that will host the MIME handler.
    content::FrameTreeNodeId frame_tree_node_id;
    content::GlobalRenderFrameHostId global_id;
  };

  // Creates a `MimeHandlerStreamManager` for `contents`, if one doesn't already
  // exist.
  static void Create(content::WebContents* contents);

  // Use `Create()` to create an instance instead.
  static void CreateForWebContents(content::WebContents*) = delete;

  MimeHandlerStreamManager(const MimeHandlerStreamManager&) = delete;
  MimeHandlerStreamManager& operator=(const MimeHandlerStreamManager&) = delete;
  ~MimeHandlerStreamManager() override;

  // Returns a pointer to the `MimeHandlerStreamManager` instance associated
  // with the `content::WebContents` of `render_frame_host`.
  static MimeHandlerStreamManager* FromRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  // Overrides factory for testing. Default (nullptr) value indicates regular
  // (non-test) environment.
  static void SetFactoryForTesting(Factory* factory);

  // Starts tracking a `StreamContainer` in an embedder FrameTreeNode, before
  // the embedder host commits. The `StreamContainer` is considered unclaimed
  // until the embedder host commits, at which point the `StreamContainer` is
  // tracked by both the frame tree node ID and the render frame host ID.
  // Replaces existing unclaimed entries with the same `frame_tree_node_id`.
  // This can occur if an embedder frame navigating to a handled URL starts
  // navigating to another handled URL before the original `StreamContainer` is
  // claimed.
  // `delegate` must not be null.
  void AddStreamContainer(
      content::FrameTreeNodeId frame_tree_node_id,
      const std::string& internal_id,
      std::unique_ptr<extensions::StreamContainer> stream_container,
      std::unique_ptr<extensions::MimeHandlerStreamDelegate> delegate);

  // Returns a pointer to a stream container that `embedder_host` has claimed or
  // nullptr if `embedder_host` hasn't claimed any stream containers.
  base::WeakPtr<extensions::StreamContainer> GetStreamContainer(
      content::RenderFrameHost* embedder_host);

  // Returns true if `render_frame_host` is an extension host for a MIME
  // handler. During a load, the initial RFH for the extension frame commits to
  // the about:blank URL. Another RFH will then be chosen to host the extension.
  // This returns true for both hosts. Depending on what navigation step the
  // frame is on, callers can also check the last committed origin to
  // differentiate between the hosts.
  bool IsExtensionHost(const content::RenderFrameHost* render_frame_host) const;

  // Returns true if `frame_tree_node_id` is the frame tree node ID for the
  // extension frame under `embedder_host`, false otherwise.
  bool IsExtensionFrameTreeNodeId(
      const content::RenderFrameHost* embedder_host,
      content::FrameTreeNodeId frame_tree_node_id) const;

  // Returns true if `embedder_host` has an extension frame and it has already
  // finished its navigation, false otherwise.
  bool DidExtensionFrameFinishNavigation(
      const content::RenderFrameHost* embedder_host) const;

  // Returns true if `render_frame_host` is a content host for a MIME handler.
  // During a load, the initial RFH for the content frame attempts to navigate
  // to the stream URL. Another RFH will then be chosen to host the content
  // frame. This returns true for both hosts. Depending on what navigation step
  // the frame is on, callers can also check the last committed URL to
  // differentiate between the hosts.
  bool IsContentHost(const content::RenderFrameHost* render_frame_host) const;

  // Returns true if `frame_tree_node_id` is the frame tree node ID for the
  // content frame under `embedder_host`, false otherwise.
  bool IsContentFrameTreeNodeId(
      const content::RenderFrameHost* embedder_host,
      content::FrameTreeNodeId frame_tree_node_id) const;

  // Returns true if `embedder_host` has a content frame and it has already
  // finished its navigation, false otherwise.
  bool DidContentFrameFinishNavigation(
      const content::RenderFrameHost* embedder_host) const;

  // Returns whether the handler plugin should handle save events.
  bool PluginCanSave(const content::RenderFrameHost* embedder_host) const;

  // Set whether the handler plugin should handle save events.
  void SetPluginCanSave(content::RenderFrameHost* embedder_host,
                        bool plugin_can_save);

  // Returns whether there's an unclaimed stream info with the default embedder
  // host info.
  bool ContainsUnclaimedStreamInfo(
      content::FrameTreeNodeId frame_tree_node_id) const;

  // Deletes the unclaimed stream info associated with `frame_tree_node_id`, and
  // deletes `this` if there are no remaining stream infos. Callers must ensure
  // such a stream info exists before calling this, otherwise crashes.
  void DeleteUnclaimedStreamInfo(content::FrameTreeNodeId frame_tree_node_id);

  // WebContentsObserver overrides.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void FrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // For testing only. Mark an unclaimed stream info with the same frame tree
  // node ID as `embedder_host` as claimed by `embedder_host`. Callers must
  // ensure such a stream info exists before calling this.
  void ClaimStreamInfoForTesting(content::RenderFrameHost* embedder_host);

  // For testing only. Returns a pointer to the StreamInfo claimed by
  // `embedder_host`, or nullptr if there is no claimed stream.
  extensions::StreamInfo* GetClaimedStreamInfoForTesting(
      content::RenderFrameHost* embedder_host);

  // For testing only. Set `embedder_host`'s extension frame tree node ID as
  // `frame_tree_node_id`. This is needed to listen for extension host deletion.
  // Callers must ensure that `embedder_host` has a claimed stream info.
  void SetExtensionFrameTreeNodeIdForTesting(
      content::RenderFrameHost* embedder_host,
      content::FrameTreeNodeId frame_tree_node_id);

  // For testing only. Set `embedder_host`'s content frame tree node ID as
  // `frame_tree_node_id`. This is needed to listen for content host deletion.
  // Callers must ensure that `embedder_host` has a claimed stream info.
  void SetContentFrameTreeNodeIdForTesting(
      content::RenderFrameHost* embedder_host,
      content::FrameTreeNodeId frame_tree_node_id);

 protected:
  // Use `Create()` to create an instance instead.
  explicit MimeHandlerStreamManager(content::WebContents* contents);

  // Returns the stream info claimed by `embedder_host`, or nullptr if there's
  // no existing stream.
  extensions::StreamInfo* GetClaimedStreamInfo(
      const content::RenderFrameHost* embedder_host);
  const extensions::StreamInfo* GetClaimedStreamInfo(
      const content::RenderFrameHost* embedder_host) const;

  // Returns the claimed StreamInfo whose delegate owns this navigation as its
  // content-frame navigation, or nullptr if no claimed stream owns it.
  extensions::StreamInfo* GetClaimedStreamInfoFromContentNavigation(
      content::NavigationHandle* navigation_handle);

  // Navigates the FrameTreeNode with ID `extension_host_frame_tree_node_id` to
  // the extension URL. Marks the extension as navigated in `stream_info`, which
  // must be non-null. `source_site_instance` should be the
  // `content::SiteInstance` of the embedder frame that will be initiating the
  // navigation.
  //
  // Subclasses may override this for use in callbacks. If so, `global_id`,
  // which is the ID for the intermediate about:blank host for the extension
  // frame, can be used to get the other parameters safely.
  virtual void NavigateToExtensionUrl(
      content::FrameTreeNodeId extension_host_frame_tree_node_id,
      extensions::StreamInfo* stream_info,
      content::SiteInstance* source_site_instance,
      content::GlobalRenderFrameHostId global_id);

 private:
  friend class content::WebContentsUserData<MimeHandlerStreamManager>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  using StreamInfoMap =
      std::map<EmbedderHostInfo, std::unique_ptr<extensions::StreamInfo>>;

  // Mark an unclaimed stream info with the same frame tree node ID as
  // `embedder_host` as claimed by `embedder_host`. Returns a pointer to the
  // claimed stream info. Callers must ensure such a stream info exists with
  // `ContainsUnclaimedStreamInfo()` before calling this.
  extensions::StreamInfo* ClaimStreamInfo(
      content::RenderFrameHost* embedder_host);

  // Deletes the claimed stream info associated with `embedder_host`, and
  // deletes `this` if there are no remaining stream infos.
  void DeleteClaimedStreamInfo(content::RenderFrameHost* embedder_host);

  // Called when a RenderFrameHost in the observed WebContents is replaced or
  // deleted. If `old_host` is an extension host, deletes the associated stream.
  // The extension host is a generic concept — all MIME handlers have one.
  // Deletes `this` if there are no remaining streams. Returns true if the
  // stream was deleted, false otherwise.
  [[nodiscard]] bool MaybeDeleteStreamOnExtensionHostChanged(
      content::RenderFrameHost* old_host);

  // Same as above, but for the content host. The content host is optional —
  // only some MIME handler types (e.g. OOPIF PDF) use a content frame.
  [[nodiscard]] bool MaybeDeleteStreamOnContentHostChanged(
      content::RenderFrameHost* old_host);

  // Intended to be called during the PDF content frame's
  // `ReadyToCommitNavigation()` event. Registers navigations occurring in a PDF
  // content frame as a subresource.
  bool MaybeRegisterPdfSubresourceOverride(
      content::NavigationHandle* navigation_handle);

  // Intended to be called during the PDF content frame's 'DidFinishNavigation'.
  // Sets up postMessage communication between the embedder frame and the PDF
  // extension frame after the PDF has finished loading.
  bool MaybeSetUpPostMessage(content::NavigationHandle* navigation_handle);

  // During the PDF content frame navigation, set the related PDF stream's
  // content host frame tree node ID.
  void SetStreamContentHostFrameTreeNodeId(
      content::NavigationHandle* navigation_handle);

  // Stores stream info by embedder host info.
  StreamInfoMap stream_infos_;
};

}  // namespace mime_handler
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_STREAM_MANAGER_H_
