// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_H_
#define CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/ax_content_node_data.h"
#include "content/common/content_export.h"
#include "content/common/render_accessibility.mojom.h"
#include "content/public/renderer/plugin_ax_tree_source.h"
#include "content/public/renderer/render_accessibility.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/accessibility/blink_ax_tree_source.h"
#include "third_party/blink/public/web/web_ax_context.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {
class WebDocument;
}  // namespace blink

namespace ui {
struct AXActionData;
class AXActionTarget;
struct AXEvent;
}

namespace content {

class AXImageAnnotator;
class RenderFrameImpl;
class RenderAccessibilityManager;

using BlinkAXTreeSerializer = ui::
    AXTreeSerializer<blink::WebAXObject, AXContentNodeData, AXContentTreeData>;

class AXTreeSnapshotterImpl : public AXTreeSnapshotter {
 public:
  explicit AXTreeSnapshotterImpl(RenderFrameImpl* render_frame);
  ~AXTreeSnapshotterImpl() override;

  // AXTreeSnapshotter implementation.
  void Snapshot(ui::AXMode ax_mode,
                size_t max_node_count,
                ui::AXTreeUpdate* accessibility_tree) override;

  // Same as above, but returns in |accessibility_tree| a AXContentTreeUpdate
  // with content-specific metadata, instead of an AXTreeUpdate.
  void SnapshotContentTree(ui::AXMode ax_mode,
                           size_t max_node_count,
                           AXContentTreeUpdate* accessibility_tree);

 private:
  RenderFrameImpl* render_frame_;
  std::unique_ptr<blink::WebAXContext> context_;

