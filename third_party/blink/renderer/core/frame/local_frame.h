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

#include "base/macros.h"
#include "base/time/default_tick_clock.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/reporting_observer.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/optimization_guide/optimization_guide.mojom-blink.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink-forward.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "third_party/blink/renderer/core/clipboard/raw_system_clipboard.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/weak_identifier_map.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_unique_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#if defined(OS_MAC)
#include "third_party/blink/public/mojom/input/text_input_host.mojom-blink.h"
#endif
#include "ui/gfx/range/range.h"
#include "ui/gfx/transform.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gfx {
class Point;
}

#if defined(OS_MAC)
namespace gfx {
class Range;
}
#endif

namespace blink {

class AdTracker;
class AssociatedInterfaceProvider;
class BrowserInterfaceBrokerProxy;
class Color;
class ContentCaptureManager;
class Document;
class Editor;
class Element;
class EventHandler;
class EventHandlerRegistry;
class FloatSize;
class FrameConsole;
class FrameOverlay;
// class FrameScheduler;
class FrameSelection;
class FrameWidget;
class InputMethodController;
class InspectorIssueReporter;
class InspectorTraceEvents;
class CoreProbeSink;
class IdlenessDetector;
class InspectorTaskRunner;
class InterfaceRegistry;
class IntSize;
class LayoutView;
class LocalDOMWindow;
class LocalWindowProxy;
class LocalFrameClient;
class Node;
class NodeTraversal;
class PerformanceMonitor;
class PluginData;
class ScriptController;
class SmoothScrollSequencer;
class SpellChecker;
class TextFragmentSelectorGenerator;
class TextSuggestionController;
class VirtualKeyboardOverlayChangedObserver;
class WebContentSettingsClient;
class WebInputEventAttribution;
class WebPluginContainerImpl;
class WebPrescientNetworking;
class WebURLLoaderFactory;

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<LocalFrame>;

// A LocalFrame is a frame hosted inside this process.
class CORE_EXPORT LocalFrame final
    : public Frame,
      public FrameScheduler::Delegate,
      public Supplementable<LocalFrame>,
      public mojom::blink::LocalFrame,
      public mojom::blink::LocalMainFrame,
      public mojom::blink::HighPriorityLocalFrame {
 public:
  // Returns the LocalFrame instance for the given |frame_token|.
  // TODO(crbug.com/1096617): Remove the UnguessableToken version of this.
  static LocalFrame* FromFrameToken(const base::UnguessableToken& frame_token);
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
      const base::UnguessableToken& frame_token,
      WindowAgentFactory* inheriting_agent_factory,
      InterfaceRegistry*,
      const base::TickClock* clock = base::DefaultTickClock::GetInstance());

  void Init(Frame* opener);
  void SetView(LocalFrameView*);
  void CreateView(const IntSize&, const Color&);

  // Frame overrides:
  ~LocalFrame() override;
  void Trace(Visitor*) const override;
  void Navigate(FrameLoadRequest&, WebFrameLoadType) override;
  bool ShouldClose() override;
  const SecurityContext* GetSecurityContext() const override;
  void PrintNavigationErrorMessage(const Frame&, const char* reason);
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
  bool BubbleLogicalScrollFromChildFrame(
      mojom::blink::ScrollDirection direction,
      ScrollGranularity granularity,
      Frame* child) override;
  void DidFocus() override;

  void DidChangeThemeColor();
  void DidChangeBackgroundColor(SkColor background_color);

  void DetachChildren();
  // After Document is attached, resets state related to document, and sets
  // context to the current document.
  void DidAttachDocument();

  void Reload(WebFrameLoadType);

  // Note: these two functions are not virtual but intentionally shadow the
  // corresponding method in the Frame base class to return the
  // LocalFrame-specific subclass.
  LocalWindowProxy* WindowProxy(DOMWrapperWorld&);
  LocalDOMWindow* DomWindow() const;
  void SetDOMWindow(LocalDOMWindow*);
  LocalFrameView* View() const override;
  Document* GetDocument() const;
  void SetPagePopupOwner(Element&);
  Element* PagePopupOwner() const { return page_popup_owner_.Get(); }

