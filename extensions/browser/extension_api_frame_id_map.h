// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_API_FRAME_ID_MAP_H_
#define EXTENSIONS_BROWSER_EXTENSION_API_FRAME_ID_MAP_H_

#include <map>
#include <memory>
#include <set>

#include "base/lazy_instance.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/global_routing_id.h"
#include "extensions/common/api/extension_types.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {

// Extension frame IDs are exposed through the chrome.* APIs and have the
// following characteristics:
// - The top-level frame has ID 0.
// - Any child frame has a positive ID.
// - A non-existent frame has ID -1.
// - They are only guaranteed to be unique within a tab.
// - The ID does not change during the frame's lifetime and is not re-used after
//   the frame is removed. The frame may change its current RenderFrameHost over
//   time, so multiple RenderFrameHosts may map to the same extension frame ID.
//
// This class provides a mapping from a (render_process_id, frame_routing_id)
// pair to a FrameData struct, which includes the extension's frame id (as
// described above), the parent frame id, and the tab id (the latter can be
// invalid if it's not in a tab).
//
// Unless stated otherwise, the methods can only be called on the UI thread.
//
// The non-static methods of this class use an internal cache.
class ExtensionApiFrameIdMap {
 public:
  using DocumentId = base::UnguessableToken;

  // The data for a RenderFrame. Every GlobalRenderFrameHostId maps to a
  // FrameData.
  struct FrameData {
    FrameData();
    FrameData(int frame_id,
              int parent_frame_id,
              int tab_id,
              int window_id,
              const DocumentId& document_id,
              const DocumentId& parent_document_id,
              api::extension_types::FrameType frame_type,
              api::extension_types::DocumentLifecycle document_lifecycle);
    ~FrameData();

    FrameData(const FrameData&);
    FrameData& operator=(const FrameData&);

    // The extension API frame ID of the frame.
    int frame_id;

    // The extension API frame ID of the parent of the frame.
    int parent_frame_id;

    // The id of the tab that the frame is in, or -1 if the frame isn't in a
    // tab.
    int tab_id;

    // The id of the window that the frame is in, or -1 if the frame isn't in a
    // window.
    int window_id;

    // The extension API document ID of the document in the frame.
    DocumentId document_id;

    // The extension API document ID of the parent document of the frame.
    DocumentId parent_document_id;

    // The type that this frame represents.
    api::extension_types::FrameType frame_type =
        api::extension_types::FrameType::kNone;

    // The lifecycle state the frame is currently in.
    api::extension_types::DocumentLifecycle document_lifecycle =
        api::extension_types::DocumentLifecycle::kNone;
  };

  // An invalid extension API frame ID.
  static const int kInvalidFrameId;

  // Extension API frame ID of the top-level frame.
  static const int kTopFrameId;

  ExtensionApiFrameIdMap(const ExtensionApiFrameIdMap&) = delete;
  ExtensionApiFrameIdMap& operator=(const ExtensionApiFrameIdMap&) = delete;

  static ExtensionApiFrameIdMap* Get();

  // Get the extension API frame ID for |render_frame_host|.
  static int GetFrameId(content::RenderFrameHost* render_frame_host);

  // Get the extension API frame ID for |navigation_handle|.
  static int GetFrameId(content::NavigationHandle* navigation_handle);

  // Get the extension API frame ID for the parent of |render_frame_host|.
  static int GetParentFrameId(content::RenderFrameHost* render_frame_host);

  // Get the extension API frame ID for the parent of |navigation_handle|.
  static int GetParentFrameId(content::NavigationHandle* navigation_handle);

  // Get the extension API document ID for the current document of
  // |render_frame_host|.
  static DocumentId GetDocumentId(content::RenderFrameHost* render_frame_host);

  // Get the extension API document ID for the document of |navigation_handle|.
  static DocumentId GetDocumentId(content::NavigationHandle* navigation_handle);

  // Gets the context ID (as used in `runtime.getContexts()`) for the given
  // `render_frame_host`).
  static base::Uuid GetContextId(content::RenderFrameHost* render_frame_host);

  // Get the extension API frame type for the current document of
  // |render_frame_host|.
  static api::extension_types::FrameType GetFrameType(
      content::RenderFrameHost* render_frame_host);

  // Get the extension API frame type for the frame of |navigation_handle|.
  static api::extension_types::FrameType GetFrameType(
      content::NavigationHandle* navigation_handle);

  // Get the extension API document lifecycle for the current document of
  // |render_frame_host|.
  static api::extension_types::DocumentLifecycle GetDocumentLifecycle(
      content::RenderFrameHost* render_frame_host);

  // Get the extension API document lifecycle for the frame of
  // |navigation_handle|.
  static api::extension_types::DocumentLifecycle GetDocumentLifecycle(
      content::NavigationHandle* navigation_handle);

  // Find the current RenderFrameHost for a given WebContents and extension
  // frame ID.
  // Returns nullptr if not found.
  static content::RenderFrameHost* GetRenderFrameHostById(
      content::WebContents* web_contents,
      int frame_id);

  // Find the current RenderFrameHost for a given extension documentID.
  // Returns nullptr if not found.
  content::RenderFrameHost* GetRenderFrameHostByDocumentId(
      const DocumentId& document_id);

  // Parses a serialized document id string to a DocumentId.
  static DocumentId DocumentIdFromString(const std::string& document_id);

  // Retrieves the FrameData for a given RenderFrameHost id.
  [[nodiscard]] FrameData GetFrameData(
      content::GlobalRenderFrameHostId render_frame_host_id);

  // Called when a render frame is deleted. Stores the FrameData for
  // |render_frame_host| in the deleted frames map so it can still be accessed
  // for beacon requests. The FrameData will be removed later in a task.
  void OnRenderFrameDeleted(content::RenderFrameHost* render_frame_host);

 protected:
  friend struct base::LazyInstanceTraitsBase<ExtensionApiFrameIdMap>;
  class ExtensionDocumentUserData
      : public content::DocumentUserData<ExtensionDocumentUserData> {
   public:
    explicit ExtensionDocumentUserData(
        content::RenderFrameHost* render_frame_host);
    ~ExtensionDocumentUserData() override;

    const DocumentId& document_id() const { return document_id_; }
    const base::Uuid& context_id() const { return context_id_; }

   private:
    friend content::DocumentUserData<ExtensionDocumentUserData>;
    DOCUMENT_USER_DATA_KEY_DECL();

    DocumentId document_id_;
    base::Uuid context_id_;
  };

  ExtensionApiFrameIdMap();
  ~ExtensionApiFrameIdMap();

  // Determines the value to be stored in |frame_data_map_| for a given key.
  // If |require_live_frame| is true, FrameData will only
  // Returns empty FrameData when the corresponding RenderFrameHost is not
  // alive and |require_live_frame| is true.
  FrameData KeyToValue(content::GlobalRenderFrameHostId key,
                       bool require_live_frame) const;
  FrameData KeyToValue(content::RenderFrameHost* render_frame_host,
                       bool require_live_frame) const;

  // Holds mappings of render frame key to FrameData from frames that have been
  // recently deleted. These are kept for a short time so beacon requests that
  // continue after a frame is unloaded can access the FrameData.
  using FrameDataMap = std::map<content::GlobalRenderFrameHostId, FrameData>;
  FrameDataMap deleted_frame_data_map_;

  // Holds mapping of DocumentIds to ExtensionDocumentUserData objects.
  using DocumentIdMap = std::map<DocumentId, ExtensionDocumentUserData*>;
  DocumentIdMap document_id_map_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_API_FRAME_ID_MAP_H_
