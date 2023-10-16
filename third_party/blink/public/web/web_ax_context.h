// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_CONTEXT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_CONTEXT_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "ui/accessibility/ax_event.h"

namespace ui {
class AXMode;
struct AXTreeUpdate;
}  // namespace ui

namespace blink {

class AXContext;

// An instance of this class, while kept alive, enables accessibility
// support for the given document.
class BLINK_EXPORT WebAXContext {
 public:
  explicit WebAXContext(WebDocument document, const ui::AXMode& mode);
  ~WebAXContext();

  bool HasActiveDocument() const;
  bool HasAXObjectCache() const;

  const ui::AXMode& GetAXMode() const;

  void SetAXMode(const ui::AXMode&) const;

  // Recompute the entire tree and reserialize it.
  // This method is useful when something that potentially affects most of the
  // page occurs, such as an inertness change or a fullscreen toggle.
  // This keeps the existing nodes, but recomputes all of their properties and
  // reserializes everything.
  // Compared with ResetSerializer() and MarkAXObjectDirtyWithDetails() with
  // subtree = true, this does more work, because it recomputes the entire tree
  // structure and properties of each node.
  void MarkDocumentDirty();

  // Compared with MarkDocumentDirty(), this does less work, because it assumes
  // the AXObjectCache's tree of objects and properties is correct, but needs to
  // be reserialized.
  void ResetSerializer();

  // Get a new AXID that's not used by any accessibility node in this process,
  // for when the client needs to insert additional nodes into the accessibility
  // tree.
  int GenerateAXID() const;

  // Retrieves a vector of all WebAXObjects in this document whose
  // bounding boxes may have changed since the last query. Sends that vector
  // via mojo to the browser process.
  void SerializeLocationChanges(uint32_t reset_token) const;

  // Searches the accessibility tree for plugin's root object and returns it.
  // Returns an empty WebAXObject if no root object is present.
  WebAXObject GetPluginRoot();

  bool SerializeEntireTree(size_t max_node_count,
                           base::TimeDelta timeout,
                           ui::AXTreeUpdate* response);

  // Serialize all AXObjects that are dirty (have changed their state since
  // the last serialization) into |updates|. (Heuristically) skips
  // serializing dirty nodes whose AX id is in |already_serialized_ids|, and
  // adds serialized dirty objects into |already_serialized_ids|.
  void SerializeDirtyObjectsAndEvents(bool has_plugin_tree_source,
                                      std::vector<ui::AXTreeUpdate>& updates,
                                      std::vector<ui::AXEvent>& events,
                                      bool& had_end_of_test_event,
                                      bool& had_load_complete_messages,
                                      bool& need_to_send_location_changes);

  // Returns a vector of the images found in |updates|.
  void GetImagesToAnnotate(ui::AXTreeUpdate& updates,
                           std::vector<ui::AXNodeData*>&);

  // Note that any pending event also causes its corresponding object to
  // become dirty.
  bool HasDirtyObjects();

  // Adds the event to a list of pending events that is cleared out by
  // a subsequent call to SerializeDirtyObjectsAndEvents. Returns false if
  // the event is already pending.
  bool AddPendingEvent(const ui::AXEvent& event,
                       bool insert_at_beginning = false);

  // Ensure that accessibility is clean and up-to-date for both the main and
  // popup document. Ensures layout is clean as well.
  void UpdateAXForAllDocuments();

  // Ensure that a layout and accessibility update will occur soon.
  void ScheduleAXUpdate();

  // If the document is loaded, fire a load complete event.
  void FireLoadCompleteIfLoaded();

 private:
  std::unique_ptr<AXContext> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_CONTEXT_H_