  // Root of the layout tree for the document contained in this frame.
  LayoutView* ContentLayoutObject() const;

  Editor& GetEditor() const;
  EventHandler& GetEventHandler() const;
  EventHandlerRegistry& GetEventHandlerRegistry() const;
  FrameLoader& Loader() const;
  FrameSelection& Selection() const;
  InputMethodController& GetInputMethodController() const;
  TextSuggestionController& GetTextSuggestionController() const;
  ScriptController& GetScriptController() const;
  SpellChecker& GetSpellChecker() const;
  FrameConsole& Console() const;

  // A local root is the root of a connected subtree that contains only
  // LocalFrames. The local root is responsible for coordinating input, layout,
  // et cetera for that subtree of frames.
  bool IsLocalRoot() const;
  LocalFrame& LocalFrameRoot() const;

  CoreProbeSink* GetProbeSink() { return probe_sink_.Get(); }
  scoped_refptr<InspectorTaskRunner> GetInspectorTaskRunner();

  // Returns ContentCaptureManager in LocalFrameRoot.
  ContentCaptureManager* GetContentCaptureManager();

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

  // =========================================================================
  // All public functions below this point are candidates to move out of
  // LocalFrame into another class.

  // See GraphicsLayerClient.h for accepted flags.
  String GetLayerTreeAsTextForTesting(unsigned flags = 0) const;

  // Begin printing with the given page size information.
  // The frame content will fit to the page size with specified shrink ratio.
  // If this frame doesn't need to fit into a page size, default values are
  // used.
  void StartPrinting(const FloatSize& page_size = FloatSize(),
                     const FloatSize& original_page_size = FloatSize(),
                     float maximum_shrink_ratio = 0);

  void EndPrinting();
  bool ShouldUsePrintingLayout() const;
  FloatSize ResizePageRectsKeepingRatio(const FloatSize& original_size,
                                        const FloatSize& expected_size) const;

  bool InViewSourceMode() const;
  void SetInViewSourceMode(bool = true);

  void SetPageZoomFactor(float);
  float PageZoomFactor() const { return page_zoom_factor_; }
  void SetTextZoomFactor(float);
  float TextZoomFactor() const { return text_zoom_factor_; }
  void SetPageAndTextZoomFactors(float page_zoom_factor,
                                 float text_zoom_factor);

  void DeviceScaleFactorChanged();
  double DevicePixelRatio() const;

  // Informs the local root's document and its local descendant subtree that a
  // media query value changed.
  void MediaQueryAffectingValueChangedForLocalSubtree(MediaValueChange);

  void WindowSegmentsChanged(const WebVector<WebRect>& window_segments);
  void UpdateCSSFoldEnvironmentVariables(
      const WebVector<WebRect>& window_segments);

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
  // https://chromium.googlesource.com/chromium/src/+/master/docs/mojo_ipc_conversion.md#Channel_Associated-Interfaces
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

  enum class LazyLoadImageSetting {
    kDisabled,
    kEnabledExplicit,
    kEnabledAutomatic
  };
  // Returns the enabled state of lazyloading of images.
  LazyLoadImageSetting GetLazyLoadImageSetting() const;

  // Returns true if parser-blocking script should be force-deferred to execute
  // after parsing completes for this frame.
  bool ShouldForceDeferScript() const;

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
  void SetViewportIntersectionFromParent(const ViewportIntersectionState&);

  IntSize GetMainFrameViewportSize() const override;
  IntPoint GetMainFrameScrollOffset() const override;

  void SetOpener(Frame* opener) override;

  // See viewport_intersection_state.h for more info on these methods.
  IntRect RemoteViewportIntersection() const {
    return intersection_state_.viewport_intersection;
  }
  IntRect RemoteMainFrameIntersection() const {
    return intersection_state_.main_frame_intersection;
  }
  gfx::Transform RemoteMainFrameTransform() const {
    return intersection_state_.main_frame_transform;
  }

