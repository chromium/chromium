/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/device_posture_provider.mojom-blink-forward.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom-blink-forward.h"
#include "third_party/blink/public/common/frame/frame_ad_evidence.h"
#include "third_party/blink/public/common/frame/transient_allow_fullscreen.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/reporting_observer.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom-blink.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/script/script_evaluation_params.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink-forward.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/weak_identifier_map.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/loader/back_forward_cache_loader_helper_impl.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/loader/fetch/loader_freeze_mode.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_unique_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/gfx/geometry/transform.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gfx {
class Point;
class Range;
class Size;
class SizeF;
}  // namespace gfx

namespace blink {

class AdTracker;
class AssociatedInterfaceProvider;
class AttributionSrcLoader;
class BackgroundColorPaintImageGenerator;
class BoxShadowPaintImageGenerator;
class BrowserInterfaceBrokerProxy;
class ClipPathPaintImageGenerator;
class Color;
class ContentCaptureManager;
class CoreProbeSink;
class Document;
class Editor;
class Element;
class EventHandler;
class EventHandlerRegistry;
class FrameConsole;
class FrameOverlay;
class FrameSelection;
class FrameWidget;
class IdlenessDetector;
class InputMethodController;
class InspectorIssueReporter;
class InspectorTaskRunner;
class InspectorTraceEvents;
class InterfaceRegistry;
class LayoutView;
class LocalDOMWindow;
class LocalFrameClient;
class LocalFrameMojoHandler;
class LocalWindowProxy;
class Node;
class NodeTraversal;
class PerformanceMonitor;
class PluginData;
class PolicyContainer;
class ScrollSnapshotClient;
class SmoothScrollSequencer;
class SpellChecker;
class StorageKey;
class SystemClipboard;
class TextFragmentHandler;
class TextSuggestionController;
class VirtualKeyboardOverlayChangedObserver;
class WebContentSettingsClient;
class WebInputEventAttribution;
class WebPluginContainerImpl;
class WebPrescientNetworking;
class WebURLLoaderFactory;
struct BlinkTransferableMessage;
struct WebScriptSource;

enum class BackForwardCacheAware;

#if !BUILDFLAG(IS_ANDROID)
class WindowControlsOverlayChangedDelegate;
#endif

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<LocalFrame>;

// A LocalFrame is a frame hosted inside this process.
//
// LocalFrame should not inherit from Mojo interfaces, and should not have
// data members for Mojo remotes in order to avoid including full mojom headers
// from local_frame.h. LocalFrameMojoHandler should do them instead.
//
// Blink code should not directly use LocalFrameMojoHandler. If Blink code
// needs to call a function that is exposed as a Mojo method, the function
// implementation should be in LocalFrame, and LocalFrameMojoHandler should
// delegate to LocalFrame's implementation.
class CORE_EXPORT LocalFrame final
    : public Frame,
      public FrameScheduler::Delegate,
      public BackForwardCacheLoaderHelperImpl::Delegate,
      public Supplementable<LocalFrame> {
 public:
  // Returns the LocalFrame instance for the given |frame_token|.
  static LocalFrame* FromFrameToken(const LocalFrameToken& frame_token);

  // For a description of |inheriting_agent_factory| go see the comment on the
  // Frame constructor.
  LocalFrame(
      LocalFrameClient*,
      Page&,
      FrameOwner*,
      Frame* parent,
      Frame* previous_sibling,
      FrameInsertType insert_type,
      const LocalFrameToken& frame_token,
      WindowAgentFactory* inheriting_agent_factory,
      InterfaceRegistry*,
      const base::TickClock* clock = base::DefaultTickClock::GetInstance());

  // Initialize the LocalFrame, creating and initializing its LocalDOMWindow. It
  // starts from the initial empty document.
  // - |policy_container| is used to set the PolicyContainer of the new
  //   LocalDOMWindow. If you pass a null |policy_container|, it will be
  //   initialized to an empty, default one, which has no PolicyContainerHost
  //   counterpart. This is usually safe to do if this LocalFrame has no
  //   corresponding RenderFrameHost.
  // - |storage_key| is the key used to partition access to storage API like DOM
  //   storage, IndexedDB, BroadcastChannel, etc...
  // - |document_ukm_source_id| is the ukm source id for the new document. If
  //   you pass ukm::kInvalidSourceId, a new ukm source id will be generated.
  //
  // Note: Usually, the initial empty document inherits its |policy_container|
  // and |storage_key| from the parent or the opener. The inheritance operation
  // is taken care of by the browser (if this LocalFrame was just created in
  // response to the creation of a RenderFrameHost) or by blink if this is a
  // synchronously created LocalFrame child.
  void Init(Frame* opener,
            const DocumentToken& document_token,
            std::unique_ptr<PolicyContainer> policy_container,
            const StorageKey& storage_key,
            ukm::SourceId document_ukm_source_id);
  void SetView(LocalFrameView*);
  void CreateView(const gfx::Size&, const Color&);

  // Frame overrides:
  ~LocalFrame() override;
  void Trace(Visitor*) const override;
  void Navigate(FrameLoadRequest&, WebFrameLoadType) override;
  bool ShouldClose() override;
  const SecurityContext* GetSecurityContext() const override;
  void PrintNavigationErrorMessage(const Frame&, const String& reason);
  void PrintNavigationWarning(const String&);
  bool DetachDocument() override;
  void CheckCompleted() override;
  void DidChangeVisibilityState() override;
  void HookBackForwardCacheEviction() override;
  void RemoveBackForwardCacheEviction() override;
  void SetTextDirection(base::i18n::TextDirection direction) override;
  // This sets the is_inert_ flag and also recurses through this frame's
  // subtree, updating the inert bit on all descendant frames.
  void SetIsInert(bool) override;
  void SetInheritedEffectiveTouchAction(TouchAction) override;
  void DidFocus() override;
  bool IsAdFrame() const override;

  // BackForwardCacheLoaderHelperImpl::Delegate:
  void EvictFromBackForwardCache(
      mojom::blink::RendererEvictionReason reason) override;
  void DidBufferLoadWhileInBackForwardCache(size_t num_bytes) override;

  void DidChangeThemeColor(bool update_theme_color_cache);
  void DidChangeBackgroundColor(SkColor background_color, bool color_adjust);

  // Returns false if detaching child frames reentrantly detached `this`.
  bool DetachChildren();
  // After Document is attached, resets state related to document, and sets
  // context to the current document.
  void DidAttachDocument();

  void Reload(WebFrameLoadType);

  // Note: these three functions are not virtual but intentionally shadow the
  // corresponding method in the Frame base class to return the
  // LocalFrame-specific subclass.
  LocalWindowProxy* WindowProxy(DOMWrapperWorld&);
  LocalWindowProxy* WindowProxyMaybeUninitialized(DOMWrapperWorld&);
  LocalDOMWindow* DomWindow();
  const LocalDOMWindow* DomWindow() const;
  void SetDOMWindow(LocalDOMWindow*);
  LocalFrameView* View() const override;
  Document* GetDocument() const;
  void SetPagePopupOwner(Element&);
  Element* PagePopupOwner() const { return page_popup_owner_.Get(); }
  bool HasPagePopupOwner() const { return page_popup_owner_; }

  // Root of the layout tree for the document contained in this frame.
  LayoutView* ContentLayoutObject() const;

  Editor& GetEditor() const;
  EventHandler& GetEventHandler() const;
  EventHandlerRegistry& GetEventHandlerRegistry() const;
  FrameLoader& Loader() const;
  FrameSelection& Selection() const;
  InputMethodController& GetInputMethodController() const;
  TextSuggestionController& GetTextSuggestionController() const;
  SpellChecker& GetSpellChecker() const;
  FrameConsole& Console() const;
  BackgroundColorPaintImageGenerator* GetBackgroundColorPaintImageGenerator();
  BoxShadowPaintImageGenerator* GetBoxShadowPaintImageGenerator();
  ClipPathPaintImageGenerator* GetClipPathPaintImageGenerator();

  // A local root is the root of a connected subtree that contains only
  // LocalFrames. The local root is responsible for coordinating input, layout,
  // et cetera for that subtree of frames.
  bool IsLocalRoot() const;
  LocalFrame& LocalFrameRoot() const;

  CoreProbeSink* GetProbeSink() { return probe_sink_.Get(); }
  scoped_refptr<InspectorTaskRunner> GetInspectorTaskRunner();

  // Returns ContentCaptureManager in LocalFrameRoot, create or destroy it as
  // needed.
  ContentCaptureManager* GetOrResetContentCaptureManager();

  // Returns the current state of caret browsing mode.
  bool IsCaretBrowsingEnabled() const;

  // Activates the user activation states of the |LocalFrame| (provided it's
  // non-null) and all its ancestors.
  //
  // The |notification_type| parameter is used for histograms only.
  static void NotifyUserActivation(
      LocalFrame*,
      mojom::blink::UserActivationNotificationType notification_type,
      bool need_browser_verification = false);

  // Returns the transient user activation state of the |LocalFrame|, provided
  // it is non-null.  Otherwise returns |false|.
  static bool HasTransientUserActivation(LocalFrame*);

  // Consumes the transient user activation state of the |LocalFrame|, provided
  // the frame pointer is non-null and the state hasn't been consumed since
  // activation.  Returns |true| if successfully consumed the state.
  static bool ConsumeTransientUserActivation(
      LocalFrame*,
      UserActivationUpdateSource update_source =
          UserActivationUpdateSource::kRenderer);

  // Registers an observer that will be notified if a VK occludes
  // the content when it raises/dismisses. The observer is a HeapHashSet
  // data structure that doesn't allow duplicates.
  void RegisterVirtualKeyboardOverlayChangedObserver(
      VirtualKeyboardOverlayChangedObserver*);

  // Notify |virtual_keyboard_overlay_changed_observers_| that keyboard overlay
  // rect has changed.
  void NotifyVirtualKeyboardOverlayRectObservers(const gfx::Rect&) const;

  // Bubbles a logical scroll to the parent frame, if one exists. For a local
  // frame, this will continue the scroll synchronously. For remote frames and
  // frame tree boundaries, this will IPC the scroll via the browser process.
  // Returns true if the scroll is locally consumed, false otherwise.
  bool BubbleLogicalScrollInParentFrame(mojom::blink::ScrollDirection direction,
                                        ui::ScrollGranularity granularity);

  // Receives and continues a bubbled logical scroll from the child frame (sent
  // via the method above). This can either be called synchronously by the
  // method above or from the RemoteFrame child after being sent via IPC.
  // Returns true if the scroll is locally consumed, false otherwise.
  bool BubbleLogicalScrollFromChildFrame(
      mojom::blink::ScrollDirection direction,
      ui::ScrollGranularity granularity,
      Frame* child);

  // =========================================================================
  // All public functions below this point are candidates to move out of
  // LocalFrame into another class.

  // See layers_as_json.h for accepted flags.
  String GetLayerTreeAsTextForTesting(unsigned flags = 0) const;

  // Begin printing with the given page size information.
  // The frame content will fit to the page size with specified shrink ratio.
  // If this frame doesn't need to fit into a page size, default values are
  // used.
  void StartPrinting(const gfx::SizeF& page_size = gfx::SizeF(),
                     const gfx::SizeF& original_page_size = gfx::SizeF(),
                     float maximum_shrink_ratio = 0);

  void EndPrinting();
  bool ShouldUsePrintingLayout() const;

  // Setup for a Paint Preview of the page which will paint the full page
  // contents.
  void StartPaintPreview();
  void EndPaintPreview();

  // Save the current scroll offset of the scrollable area associated with the
  // given node (if not already saved). All saved scroll offsets can be restored
  // via RestoreScrollOffsets() (this will also clear all entries for saved
  // scroll offsets).
  void EnsureSaveScrollOffset(Node&);
  void RestoreScrollOffsets();

  gfx::SizeF ResizePageRectsKeepingRatio(const gfx::SizeF& original_size,
                                         const gfx::SizeF& expected_size) const;

  bool InViewSourceMode() const;
  void SetInViewSourceMode(bool = true);

  void SetPageZoomFactor(float);
  float PageZoomFactor() const { return page_zoom_factor_; }
  void SetTextZoomFactor(float);
  float TextZoomFactor() const { return text_zoom_factor_; }
  void SetPageAndTextZoomFactors(float page_zoom_factor,
                                 float text_zoom_factor);

  double DevicePixelRatio() const;

  // Informs the local root's document and its local descendant subtree that a
  // media query value changed.
  void MediaQueryAffectingValueChangedForLocalSubtree(MediaValueChange);

  void WindowSegmentsChanged(const WebVector<gfx::Rect>& window_segments);
  void UpdateViewportSegmentCSSEnvironmentVariables(
      const WebVector<gfx::Rect>& window_segments);

  device::mojom::blink::DevicePostureType GetDevicePosture();

  String SelectedText() const;
  String SelectedTextForClipboard() const;
  void TextSelectionChanged(const WTF::String& selection_text,
                            uint32_t offset,
                            const gfx::Range& range) const;

  PositionWithAffinityTemplate<EditingAlgorithm<NodeTraversal>>
  PositionForPoint(const PhysicalOffset& frame_point);
  Document* DocumentAtPoint(const PhysicalOffset&);

  void RemoveSpellingMarkersUnderWords(const Vector<String>& words);

  bool ShouldThrottleRendering() const;

  // Called on the main frame of the portal is activated or adopted.
  void PortalStateChanged();

  // Returns frame scheduler for this frame.
  // FrameScheduler is destroyed during frame detach and nullptr will be
  // returned after it.
  FrameScheduler* GetFrameScheduler();
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(TaskType);
  void ScheduleVisualUpdateUnlessThrottled();

  bool IsNavigationAllowed() const { return navigation_disable_count_ == 0; }

  // destination_url is only used when a navigation is blocked due to
  // framebusting defenses, in order to give the option of restarting the
  // navigation at a later time.
  bool CanNavigate(const Frame&, const KURL& destination_url = KURL());

  void WillPotentiallyStartOutermostMainFrameNavigation(const KURL& url) const;

  // Whether a navigation should replace the current history entry or not.
  // Note this isn't exhaustive; there are other cases where a navigation does a
  // replacement which this function doesn't cover.
  bool NavigationShouldReplaceCurrentHistoryEntry(
      const FrameLoadRequest& request,
      WebFrameLoadType frame_load_type);

  // Return this frame's BrowserInterfaceBroker. Must not be called on detached
  // frames (that is, frames where `Client()` returns nullptr).
  BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker();

  InterfaceRegistry* GetInterfaceRegistry() { return interface_registry_; }

  // Returns an AssociatedInterfaceProvider the frame can use to request
  // navigation-associated interfaces from the browser. Messages transmitted
  // over such interfaces will be dispatched in FIFO order with respect to each
  // other and messages implementing navigation.
  //
  // Carefully consider whether an interface needs to be navigation-associated
  // before introducing new navigation-associated interfaces.
  //
  // Navigation-associated interfaces are currently implemented as
  // channel-associated interfaces. See
  // https://chromium.googlesource.com/chromium/src/+/main/docs/mojo_ipc_conversion.md#Channel_Associated-Interfaces
  AssociatedInterfaceProvider* GetRemoteNavigationAssociatedInterfaces();

  LocalFrameClient* Client() const;

  // Returns the widget for this frame, or from the nearest ancestor which is a
  // local root. It is never null for frames in ordinary Pages (which means the
  // Page is inside a WebView), except very early in initialization. For frames
  // in a non-ordinary Page (without a WebView, such as in unit tests, popups,
  // devtools), it will always be null.
  FrameWidget* GetWidgetForLocalRoot();

  WebContentSettingsClient* GetContentSettingsClient();

  PluginData* GetPluginData() const;

  PerformanceMonitor* GetPerformanceMonitor() { return performance_monitor_; }
  IdlenessDetector* GetIdlenessDetector() { return idleness_detector_; }
  AdTracker* GetAdTracker() { return ad_tracker_; }
  void SetAdTrackerForTesting(AdTracker* ad_tracker);
  AttributionSrcLoader* GetAttributionSrcLoader() {
    return attribution_src_loader_;
  }

  enum class LazyLoadImageSetting { kDisabled, kEnabledExplicit };
  // Returns the enabled state of lazyloading of images.
  LazyLoadImageSetting GetLazyLoadImageSetting() const;

  // The returned value is a off-heap raw-ptr and should not be stored.
  WebURLLoaderFactory* GetURLLoaderFactory();

  bool IsInert() const { return is_inert_; }

  // If the frame hosts a PluginDocument, this method returns the
  // WebPluginContainerImpl that hosts the plugin. If the provided node is a
  // plugin, then it returns its WebPluginContainerImpl. Otherwise, uses the
  // currently focused element (if any).
  // TODO(slangley): Refactor this method to extract the logic of looking up
  // focused element or passed node into explicit methods.
  WebPluginContainerImpl* GetWebPluginContainer(Node* = nullptr) const;

  // Called on a view for a LocalFrame with a RemoteFrame parent. This makes
  // viewport intersection and occlusion/obscuration available that accounts for
  // remote ancestor frames and their respective scroll positions, clips, etc.
  void SetViewportIntersectionFromParent(
      const mojom::blink::ViewportIntersectionState& intersection_state);

  gfx::Size GetOutermostMainFrameSize() const override;
  gfx::Point GetOutermostMainFrameScrollPosition() const override;

  void SetOpener(Frame* opener) override;

  // See viewport_intersection_state.mojom for more info on these
  // methods.
  gfx::Rect RemoteViewportIntersection() const {
    return intersection_state_.viewport_intersection;
  }
  gfx::Rect RemoteMainFrameIntersection() const {
    return intersection_state_.main_frame_intersection;
  }
  gfx::Transform RemoteMainFrameTransform() const {
    return intersection_state_.main_frame_transform;
  }

  mojom::blink::FrameOcclusionState GetOcclusionState() const;

  bool NeedsOcclusionTracking() const;

  // Replaces the initial empty document with a Document suitable for
  // |mime_type| and populated with the contents of |data|. Only intended for
  // use in internal-implementation LocalFrames that aren't in the frame tree.
  void ForceSynchronousDocumentInstall(const AtomicString& mime_type,
                                       scoped_refptr<const SharedBuffer> data);

  bool should_send_resource_timing_info_to_parent() const {
    return should_send_resource_timing_info_to_parent_;
  }
  void SetShouldSendResourceTimingInfoToParent(bool value) {
    should_send_resource_timing_info_to_parent_ = value;
  }

  // Called when certain event listeners are added for the first time/last time,
  // making it possible/not possible to terminate the frame suddenly.
  void UpdateSuddenTerminationStatus(
      bool added_listener,
      mojom::blink::SuddenTerminationDisablerType disabler_type);

  // Called when we added an event listener that might affect sudden termination
  // disabling of the page.
  void AddedSuddenTerminationDisablerListener(const EventTarget& event_target,
                                              const AtomicString& event_type);
  // Called when we removed event listeners that might affect sudden termination
  // disabling of the page.
  void RemovedSuddenTerminationDisablerListener(const EventTarget& event_target,
                                                const AtomicString& event_type);

  // TODO(https://crbug.com/578349): provisional frames are a hack that should
  // be removed.
  bool IsProvisional() const;

  // Whether the frame is considered to be a root ad frame by Ad Tagging.
  bool IsAdRoot() const;

  // Called by the embedder on creation of the initial empty document and, for
  // all other documents, just before commit (ReadyToCommitNavigation time).
  void SetAdEvidence(const blink::FrameAdEvidence& ad_evidence);

  // This is used to check if a script tagged as an ad is currently on the v8
  // stack.
  bool IsAdScriptInStack() const;

  // The evidence for or against a frame being an ad. `absl::nullopt` if not yet
  // set or if the frame is a subfiltering root frame (outermost main frame or
  // portal) as only child frames can be tagged as ads.
  const absl::optional<blink::FrameAdEvidence>& AdEvidence() const {
    return ad_evidence_;
  }

  bool IsFrameCreatedByAdScript() const {
    return is_frame_created_by_ad_script_;
  }

  // Updates the frame color overlay to match the highlight ad setting.
  void UpdateAdHighlight();

  // Binds |receiver| and prevents resource loading until either the frame is
  // navigated or the receiver pipe is closed.
  void PauseSubresourceLoading(
      mojo::PendingReceiver<blink::mojom::blink::PauseSubresourceLoadingHandle>
          receiver);

  void ResumeSubresourceLoading();

  void AnimateSnapFling(base::TimeTicks monotonic_time);

  ClientHintsPreferences& GetClientHintsPreferences() {
    return client_hints_preferences_;
  }

  SmoothScrollSequencer& GetSmoothScrollSequencer();

  mojom::blink::ReportingServiceProxy* GetReportingService();

  // Returns the frame host ptr. The interface returned is backed by an
  // associated interface with the legacy Chrome IPC channel.
  mojom::blink::LocalFrameHost& GetLocalFrameHostRemote() const;

  // Returns the bfcache controller host ptr. The interface returned is backed
  // by an associated interface with the legacy Chrome IPC channel.
  mojom::blink::BackForwardCacheControllerHost&
  GetBackForwardCacheControllerHostRemote();

  // Sets back/forward cache NotRestoredReasons for this frame. Only set for
  // outermost main frame.
  void SetNotRestoredReasons(
      mojom::blink::BackForwardCacheNotRestoredReasonsPtr);
  const mojom::blink::BackForwardCacheNotRestoredReasonsPtr&
  GetNotRestoredReasons();

  const AtomicString& GetReducedAcceptLanguage() const {
    return reduced_accept_language_;
  }

  void SetReducedAcceptLanguage(const AtomicString& reduced_accept_language);

  // Overlays a color on top of this LocalFrameView if it is associated with
  // the main frame. Should not have multiple consumers.
  void SetMainFrameColorOverlay(SkColor color);

  // Overlays a color on top of this LocalFrameView if it is associated with
  // a subframe. Should not have multiple consumers.
  void SetSubframeColorOverlay(SkColor color);
  void UpdateFrameColorOverlayPrePaint();

  void PaintFrameColorOverlay(GraphicsContext&);

  // To be called from OomInterventionImpl.
  void ForciblyPurgeV8Memory();

  void OnPageLifecycleStateUpdated();

  void WasHidden();
  void WasShown();
  bool IsHidden() const { return hidden_; }

  // Whether the frame clips its content to the frame's size.
  bool ClipsContent() const;

  // For a navigation initiated from this LocalFrame with user gesture, record
  // the UseCounter AdClickNavigation if this frame is an adframe.
  //
  // TODO(crbug.com/939370): Currently this is called in a couple of sites,
  // which is fragile and prone to break. If we have the ad status in
  // RemoteFrame, we could call it at FrameLoader::StartNavigation where all
  // navigations go through.
  void MaybeLogAdClickNavigation();

  // Triggers a use counter if a feature, which is currently available in all
  // frames, would be blocked by the introduction of permissions policy. This
  // takes two counters (which may be the same). It triggers
  // |blockedCrossOrigin| if the frame is cross-origin relative to the top-level
  // document, and triggers |blockedSameOrigin| if it is same-origin with the
  // top level, but is embedded in any way through a cross-origin frame.
  // (A->B->A embedding)
  void CountUseIfFeatureWouldBeBlockedByPermissionsPolicy(
      mojom::WebFeature blocked_cross_origin,
      mojom::WebFeature blocked_same_origin);

  void FinishedLoading(FrameLoader::NavigationFinishState);

  void UpdateFaviconURL();

  using IsCapturingMediaCallback = base::RepeatingCallback<bool()>;
  void SetIsCapturingMediaCallback(IsCapturingMediaCallback callback);
  bool IsCapturingMedia() const;

  void DidChangeVisibleToHitTesting() override;

  WebPrescientNetworking* PrescientNetworking();
  void SetPrescientNetworkingForTesting(
      std::unique_ptr<WebPrescientNetworking> prescient_networking);

  void CopyImageAtViewportPoint(const gfx::Point& viewport_point);
  void MediaPlayerActionAtViewportPoint(
      const gfx::Point& viewport_position,
      const blink::mojom::blink::MediaPlayerActionType type,
      bool enable);

  // Handle the request as a download. If the request is for a blob: URL,
  // a BlobURLToken should be provided as |blob_url_token| to ensure the
  // correct blob gets downloaded.
  void DownloadURL(
      const ResourceRequest& request,
      network::mojom::blink::RedirectMode cross_origin_redirect_behavior);
  void DownloadURL(
      const ResourceRequest& request,
      network::mojom::blink::RedirectMode cross_origin_redirect_behavior,
      mojo::PendingRemote<mojom::blink::BlobURLToken> blob_url_token);

  void NotifyUserActivation(
      mojom::blink::UserActivationNotificationType notification_type);
  void AddInspectorIssue(mojom::blink::InspectorIssueInfoPtr);
  void SaveImageAt(const gfx::Point& window_point);
  void AdvanceFocusForIME(mojom::blink::FocusType focus_type);
  void PostMessageEvent(
      const absl::optional<RemoteFrameToken>& source_frame_token,
      const String& source_origin,
      const String& target_origin,
      BlinkTransferableMessage message);

  void SetScaleFactor(float scale);
  void ClosePageForTesting();
  void SetInitialFocus(bool reverse);

#if BUILDFLAG(IS_MAC)
  void GetCharacterIndexAtPoint(const gfx::Point& point);
#endif

#if !BUILDFLAG(IS_ANDROID)
  void UpdateWindowControlsOverlay(const gfx::Rect& bounding_rect_in_dips);
  void RegisterWindowControlsOverlayChangedDelegate(
      WindowControlsOverlayChangedDelegate*);
  // For PWAs with display_overrides, these getters are information about the
  // titlebar bounds sent over from the browser via UpdateWindowControlsOverlay
  // in LocalMainFrame that are needed to persist the lifetime of the frame.
  const gfx::Rect& GetWindowControlsOverlayRect() const {
    return window_controls_overlay_rect_;
  }
  bool IsWindowControlsOverlayVisible() const {
    return is_window_controls_overlay_visible_;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  SystemClipboard* GetSystemClipboard();

  // Indicate that this frame was attached as a MainFrame.
  void WasAttachedAsLocalMainFrame();

  // Return true if the frame is able to access an event with the given
  // attribution (i.e. the event is targeted for an origin that the frame may
  // access).
  bool CanAccessEvent(const WebInputEventAttribution&) const;

  // Return true if the frame has a transient affordance to enter fullscreen.
  bool IsTransientAllowFullscreenActive() const;

  // Activate the transient affordance to enter fullscreen.
  void ActivateTransientAllowFullscreen() {
    transient_allow_fullscreen_.Activate();
  }

  LocalFrameToken GetLocalFrameToken() const;

  LoaderFreezeMode GetLoaderFreezeMode();

  // Swaps `this` LocalFrame in to replace the current frame  (e.g. in the case
  // of subframes, `Owner()->frame()`, or in the case of the main frame,
  // `GetPage()->Frame()`). Must only be called on provisional frames.
  bool SwapIn();

  void LoadJavaScriptURL(const KURL& url);
  void RequestExecuteScript(int32_t world_id,
                            base::span<const WebScriptSource> sources,
                            mojom::blink::UserActivationOption,
                            mojom::blink::EvaluationTiming,
                            mojom::blink::LoadEventBlockingOption,
                            WebScriptExecutionCallback,
                            BackForwardCacheAware back_forward_cache_aware,
                            mojom::blink::WantResultOption,
                            mojom::blink::PromiseResultOption);

  void SetEvictCachedSessionStorageOnFreezeOrUnload();

  // Whether to maintain a trivial session history.
  //
  // One example is prerender.
  // Explainer:
  // https://github.com/jeremyroman/alternate-loading-modes/blob/main/browsing-context.md#session-history
  bool ShouldMaintainTrivialSessionHistory() const;

  TextFragmentHandler* GetTextFragmentHandler() const {
    return text_fragment_handler_;
  }

  void BindTextFragmentReceiver(
      mojo::PendingReceiver<mojom::blink::TextFragmentReceiver> receiver);

  void CreateTextFragmentHandler();

  // Invokes on first paint, this method could be invoked multiple times, refer
  // to FrameFirstPaint.
  void OnFirstPaint(bool text_painted, bool image_painted);

#if BUILDFLAG(IS_MAC)
  void ResetTextInputHostForTesting();
  void RebindTextInputHostForTesting();
#endif

  void WriteIntoTrace(perfetto::TracedValue ctx) const;

  bool AncestorOrSelfHasCSPEE() const { return ancestor_or_self_has_cspee_; }
  void SetAncestorOrSelfHasCSPEE(bool has_policy) {
    ancestor_or_self_has_cspee_ = has_policy;
  }

  void SetBackgroundColorPaintImageGeneratorForTesting(
      BackgroundColorPaintImageGenerator* generator);

  absl::optional<SkColor> GetFrameOverlayColorForTesting() const;

  // Returns a PendingRemote resolved via this frame's BrowserInterfaceBroker
  // for use when creating the PublicUrlManager instance in threaded worklets.
  // See `WorkletGlobalScope::TakeBlobUrlStorePendingRemote()` for more info.
  mojo::PendingRemote<mojom::blink::BlobURLStore>
  GetBlobUrlStorePendingRemote();

  void AddScrollSnapshotClient(ScrollSnapshotClient&);

  // Take a snapshot for relevant scrollers at the beginning of a frame update.
  // https://drafts.csswg.org/scroll-animations-1/#avoiding-cycles
  void UpdateScrollSnapshots();

  // All newly created ScrollSnapshotClients are considered "unvalidated". This
  // means that the internal state of the client is considered tentative, and
  // computing the actual state may require an additional style/layout pass.
  //
  // The lifecycle update will call this function after style and layout has
  // completed. The function will then go though all unvalidated clients,
  // and compare the current state snapshot to a fresh state snapshot. If they
  // are equal, then the tentative state turned out to be correct, and no
  // further action is needed. Otherwise, all effects targets associated with
  // the client are marked for recalc, which causes the style/layout phase to
  // run again.
  //
  // Returns true if all client states are correct, otherwise returns false.
  //
  // https://github.com/w3c/csswg-drafts/issues/5261
  bool ValidateScrollSnapshotClients();

  const HeapHashSet<WeakMember<ScrollSnapshotClient>>&
  GetUnvalidatedScrollSnapshotClientsForTesting() {
    return unvalidated_scroll_snapshot_clients_;
  }

  void ScheduleNextServiceForScrollSnapshotClients();

 private:
  friend class FrameNavigationDisabler;
  // LocalFrameMojoHandler is a part of LocalFrame.
  friend class LocalFrameMojoHandler;

  FRIEND_TEST_ALL_PREFIXES(LocalFrameTest, CharacterIndexAtPointWithPinchZoom);
  FRIEND_TEST_ALL_PREFIXES(WebFrameTest, SmartClipData);
  FRIEND_TEST_ALL_PREFIXES(WebFrameTest, SmartClipDataWithPinchZoom);
  FRIEND_TEST_ALL_PREFIXES(WebFrameTest,
                           SmartClipReturnsEmptyStringsWhenUserSelectIsNone);
  FRIEND_TEST_ALL_PREFIXES(WebFrameTest, SmartClipDoesNotCrashPositionReversed);

  // Frame protected overrides:
  bool DetachImpl(FrameDetachType) override;

  // Intentionally private to prevent redundant checks when the type is
  // already LocalFrame.
  bool IsLocalFrame() const override { return true; }
  bool IsRemoteFrame() const override { return false; }

  void EnableNavigation() { --navigation_disable_count_; }
  void DisableNavigation() { ++navigation_disable_count_; }

  // Internal implementation for starting and ending paint preview capture.
  // `capturing` is true when capture starts and false when it ends.
  void SetInvalidationForCapture(bool capturing);

  // Internal implementation for starting or ending printing.
  // |printing| is true when printing starts, false when printing ends.
  // |page_size|, |original_page_size|, and |maximum_shrink_ratio| are only
  // meaningful when we should use printing layout for this frame.
  void SetPrinting(bool printing,
                   const gfx::SizeF& page_size,
                   const gfx::SizeF& original_page_size,
                   float maximum_shrink_ratio);

  // FrameScheduler::Delegate overrides:
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  void UpdateTaskTime(base::TimeDelta time) override;
  void UpdateBackForwardCacheDisablingFeatures(
      uint64_t features_mask,
      const BFCacheBlockingFeatureAndLocations&
          non_sticky_features_and_js_locations,
      const BFCacheBlockingFeatureAndLocations&
          sticky_features_and_js_locations) override;
  const base::UnguessableToken& GetAgentClusterId() const override;

  // Activates the user activation states of this frame and all its ancestors.
  //
  // The |notification_type| parameter is used for histograms only.
  void NotifyUserActivation(
      mojom::blink::UserActivationNotificationType notification_type,
      bool need_browser_verification);

  // Consumes and returns the transient user activation state this frame, after
  // updating all other frames in the frame tree.
  bool ConsumeTransientUserActivation(UserActivationUpdateSource update_source);

  void SetFrameColorOverlay(SkColor color);

  void DidFreeze();
  void DidResume();
  void SetContextPaused(bool);

  HitTestResult HitTestResultForVisualViewportPos(
      const gfx::Point& pos_in_viewport);

  bool ShouldThrottleDownload();

  // Returns the `Frame` for which `provisional_frame_ == this`. May only be
  // called on a provisional frame.
  Frame* GetProvisionalOwnerFrame();

  void ExtractSmartClipDataInternal(const gfx::Rect& rect_in_viewport,
                                    String& clip_text,
                                    String& clip_html,
                                    gfx::Rect& clip_rect);

#if !BUILDFLAG(IS_ANDROID)
  void SetTitlebarAreaDocumentStyleEnvironmentVariables() const;
  void MaybeUpdateWindowControlsOverlayWithNewZoomLevel();
#endif

  std::unique_ptr<FrameScheduler> frame_scheduler_;

  // Holds all PauseSubresourceLoadingHandles allowing either |this| to delete
  // them explicitly or the pipe closing to delete them.
  //
  // LocalFrame can be reused by multiple ExecutionContext.
  HeapMojoUniqueReceiverSet<blink::mojom::blink::PauseSubresourceLoadingHandle>
      pause_handle_receivers_{nullptr};

  // Keeps track of all the registered VK observers.
  HeapHashSet<WeakMember<VirtualKeyboardOverlayChangedObserver>>
      virtual_keyboard_overlay_changed_observers_;

  mutable FrameLoader loader_;

  // Cleared by LocalFrame::detach(), so as to keep the observable lifespan
  // of LocalFrame::view().
  Member<LocalFrameView> view_;
  // Usually 0. Non-null if this is the top frame of PagePopup.
  Member<Element> page_popup_owner_;

  const Member<Editor> editor_;
  const Member<FrameSelection> selection_;
  const Member<EventHandler> event_handler_;
  const Member<FrameConsole> console_;

  int navigation_disable_count_;
  // TODO(dcheng): In theory, this could be replaced by checking the
  // FrameLoaderStateMachine if a real load has committed. Unfortunately, the
  // internal state tracked there is incorrect today. See
  // https://crbug.com/778318.
  unsigned should_send_resource_timing_info_to_parent_ : 1;
  unsigned in_view_source_mode_ : 1;
  // Whether this frame is frozen or not. This is a copy of Page::IsFrozen()
  // and is stored here to ensure that we do not dispatch onfreeze() twice
  // in a row and every onfreeze() has a single corresponding onresume().
  unsigned frozen_ : 1;
  // Whether this frame is paused or not. This is a copy of Page::IsPaused()
  // and is stored here to ensure that we do not call SetContextPaused() twice
  // in a row with the same argument.
  unsigned paused_ : 1;
  // Whether this frame is known to be completely occluded by other opaque
  // OS-level windows.
  unsigned hidden_ : 1;

  float page_zoom_factor_;
  float text_zoom_factor_;

  Member<CoreProbeSink> probe_sink_;
  scoped_refptr<InspectorTaskRunner> inspector_task_runner_;
  Member<PerformanceMonitor> performance_monitor_;
  Member<AdTracker> ad_tracker_;
  Member<IdlenessDetector> idleness_detector_;
  Member<AttributionSrcLoader> attribution_src_loader_;
  Member<InspectorIssueReporter> inspector_issue_reporter_;
  Member<InspectorTraceEvents> inspector_trace_events_;
  // SmoothScrollSequencer is only populated for local roots; all local frames
  // use the instance owned by their local root.
  Member<SmoothScrollSequencer> smooth_scroll_sequencer_;
  // Access content_capture_manager_ through GetOrResetContentCaptureManager()
  // because WebContentCaptureClient might already stop the capture.
  Member<ContentCaptureManager> content_capture_manager_;

  InterfaceRegistry* const interface_registry_;

  mojom::blink::ViewportIntersectionState intersection_state_;

  // Only set for outermost main frame.
  mojom::blink::BackForwardCacheNotRestoredReasonsPtr not_restored_reasons_;

  // Per-frame URLLoader factory.
  std::unique_ptr<WebURLLoaderFactory> url_loader_factory_;

  ClientHintsPreferences client_hints_preferences_;

  IsCapturingMediaCallback is_capturing_media_callback_;

  Member<FrameOverlay> frame_color_overlay_;

  absl::optional<base::UnguessableToken> embedding_token_;

  mojom::FrameLifecycleState lifecycle_state_;

  std::unique_ptr<WebPrescientNetworking> prescient_networking_;

  Member<LocalFrameMojoHandler> mojo_handler_;

  // Variable to control burst of download requests.
  int num_burst_download_requests_ = 0;
  base::TimeTicks burst_download_start_time_;

  // Access to the global sanitized system clipboard.
  Member<SystemClipboard> system_clipboard_;

  // Access to background-color paint image generator. Initialized per local
  // root and reused among sub frames.
  Member<BackgroundColorPaintImageGenerator>
      background_color_paint_image_generator_;

  // TODO(crbug.com/1264553) : use a map from property id to
  // NativePaintImageGenerator, then we could avoid needing to switch on the
  // property in compositor_animations.cc
  // Access to box shadow paint image
  // generator. Initialized per local root and reused among sub frames.
  Member<BoxShadowPaintImageGenerator> box_shadow_paint_image_generator_;

  // Access to clip-path paint image generator. Initialized per local root and
  // reused among sub frames.
  Member<ClipPathPaintImageGenerator> clip_path_paint_image_generator_;

  using SavedScrollOffsets = HeapHashMap<Member<Node>, ScrollOffset>;
  Member<SavedScrollOffsets> saved_scroll_offsets_;

  // Created lazily when needed, either via the browser's SharedHighlighting
  // binding to it or via a context menu with a selection being opened in a
  // frame.
  Member<TextFragmentHandler> text_fragment_handler_;

  // ScrollSnapshotClients owned by elements in this frame. The clients must
  // be registered at the actual elements as the references here are weak.
  HeapHashSet<WeakMember<ScrollSnapshotClient>> scroll_snapshot_clients_;

  HeapHashSet<WeakMember<ScrollSnapshotClient>>
      unvalidated_scroll_snapshot_clients_;

  // Manages a transient affordance for this frame to enter fullscreen.
  TransientAllowFullscreen transient_allow_fullscreen_;

#if !BUILDFLAG(IS_ANDROID)
  bool is_window_controls_overlay_visible_ = false;
  // |page_zoom_factor_| is asynchronously set sometimes (most prominently seen
  // on mac) in |LocalFrame| via |PropagatePageZoomToNewlyAttachedFrame| on
  // navigation. We need to store the window_controls_overlay_rect sent from the
  // browser in dips so we can convert the rect to blink space coordinates when
  // |page_zoom_factor_| gets updated this way.
  gfx::Rect window_controls_overlay_rect_in_dips_;
  gfx::Rect window_controls_overlay_rect_;
  WeakMember<WindowControlsOverlayChangedDelegate>
      window_controls_overlay_changed_delegate_;
#endif

  // The evidence for or against a frame being an ad frame. `absl::nullopt` if
  // not yet set or if the frame is a subfiltering root frame. (Only non-root
  // frames can be tagged as ad frames.) This is per-frame (as opposed to
  // per-document) as we want to decide whether a frame is an ad or not before
  // commit, while the document has not yet been created.
  //
  // This is constructed directly in the renderer in the case of an initial
  // synchronous commit and otherwise is signaled from the browser process at
  // ready-to-commit time.
  absl::optional<blink::FrameAdEvidence> ad_evidence_;

  // True if this frame is a frame that had a script tagged as an ad on the v8
  // stack at the time of creation. This is updated in `SetAdEvidence()`,
  // allowing the bit to be propagated when a frame navigates cross-origin.
  // Fenced frames do not set this bit for the initial empty document, see
  // SubresourceFilterAgent::Initialize.
  bool is_frame_created_by_ad_script_ = false;

  bool evict_cached_session_storage_on_freeze_or_unload_ = false;

  // Indicate if the current document's color scheme was notified.
  bool notified_color_scheme_ = false;

  // Stores whether this frame is affected by a CSPEE policy (from any ancestor
  // frame). Calculated browser-side and used to help determine if this frame
  // is allowed to load a new child opaque-ads fenced frame.
  bool ancestor_or_self_has_cspee_ = false;

  // Reduced accept language for top-level frame.
  AtomicString reduced_accept_language_;
};

inline FrameLoader& LocalFrame::Loader() const {
  return loader_;
}

inline LocalFrameView* LocalFrame::View() const {
  return view_.Get();
}

inline FrameSelection& LocalFrame::Selection() const {
  return *selection_;
}

inline Editor& LocalFrame::GetEditor() const {
  return *editor_;
}

inline FrameConsole& LocalFrame::Console() const {
  return *console_;
}

inline bool LocalFrame::InViewSourceMode() const {
  return in_view_source_mode_;
}

inline void LocalFrame::SetInViewSourceMode(bool mode) {
  in_view_source_mode_ = mode;
}

inline EventHandler& LocalFrame::GetEventHandler() const {
  DCHECK(event_handler_);
  return *event_handler_;
}

template <>
struct DowncastTraits<LocalFrame> {
  static bool AllowFrom(const Frame& frame) { return frame.IsLocalFrame(); }
};

DECLARE_WEAK_IDENTIFIER_MAP(LocalFrame);

class FrameNavigationDisabler {
  STACK_ALLOCATED();

 public:
  explicit FrameNavigationDisabler(LocalFrame&);
  FrameNavigationDisabler(const FrameNavigationDisabler&) = delete;
  FrameNavigationDisabler& operator=(const FrameNavigationDisabler&) = delete;
  ~FrameNavigationDisabler();

 private:
  LocalFrame* frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_H_
