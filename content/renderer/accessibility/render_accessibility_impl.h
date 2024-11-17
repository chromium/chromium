// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_H_
#define CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_accessibility.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom.h"
#include "third_party/blink/public/web/web_ax_context.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_tree_update.h"

namespace base {
class ElapsedTimer;
}  // namespace base

namespace blink {
class WebDocument;
}  // namespace blink

namespace ui {

struct AXActionData;
class AXActionTarget;

}  // namespace ui

namespace ukm {
class MojoUkmRecorder;
}

namespace content {

class AXAnnotatorsManager;
class RenderFrameImpl;
class RenderAccessibilityManager;

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
  // A call to NotifyAccessibilityModeChange() is required after construction
  // to start accessibility.
  RenderAccessibilityImpl(
      RenderAccessibilityManager* const render_accessibility_manager,
      RenderFrameImpl* const render_frame);

  RenderAccessibilityImpl(const RenderAccessibilityImpl&) = delete;
  RenderAccessibilityImpl& operator=(const RenderAccessibilityImpl&) = delete;

  ~RenderAccessibilityImpl() override;

  void NotifyAccessibilityModeChange(const ui::AXMode& mode);
  ui::AXMode GetAccessibilityMode() { return accessibility_mode_; }

  // RenderAccessibility implementation.
  bool HasActiveDocument() const override;
  ui::AXMode GetAXMode() const override;
  void RecordInaccessiblePdfUkm() override;
  void SetPluginAXTreeActionTargetAdapter(
      PluginAXTreeActionTargetAdapter* adapter) override;
#if BUILDFLAG(IS_CHROMEOS)
  void FireLayoutComplete() override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // RenderFrameObserver implementation.
  void DidCreateNewDocument() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;

  void HitTest(const gfx::Point& point,
               ax::mojom::Event event_to_fire,
               int request_id,
               blink::mojom::RenderAccessibility::HitTestCallback callback);
  void PerformAction(const ui::AXActionData& data);
  void Reset(uint32_t reset_token);

  // Called when accessibility mode changes so that any obsolete accessibility
  // bundles for the old mode can be ignored.
  void set_reset_token(uint32_t reset_token);

  // Called when an accessibility notification occurs in Blink.
  void HandleAXEvent(const ui::AXEvent& event);
  // An AXObject should be serialized at the next available opportunity.
  void MarkWebAXObjectDirty(
      const blink::WebAXObject& obj,
      ax::mojom::EventFrom event_from = ax::mojom::EventFrom::kNone,
      ax::mojom::Action event_from_action = ax::mojom::Action::kNone,
      std::vector<ui::AXEventIntent> event_intents = {},
      ax::mojom::Event event_type = ax::mojom::Event::kNone);

  void NotifyWebAXObjectMarkedDirty(const blink::WebAXObject& obj,
    ax::mojom::Event event_type = ax::mojom::Event::kNone);
  // Called when it is safe to begin a serialization.
  // Returns true if a serialization occurs.
  bool SendAccessibilitySerialization(
      std::vector<ui::AXTreeUpdate> updates,
      std::vector<ui::AXEvent> events,
      ui::AXLocationAndScrollUpdates location_and_scroll_updates,
      bool had_load_complete_messages);

  // Returns the main top-level document for this page, or NULL if there's
  // no view or frame.
  blink::WebDocument GetMainDocument() const;

  blink::WebAXContext* GetAXContext() { return ax_context_.get(); }

  // Returns the page language.
  std::string GetLanguage();

  // Access the UKM recorder.
  ukm::MojoUkmRecorder* ukm_recorder() const { return ukm_recorder_.get(); }

  // Called when the renderer has closed the connection to reset the state
  // machine.
  void ConnectionClosed();

  // The AxID of the first unlabeled image we have encountered in this tree.
  //
  // Used to ensure that the tutor message that explains to screen reader users
  // how to turn on automatic image labels is provided only once.
  mutable std::optional<int32_t> first_unlabeled_image_id_ = std::nullopt;

  // Whether we should highlight annotation results visually on the page
  // for debugging.
  bool image_annotation_debugging_ = false;

  AXAnnotatorsManager* ax_annotators_manager_for_testing() {
    return ax_annotators_manager_.get();
  }