  FrameOcclusionState GetOcclusionState() const;

  bool NeedsOcclusionTracking() const;

  // Replaces the initial empty document with a Document suitable for
  // |mime_type| and populated with the contents of |data|. Only intended for
  // use in internal-implementation LocalFrames that aren't in the frame tree.
  void ForceSynchronousDocumentInstall(const AtomicString& mime_type,
                                       scoped_refptr<SharedBuffer> data);

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

  // Called in the constructor if AdTracker heuristics have determined that this
  // frame is an ad; LocalFrames created on behalf of OOPIF aren't set until
  // just before commit (ReadyToCommitNavigation time) by the embedder.
  void SetIsAdSubframe(blink::mojom::AdFrameType ad_frame_type);

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

  // Overlays a color on top of this LocalFrameView if it is associated with
  // the main frame. Should not have multiple consumers.
  void SetMainFrameColorOverlay(SkColor color);

  // Overlays a color on top of this LocalFrameView if it is associated with
  // a subframe. Should not have multiple consumers.
  void SetSubframeColorOverlay(SkColor color);
  void UpdateFrameColorOverlayPrePaint();

  // For CompositeAfterPaint.
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
  // frames, would be blocked by the introduction of feature policy. This takes
  // two counters (which may be the same). It triggers |blockedCrossOrigin| if
  // the frame is cross-origin relative to the top-level document, and triggers
  // |blockedSameOrigin| if it is same-origin with the top level, but is
  // embedded in any way through a cross-origin frame. (A->B->A embedding)
  void CountUseIfFeatureWouldBeBlockedByFeaturePolicy(
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

  void CopyImageAtViewportPoint(const IntPoint& viewport_point);
  void MediaPlayerActionAtViewportPoint(
      const IntPoint& viewport_position,
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

  // blink::mojom::LocalFrame overrides:
  void GetTextSurroundingSelection(
      uint32_t max_length,
      GetTextSurroundingSelectionCallback callback) final;
  void SendInterventionReport(const String& id, const String& message) final;
  void SetFrameOwnerProperties(
      mojom::blink::FrameOwnerPropertiesPtr properties) final;
  void NotifyUserActivation() final;
  void NotifyVirtualKeyboardOverlayRect(const gfx::Rect& keyboard_rect) final;
  void AddMessageToConsole(mojom::blink::ConsoleMessageLevel level,
                           const WTF::String& message,
                           bool discard_duplicates) final;
  void AddInspectorIssue(mojom::blink::InspectorIssueInfoPtr) final;
  void StopLoading() final;
  void Collapse(bool collapsed) final;
  void EnableViewSourceMode() final;
  void Focus() final;
  void ClearFocusedElement() final;
  void GetResourceSnapshotForWebBundle(
      mojo::PendingReceiver<
          data_decoder::mojom::blink::ResourceSnapshotForWebBundle> receiver)
      final;
  void CopyImageAt(const gfx::Point& window_point) final;
  void SaveImageAt(const gfx::Point& window_point) final;
  void ReportBlinkFeatureUsage(const Vector<mojom::blink::WebFeature>&) final;
  void RenderFallbackContent() final;
  void BeforeUnload(bool is_reload, BeforeUnloadCallback callback) final;
  void DispatchBeforeUnload(bool is_reload,
                            BeforeUnloadCallback callback) final;
  void MediaPlayerActionAt(
      const gfx::Point& window_point,
      blink::mojom::blink::MediaPlayerActionPtr action) final;
  void AdvanceFocusInFrame(
      mojom::blink::FocusType focus_type,
      const base::Optional<base::UnguessableToken>& source_frame_token) final;
  void AdvanceFocusInForm(mojom::blink::FocusType focus_type) final;
  void ReportContentSecurityPolicyViolation(
      network::mojom::blink::CSPViolationPtr csp_violation) final;
  // Updates the snapshotted policy attributes (sandbox flags and feature policy
  // container policy) in the frame's FrameOwner. This is used when this frame's
  // parent is in another process and it dynamically updates this frame's
  // sandbox flags or container policy. The new policy won't take effect until
  // the next navigation.
  void DidUpdateFramePolicy(const FramePolicy& frame_policy) final;
  void OnScreensChange() final;
  void PostMessageEvent(
      const base::Optional<base::UnguessableToken>& source_frame_token,
      const String& source_origin,
      const String& target_origin,
      BlinkTransferableMessage message) final;
  void BindReportingObserver(
      mojo::PendingReceiver<mojom::blink::ReportingObserver> receiver) final;
  void UpdateOpener(
      const base::Optional<base::UnguessableToken>& opener_routing_id) final;
  void GetSavableResourceLinks(GetSavableResourceLinksCallback callback) final;

  // blink::mojom::LocalMainFrame overrides:
  void AnimateDoubleTapZoom(const gfx::Point& point,
                            const gfx::Rect& rect) override;
  void SetScaleFactor(float scale) override;
  void ClosePage(
      mojom::blink::LocalMainFrame::ClosePageCallback callback) override;
  // Performs the specified plugin action on the node at the given location.
  void PluginActionAt(const gfx::Point& location,
                      mojom::blink::PluginActionType action) override;
  void SetInitialFocus(bool reverse) override;
  void EnablePreferredSizeChangedMode() override;
  void ZoomToFindInPageRect(const gfx::Rect& rect_in_root_frame) override;
#if defined(OS_MAC)
  void GetCharacterIndexAtPoint(const gfx::Point& point) final;
  void GetFirstRectForRange(const gfx::Range& range) final;
  void GetStringForRange(const gfx::Range& range,
                         GetStringForRangeCallback callback) final;
#endif
  void InstallCoopAccessMonitor(
      network::mojom::blink::CoopAccessReportType report_type,
      const base::UnguessableToken& accessed_window,
      mojo::PendingRemote<
          network::mojom::blink::CrossOriginOpenerPolicyReporter> reporter)
      final;
  void OnPortalActivated(
      const PortalToken& portal_token,
      mojo::PendingAssociatedRemote<mojom::blink::Portal> portal,
      mojo::PendingAssociatedReceiver<mojom::blink::PortalClient> portal_client,
      BlinkTransferableMessage data,
      OnPortalActivatedCallback callback) final;
  void ForwardMessageFromHost(
      BlinkTransferableMessage message,
      const scoped_refptr<const SecurityOrigin>& source_origin) final;

  SystemClipboard* GetSystemClipboard();
  RawSystemClipboard* GetRawSystemClipboard();

  // Indicate that this frame was attached as a MainFrame.
  void WasAttachedAsLocalMainFrame();

  // Return true if the frame is able to access an event with the given
  // attribution (i.e. the event is targeted for an origin that the frame may
  // access).
  bool CanAccessEvent(const WebInputEventAttribution&) const;

  void SetOptimizationGuideHints(
      mojom::blink::BlinkOptimizationGuideHintsPtr hints);
  mojom::blink::BlinkOptimizationGuideHints* GetOptimizationGuideHints() {
    return optimization_guide_hints_.get();
  }

  LocalFrameToken GetLocalFrameToken() const {
    return LocalFrameToken(GetFrameToken());
  }

  TextFragmentSelectorGenerator* GetTextFragmentSelectorGenerator() const {
    return text_fragment_selector_generator_;
  }

 private:
  friend class FrameNavigationDisabler;
  FRIEND_TEST_ALL_PREFIXES(LocalFrameTest, CharacterIndexAtPointWithPinchZoom);

  // Frame protected overrides:
  void DetachImpl(FrameDetachType) override;

  // Intentionally private to prevent redundant checks when the type is
  // already LocalFrame.
  bool IsLocalFrame() const override { return true; }
  bool IsRemoteFrame() const override { return false; }

  void EnableNavigation() { --navigation_disable_count_; }
  void DisableNavigation() { ++navigation_disable_count_; }

  void SetIsAdSubframeIfNecessary();

  void PropagateInertToChildFrames();

  // Internal implementation for starting or ending printing.
  // |printing| is true when printing starts, false when printing ends.
  // |page_size|, |original_page_size|, and |maximum_shrink_ratio| are only
  // meaningful when we should use printing layout for this frame.
  void SetPrinting(bool printing,
                   const FloatSize& page_size,
                   const FloatSize& original_page_size,
                   float maximum_shrink_ratio);

  // FrameScheduler::Delegate overrides:
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  void UpdateTaskTime(base::TimeDelta time) override;
  void UpdateActiveSchedulerTrackedFeatures(uint64_t features_mask) override;
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

  void EvictFromBackForwardCache();

  HitTestResult HitTestResultForVisualViewportPos(
      const IntPoint& pos_in_viewport);

  bool ShouldThrottleDownload();

#if defined(OS_MAC)
  mojom::blink::TextInputHost& GetTextInputHost();
#endif

  static void BindToReceiver(
      blink::LocalFrame* frame,
      mojo::PendingAssociatedReceiver<mojom::blink::LocalFrame> receiver);
  static void BindToMainFrameReceiver(
      blink::LocalFrame* frame,
      mojo::PendingAssociatedReceiver<mojom::blink::LocalMainFrame> receiver);
  void BindToHighPriorityReceiver(
      mojo::PendingReceiver<mojom::blink::HighPriorityLocalFrame> receiver);

  void BindTextFragmentSelectorProducer(
      mojo::PendingReceiver<mojom::blink::TextFragmentSelectorProducer>
          receiver);

  std::unique_ptr<FrameScheduler> frame_scheduler_;

  // Holds all PauseSubresourceLoadingHandles allowing either |this| to delete
  // them explicitly or the pipe closing to delete them.
  //
  // LocalFrame can be reused by multiple ExecutionContext.
  HeapMojoUniqueReceiverSet<
      blink::mojom::blink::PauseSubresourceLoadingHandle,
      std::default_delete<blink::mojom::blink::PauseSubresourceLoadingHandle>,
      HeapMojoWrapperMode::kWithoutContextObserver>
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

  const Member<ScriptController> script_controller_;
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
  Member<InspectorIssueReporter> inspector_issue_reporter_;
  Member<InspectorTraceEvents> inspector_trace_events_;
  // SmoothScrollSequencer is only populated for local roots; all local frames
  // use the instance owned by their local root.
  Member<SmoothScrollSequencer> smooth_scroll_sequencer_;
  Member<ContentCaptureManager> content_capture_manager_;

  InterfaceRegistry* const interface_registry_;
  // This is declared mutable so that the service endpoint can be cached by
  // const methods.
  //
  // LocalFrame can be reused by multiple ExecutionContext.
  mutable HeapMojoRemote<mojom::blink::ReportingServiceProxy,
                         HeapMojoWrapperMode::kWithoutContextObserver>
      reporting_service_{nullptr};

#if defined(OS_MAC)
  // LocalFrame can be reused by multiple ExecutionContext.
  HeapMojoRemote<mojom::blink::TextInputHost,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      text_input_host_{nullptr};
#endif

  ViewportIntersectionState intersection_state_;

  // Per-frame URLLoader factory.
  std::unique_ptr<WebURLLoaderFactory> url_loader_factory_;

  ClientHintsPreferences client_hints_preferences_;

  // The value of |is_save_data_enabled_| is read once per frame from
  // NetworkStateNotifier, which is guarded by a mutex lock, and cached locally
  // here for performance.
  // TODO(sclittle): This field doesn't really belong here - we should find some
  // way to make the state of NetworkStateNotifier accessible without needing to
  // acquire a mutex, such as by adding thread-local objects to hold the network
  // state that get updated whenever the network state changes. That way, this
  // field would be no longer necessary.
  const bool is_save_data_enabled_;

  IsCapturingMediaCallback is_capturing_media_callback_;

  std::unique_ptr<FrameOverlay> frame_color_overlay_;

  base::Optional<base::UnguessableToken> embedding_token_;

  mojom::FrameLifecycleState lifecycle_state_;

  std::unique_ptr<WebPrescientNetworking> prescient_networking_;

  // LocalFrame can be reused by multiple ExecutionContext.
  HeapMojoAssociatedRemote<mojom::blink::LocalFrameHost,
                           HeapMojoWrapperMode::kWithoutContextObserver>
      local_frame_host_remote_{nullptr};
  // LocalFrame can be reused by multiple ExecutionContext.
  HeapMojoAssociatedRemote<mojom::blink::BackForwardCacheControllerHost,
                           HeapMojoWrapperMode::kWithoutContextObserver>
      back_forward_cache_controller_host_remote_{nullptr};
  // LocalFrame can be reused by multiple ExecutionContext.
  HeapMojoAssociatedReceiver<mojom::blink::LocalFrame,
                             LocalFrame,
                             HeapMojoWrapperMode::kWithoutContextObserver>
      receiver_{this, nullptr};
  // LocalFrame can be reused by multiple ExecutionContext.
  HeapMojoAssociatedReceiver<mojom::blink::LocalMainFrame,
                             LocalFrame,
                             HeapMojoWrapperMode::kWithoutContextObserver>
      main_frame_receiver_{this, nullptr};
  // LocalFrame can be reused by multiple ExecutionContext.
  HeapMojoReceiver<mojom::blink::HighPriorityLocalFrame,
                   LocalFrame,
                   HeapMojoWrapperMode::kWithoutContextObserver>
      high_priority_frame_receiver_{this, nullptr};

  // Variable to control burst of download requests.
  int num_burst_download_requests_ = 0;
  base::TimeTicks burst_download_start_time_;

  // Access to the global sanitized system clipboard.
  Member<SystemClipboard> system_clipboard_;
  // Access to the global raw/unsanitized system clipboard
  Member<RawSystemClipboard> raw_system_clipboard_;

  mojom::blink::BlinkOptimizationGuideHintsPtr optimization_guide_hints_;

  Member<TextFragmentSelectorGenerator> text_fragment_selector_generator_;
};

inline FrameLoader& LocalFrame::Loader() const {
  return loader_;
}

inline LocalFrameView* LocalFrame::View() const {
  return view_.Get();
}

inline ScriptController& LocalFrame::GetScriptController() const {
  return *script_controller_;
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
  ~FrameNavigationDisabler();

 private:
  LocalFrame* frame_;

  DISALLOW_COPY_AND_ASSIGN(FrameNavigationDisabler);
};

// A helper class for attributing cost inside a scope to a LocalFrame, with
// output written to the trace log. The class is irrelevant to the core logic
// of LocalFrame.  Sample usage:
//
// void foo(LocalFrame* frame)
// {
//     ScopedFrameBlamer frameBlamer(frame);
//     TRACE_EVENT0("blink", "foo");
//     // Do some real work...
// }
//
// In Trace Viewer, we can find the cost of slice |foo| attributed to |frame|.
// Design doc:
// https://docs.google.com/document/d/15BB-suCb9j-nFt55yCFJBJCGzLg2qUm3WaSOPb8APtI/edit?usp=sharing
//
// This class is used in performance-sensitive code (like V8 entry), so care
// should be taken to ensure that it has an efficient fast path (for the common
// case where we are not tracking this).
class ScopedFrameBlamer {
  STACK_ALLOCATED();

 public:
  explicit ScopedFrameBlamer(LocalFrame*);
  ~ScopedFrameBlamer() {
    if (UNLIKELY(frame_))
      LeaveContext();
  }

 private:
  void LeaveContext();

  LocalFrame* frame_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFrameBlamer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_H_
