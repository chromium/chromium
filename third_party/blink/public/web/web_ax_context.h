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
}  // namespace ui

namespace blink {

class AXContext;

// An instance of this class, while kept alive, enables accessibility
// support for the given document.
class BLINK_EXPORT WebAXContext {
 public:
  explicit WebAXContext(WebDocument document, const ui::AXMode& mode);
  ~WebAXContext();

  const ui::AXMode& GetAXMode() const;

  void SetAXMode(const ui::AXMode&) const;

  void ResetSerializer();

  // Get a new AXID that's not used by any accessibility node in this process,
  // for when the client needs to insert additional nodes into the accessibility
  // tree.
  int GenerateAXID() const;

  // Retrieves a vector of all WebAXObjects in this document whose
  // bounding boxes may have changed since the last query. Sends that vector
  // via mojo to the browser process.
  void SerializeLocationChanges() const;

  // Searches the accessibility tree for plugin's root object and returns it.
  // Returns an empty WebAXObject if no root object is present.
  WebAXObject GetPluginRoot();

  void Freeze();

  void Thaw();

  bool SerializeEntireTree(bool exclude_offscreen,
                           size_t max_node_count,
                           base::TimeDelta timeout,
                           ui::AXTreeUpdate* response);

  void MarkAllImageAXObjectsDirty();

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

  // Clears out the list of dirty AXObjects and of pending events.
  void ClearDirtyObjectsAndPendingEvents();

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