 private:
  // Called whenever the "ack" message is received for a serialization message
  // sent to the browser process, indicating it was received.
  void OnSerializationReceived();

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Handlers for messages from the browser to the renderer.
  void OnLoadInlineTextBoxes(const ui::AXActionTarget* target);
  void OnGetImageData(const ui::AXActionTarget* target,
                      const gfx::Size& max_size);

  // If the document is loaded, fire a load complete event.
  void FireLoadCompleteIfLoaded();

  // Marks all AXObjects with the given role in the current tree dirty.
  void MarkAllAXObjectsDirty(ax::mojom::Role role,
                             ax::mojom::Action event_from_action);

  // Ensure that SendAccessibilitySerialization() will be called at the next
  // available opportunity, so that any dirty objects will be serialized soon.
  void ScheduleImmediateAXUpdate();

  // Returns the document for the active popup if any.
  blink::WebDocument GetPopupDocument();

  blink::WebAXObject ComputeRoot();

  // Sends the URL-keyed metrics for the maximum amount of time spent in
  // SendPendingAccessibilityEvents if they meet the minimum criteria for
  // sending.
  void MaybeSendUKM();

  // Reset all of the UKM data. This can be called after sending UKM data,
  // or after navigating to a new page when any previous data will no
  // longer be valid.
  void ResetUKMData();

  bool SerializeUpdatesAndEvents(blink::WebDocument document,
                                 blink::WebAXObject root,
                                 std::vector<ui::AXEvent>& events,
                                 std::vector<ui::AXTreeUpdate>& updates);

  // The RenderAccessibilityManager that owns us.
  raw_ptr<RenderAccessibilityManager> render_accessibility_manager_;

  // The associated RenderFrameImpl by means of the RenderAccessibilityManager.
  raw_ptr<RenderFrameImpl> render_frame_;

  // This keeps accessibility enabled as long as it lives.
  std::unique_ptr<blink::WebAXContext> ax_context_;

  // Manages generated annotations of the AXTree.
  std::unique_ptr<AXAnnotatorsManager> ax_annotators_manager_;

  raw_ptr<PluginAXTreeActionTargetAdapter> plugin_action_target_adapter_;

  // Token to return this token in the next IPC, so that RenderFrameHostImpl
  // can discard stale data, when the token does not match the expected token.
  std::optional<uint32_t> reset_token_;

  // The specified page language, or empty if unknown.
  std::string page_language_;

  // The URL-keyed metrics recorder interface.
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;

  // The longest amount of time spent serializing the accessibility tree
  // in SendPendingAccessibilityEvents. This is periodically uploaded as
  // a UKM and then reset.
  base::TimeDelta slowest_serialization_time_;

  // Tracks the stage in the loading process for a document, which is used to
  // determine if serialization is part of the loading process or post load.
  enum class LoadingStage {
    // Expect to process a kLoadComplete event.
    kPreload,
    // kLoadComplete event has been processed and waiting on next AXReady call
    // to indicate that we have completed processing all events associated with
    // loading the document.
    kLoadCompleted,
    // All accessibility events associated with the initial document load have
    // been serialized.
    kPostLoad
  };
  LoadingStage loading_stage_ = LoadingStage::kPreload;

  // The amount of time since the last UKM upload.
  std::unique_ptr<base::ElapsedTimer> ukm_timer_;

  // The UKM Source ID that corresponds to the web page represented by
  // slowest_serialization_ms_. We report UKM before the user navigates
  // away, or every few minutes.
  ukm::SourceId last_ukm_source_id_;

  // Note: this is the accessibility mode communicated to this object.
  // The actual accessibility mode on a Document is the combination of this
  // mode and any other active AXContext objects' accessibility modes.
  ui::AXMode accessibility_mode_;

  // So we can queue up tasks to be executed later.
  base::WeakPtrFactory<RenderAccessibilityImpl>
      weak_factory_for_pending_events_{this};

  friend class AXImageAnnotatorTest;
  friend class PluginActionHandlingTest;
  friend class RenderAccessibilityImplTest;
  friend class RenderAccessibilityImplUKMTest;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_RENDER_ACCESSIBILITY_IMPL_H_