  AXTreeSnapshotterImpl(const AXTreeSnapshotterImpl&) = delete;
  AXTreeSnapshotterImpl& operator=(const AXTreeSnapshotterImpl&) = delete;
};

// The browser process implements native accessibility APIs, allowing assistive
// technology (e.g., screen readers, magnifiers) to access and control the web
// contents with high-level APIs. These APIs are also used by automation tools,
// and Windows 8 uses them to determine when the on-screen keyboard should be
// shown.
//
// An instance of this class belongs to the RenderAccessibilityManager object.
// Accessibility is initialized based on the ui::AXMode passed from the browser
// process to the manager object; it lazily starts as Off or EditableTextOnly
// depending on the operating system, and switches to Complete if assistive
// technology is detected or a flag is set.
//
// A tree of accessible objects is built here and sent to the browser process;
// the browser process maintains this as a tree of platform-native accessible
// objects that can be used to respond to accessibility requests from other
// processes.
//
// This class implements complete accessibility support for assistive
// technology. It turns on Blink's accessibility code and sends a serialized
// representation of that tree whenever it changes. It also handles requests
// from the browser to perform accessibility actions on nodes in the tree (e.g.,
// change focus, or click on a button).
class CONTENT_EXPORT RenderAccessibilityImpl : public RenderAccessibility,
                                               public RenderFrameObserver {
 public:
  // Request a one-time snapshot of the accessibility tree without
  // enabling accessibility if it wasn't already enabled.
  static void SnapshotAccessibilityTree(RenderFrameImpl* render_frame,
                                        AXContentTreeUpdate* response,
                                        ui::AXMode ax_mode);

  RenderAccessibilityImpl(
      RenderAccessibilityManager* const render_accessibility_manager,
      RenderFrameImpl* const render_frame,
      ui::AXMode mode);
  ~RenderAccessibilityImpl() override;

  ui::AXMode GetAccessibilityMode() {
    return tree_source_.accessibility_mode();
  }

  // RenderAccessibility implementation.
  int GenerateAXID() override;
  void SetPluginTreeSource(PluginAXTreeSource* source) override;
  void OnPluginRootNodeUpdated() override;
  void ShowPluginContextMenu() override;

  // RenderFrameObserver implementation.
  void DidCreateNewDocument() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void AccessibilityModeChanged(const ui::AXMode& mode) override;

  void HitTest(const ui::AXActionData& action_data,
               mojom::RenderAccessibility::HitTestCallback callback);
  void PerformAction(const ui::AXActionData& data);
  void Reset(int32_t reset_token);

  // Called when an accessibility notification occurs in Blink.
  void HandleWebAccessibilityEvent(const ui::AXEvent& event);
  void MarkWebAXObjectDirty(const blink::WebAXObject& obj, bool subtree);

  void HandleAXEvent(const ui::AXEvent& event);

  // Returns the main top-level document for this page, or NULL if there's
  // no view or frame.
  blink::WebDocument GetMainDocument();

  // Returns the page language.
  std::string GetLanguage();

 protected:
  // Send queued events from the renderer to the browser.
  void SendPendingAccessibilityEvents();

  // Check the entire accessibility tree to see if any nodes have
  // changed location, by comparing their locations to the cached
  // versions. If any have moved, send an IPC with the new locations.
  void SendLocationChanges();

 private:
  struct DirtyObject {
    DirtyObject();
    DirtyObject(const DirtyObject& other);
    ~DirtyObject();
    blink::WebAXObject obj;
    ax::mojom::EventFrom event_from;
    std::vector<ui::AXEventIntent> event_intents;
  };

  enum class EventScheduleMode { kDeferEvents, kProcessEventsImmediately };

  enum class EventScheduleStatus {
    // Events have been scheduled with a delay, but have not been sent.
    kScheduledDeferred,
    // Events have been scheduled without a delay, but have not been sent.
    kScheduledImmediate,
    // Events have been sent, waiting for callback.
    kWaitingForAck,
    // Events are not scheduled and we are not waiting for an ack.
    kNotWaiting
  };

  // Callback that will be called from the browser upon handling the message
  // previously sent to it via SendPendingAccessibilityEvents().
  void OnAccessibilityEventsHandled();

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Handlers for messages from the browser to the renderer.
  void OnLoadInlineTextBoxes(const ui::AXActionTarget* target);
  void OnGetImageData(const ui::AXActionTarget* target,
                      const gfx::Size& max_size);
  void AddPluginTreeToUpdate(AXContentTreeUpdate* update,
                             bool invalidate_plugin_subtree);

  // Creates and takes ownership of an instance of the class that automatically
  // labels images for accessibility.
  void CreateAXImageAnnotator();

  // Automatically labels images for accessibility if the accessibility mode for
  // this feature is turned on, otherwise stops automatic labeling and removes
  // any automatic annotations that might have been added before.
  void StartOrStopLabelingImages(ui::AXMode old_mode, ui::AXMode new_mode);

  // Marks all AXObjects with the given role in the current tree dirty.
  void MarkAllAXObjectsDirty(ax::mojom::Role role);

  void Scroll(const ui::AXActionTarget* target,
              ax::mojom::Action scroll_action);
  // If we are calling this from a task, scheduling is allowed even if there is
  // a running task
  void ScheduleSendPendingAccessibilityEvents(
      bool scheduling_from_task = false);
  void AddImageAnnotationDebuggingAttributes(
      const std::vector<AXContentTreeUpdate>& updates);

  // Returns the document for the active popup if any.
  blink::WebDocument GetPopupDocument();

  // Searches the accessibility tree for plugin's root object and returns it.
  // Returns an empty WebAXObject if no root object is present.
  blink::WebAXObject GetPluginRoot();

  // Cancels scheduled events that are not yet in flight
  void CancelScheduledEvents();

  // The RenderAccessibilityManager that owns us.
  RenderAccessibilityManager* render_accessibility_manager_;

  // The associated RenderFrameImpl by means of the RenderAccessibilityManager.
  RenderFrameImpl* render_frame_;

  // This keeps accessibility enabled as long as it lives.
  std::unique_ptr<blink::WebAXContext> ax_context_;

  // Manages the automatic image annotations, if enabled.
  std::unique_ptr<AXImageAnnotator> ax_image_annotator_;

  // Events from Blink are collected until they are ready to be
  // sent to the browser.
  std::vector<ui::AXEvent> pending_events_;

  // Objects that need to be re-serialized, the next time
  // we send an event bundle to the browser - but don't specifically need
  // an event fired.
  std::vector<DirtyObject> dirty_objects_;

  // The adapter that exposes Blink's accessibility tree to AXTreeSerializer.
  BlinkAXTreeSource tree_source_;

  // The serializer that sends accessibility messages to the browser process.
  BlinkAXTreeSerializer serializer_;

  using PluginAXTreeSerializer = ui::AXTreeSerializer<const ui::AXNode*,
                                                      ui::AXNodeData,
                                                      ui::AXTreeData>;
  std::unique_ptr<PluginAXTreeSerializer> plugin_serializer_;
  PluginAXTreeSource* plugin_tree_source_;
  blink::WebAXObject plugin_host_node_;

  // Current location of every object, so we can detect when it moves.
  std::unordered_map<int, ui::AXRelativeBounds> locations_;

  // The most recently observed scroll offset of the root document element.
  // TODO(dmazzoni): remove once https://bugs.webkit.org/show_bug.cgi?id=73460
  // is fixed.
  gfx::Size last_scroll_offset_;

  // Current event scheduling status
  EventScheduleStatus event_schedule_status_;

  // Nonzero if the browser requested we reset the accessibility state.
  // We need to return this token in the next IPC.
  int reset_token_;

  // Whether or not we've injected a stylesheet in this document
  // (only when debugging flags are enabled, never under normal circumstances).
  bool has_injected_stylesheet_ = false;

  // We defer events to improve performance durung the initial page load.
  EventScheduleMode event_schedule_mode_ = EventScheduleMode::kDeferEvents;

  // Whether we should highlight annotation results visually on the page
  // for debugging.
  bool image_annotation_debugging_ = false;

  // The specified page language, or empty if unknown.
  std::string page_language_;

  // So we can queue up tasks to be executed later.
  base::WeakPtrFactory<RenderAccessibilityImpl>
      weak_factory_for_pending_events_{this};

  friend class AXImageAnnotatorTest;
  friend class PluginActionHandlingTest;
  friend class RenderAccessibilityImplTest;

  DISALLOW_COPY_AND_ASSIGN(RenderAccessibilityImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_H_
