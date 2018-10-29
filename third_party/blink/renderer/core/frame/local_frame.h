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
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom-blink.h"
#include "third_party/blink/public/platform/reporting.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/accessibility/axid.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/dom/weak_identifier_map.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/frame_types.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instance_counters.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace service_manager {
class InterfaceProvider;
}

namespace blink {

class AdTracker;
class AssociatedInterfaceProvider;
class Color;
class ComputedAccessibleNode;
class ContentSecurityPolicy;
class Document;
class Editor;
class Element;
class EventHandler;
class EventHandlerRegistry;
class FloatSize;
class FrameConsole;
class FrameResourceCoordinator;
// class FrameScheduler;
class FrameSelection;
class InputMethodController;
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
class NavigationScheduler;
class Node;
class NodeTraversal;
class PerformanceMonitor;
class PluginData;
class ResourceRequest;
class ScriptController;
class SharedBuffer;
class SmoothScrollSequencer;
class SpellChecker;
class TextSuggestionController;
class WebComputedAXTree;
class WebContentSettingsClient;
class WebPluginContainerImpl;
class WebURLLoaderFactory;

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<LocalFrame>;

class CORE_EXPORT LocalFrame final : public Frame,
                                     public FrameScheduler::Delegate,
                                     public Supplementable<LocalFrame> {
  USING_GARBAGE_COLLECTED_MIXIN(LocalFrame);

 public:
  static LocalFrame* Create(LocalFrameClient*,
                            Page&,
                            FrameOwner*,
                            InterfaceRegistry* = nullptr);

  void Init();
  void SetView(LocalFrameView*);
  void CreateView(const IntSize&, const Color&);

  // Frame overrides:
  ~LocalFrame() override;
  void Trace(blink::Visitor*) override;
  void ScheduleNavigation(Document& origin_document,
                          const KURL&,
                          WebFrameLoadType,
                          UserGestureStatus) override;
  void Navigate(const FrameLoadRequest&, WebFrameLoadType) override;
  bool ShouldClose() override;
  SecurityContext* GetSecurityContext() const override;
  void PrintNavigationErrorMessage(const Frame&, const char* reason);
  void PrintNavigationWarning(const String&);
  bool PrepareForCommit() override;
  void CheckCompleted() override;
  void DidChangeVisibilityState() override;
  void DidFreeze() override;
  void DidResume() override;
  // This sets the is_inert_ flag and also recurses through this frame's
  // subtree, updating the inert bit on all descendant frames.
  void SetIsInert(bool) override;
  void SetInheritedEffectiveTouchAction(TouchAction) override;
  bool BubbleLogicalScrollFromChildFrame(ScrollDirection direction,
                                         ScrollGranularity granularity,
                                         Frame* child) override;

  void DetachChildren();
  void DocumentAttached();

  Frame* FindFrameForNavigation(const AtomicString& name,
                                LocalFrame& active_frame,
                                const KURL& destination_url);
  void Reload(WebFrameLoadType, ClientRedirectPolicy);

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
  NavigationScheduler& GetNavigationScheduler() const;
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

  // Note that the result of this function should not be cached: a frame is
  // not necessarily detached when it is navigated, so the return value can
  // change.
  // In addition, this function will always return true for a detached frame.
  // TODO(dcheng): Move this to LocalDOMWindow and figure out the right
  // behavior for detached windows.
  bool IsCrossOriginSubframe() const;

  CoreProbeSink* GetProbeSink() { return probe_sink_.Get(); }
  scoped_refptr<InspectorTaskRunner> GetInspectorTaskRunner();

  // Activates the user activation states of the |LocalFrame| (provided it's
  // non-null) and all its ancestors.  Also creates a |UserGestureIndicator|
  // that contains a |UserGestureToken| with the given status.
  static std::unique_ptr<UserGestureIndicator> NotifyUserActivation(
      LocalFrame*,
      UserGestureToken::Status = UserGestureToken::kPossiblyExistingGesture);

  // Similar to above, but used only in old UAv1-specific code.
  static std::unique_ptr<UserGestureIndicator> NotifyUserActivation(
      LocalFrame*,
      UserGestureToken*);

  // Returns the transient user activation state of the |LocalFrame|, provided
  // it is non-null.  Otherwise returns |false|.
  //
  // The |check_if_main_thread| parameter determines if the token based gestures
  // (legacy UAv1 code) must be used in a thread-safe manner.
  static bool HasTransientUserActivation(LocalFrame*,
                                         bool check_if_main_thread = false);

  // Consumes the transient user activation state of the |LocalFrame|, provided
  // the frame pointer is non-null and the state hasn't been consumed since
  // activation.  Returns |true| if successfully consumed the state.
  //
  // The |check_if_main_thread| parameter determines if the token based gestures
  // (legacy code) must be used in a thread-safe manner.
  static bool ConsumeTransientUserActivation(
      LocalFrame*,
      bool check_if_main_thread = false,
      UserActivationUpdateSource update_source =
          UserActivationUpdateSource::kRenderer);

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

  String SelectedText() const;
  String SelectedTextForClipboard() const;

  PositionWithAffinityTemplate<EditingAlgorithm<NodeTraversal>>
  PositionForPoint(const LayoutPoint& frame_point);
  Document* DocumentAtPoint(const LayoutPoint&);

  bool ShouldReuseDefaultView(const KURL&, const ContentSecurityPolicy*) const;
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

  service_manager::InterfaceProvider& GetInterfaceProvider();
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
  // https://chromium.googlesource.com/chromium/src/+/master/ipc#Using-Channel_associated-Interfaces.
  AssociatedInterfaceProvider* GetRemoteNavigationAssociatedInterfaces();

  LocalFrameClient* Client() const;

  WebContentSettingsClient* GetContentSettingsClient();

  // GetFrameResourceCoordinator may return nullptr when it can not hook up to
  // services/resource_coordinator.
  FrameResourceCoordinator* GetFrameResourceCoordinator();

  PluginData* GetPluginData() const;

  PerformanceMonitor* GetPerformanceMonitor() { return performance_monitor_; }
  IdlenessDetector* GetIdlenessDetector() { return idleness_detector_; }
  AdTracker* GetAdTracker() { return ad_tracker_; }
  void SetAdTrackerForTesting(AdTracker* ad_tracker);

  // Returns true if Client Lo-Fi should be used for this request.
  bool IsClientLoFiAllowed(const ResourceRequest&) const;

  // Returns true if lazyloading the image is possible.
  bool IsLazyLoadingImageAllowed() const;

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
  void SetViewportIntersectionFromParent(const IntRect&, bool);
  IntRect RemoteViewportIntersection() { return remote_viewport_intersection_; }
  bool MayBeOccludedOrObscuredByRemoteAncestor() const {
    return occluded_or_obscured_by_ancestor_;
  }

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

  // TODO(https://crbug.com/578349): provisional frames are a hack that should
  // be removed.
  bool IsProvisional() const;

  // Returns whether the frame is trying to save network data by showing a
  // preview.
  bool IsUsingDataSavingPreview() const;

  ComputedAccessibleNode* GetOrCreateComputedAccessibleNode(AXID,
                                                            WebComputedAXTree*);

  // True if AdTracker heuristics have determined that this frame is an ad.
  // Calculated in the constructor but LocalFrames created on behalf of OOPIF
  // aren't set until just before commit (ReadyToCommitNavigation time) by the
  // embedder.
  bool IsAdSubframe() const { return is_ad_subframe_; }
  void SetIsAdSubframe() {
    DCHECK(!IsMainFrame());
    if (is_ad_subframe_)
      return;
    is_ad_subframe_ = true;
    frame_scheduler_->SetIsAdFrame();
    InstanceCounters::IncrementCounter(InstanceCounters::kAdSubframeCounter);
  }

  // Binds |request| and prevents resource loading until either the frame is
  // navigated or the request pipe is closed.
  void PauseSubresourceLoading(
      blink::mojom::blink::PauseSubresourceLoadingHandleRequest request);

  void ResumeSubresourceLoading();

  void AnimateSnapFling(base::TimeTicks monotonic_time);

  ClientHintsPreferences& GetClientHintsPreferences() {
    return client_hints_preferences_;
  }

  void BindPreviewsResourceLoadingHintsRequest(
      blink::mojom::blink::PreviewsResourceLoadingHintsReceiverRequest request);

  SmoothScrollSequencer& GetSmoothScrollSequencer();

  // TODO(iclelland): Replace this with a method on Document
  void DeprecatedReportFeaturePolicyViolation(
      mojom::FeaturePolicyFeature) const override;

  const mojom::blink::ReportingServiceProxyPtr& GetReportingService() const;

 private:
  friend class FrameNavigationDisabler;

  LocalFrame(LocalFrameClient*, Page&, FrameOwner*, InterfaceRegistry*);

  // Frame protected overrides:
  void DetachImpl(FrameDetachType) override;

  // Intentionally private to prevent redundant checks when the type is
  // already LocalFrame.
  bool IsLocalFrame() const override { return true; }
  bool IsRemoteFrame() const override { return false; }

  void EnableNavigation() { --navigation_disable_count_; }
  void DisableNavigation() { ++navigation_disable_count_; }

  bool CanNavigateWithoutFramebusting(const Frame&, String& error_reason);

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

  // Activates the user activation states of this frame and all its ancestors.
  void NotifyUserActivation();

  // Returns the transient user activation state of this frame
  bool HasTransientUserActivation();

  // Consumes and returns the transient user activation state of this frame,
  // after updating all ancestor/descendant frames.
  bool ConsumeTransientUserActivation(UserActivationUpdateSource update_source);

  std::unique_ptr<FrameScheduler> frame_scheduler_;

  // Holds all PauseSubresourceLoadingHandles allowing either |this| to delete
  // them explicitly or the pipe closing to delete them.
  mojo::StrongBindingSet<blink::mojom::blink::PauseSubresourceLoadingHandle>
      pause_handle_bindings_;

  mutable FrameLoader loader_;
  Member<NavigationScheduler> navigation_scheduler_;

  // Cleared by LocalFrame::detach(), so as to keep the observable lifespan
  // of LocalFrame::view().
  Member<LocalFrameView> view_;
  // Usually 0. Non-null if this is the top frame of PagePopup.
  Member<Element> page_popup_owner_;

  const Member<ScriptController> script_controller_;
  const Member<Editor> editor_;
  const Member<SpellChecker> spell_checker_;
  const Member<FrameSelection> selection_;
  const Member<EventHandler> event_handler_;
  const Member<FrameConsole> console_;
  const Member<InputMethodController> input_method_controller_;
  const Member<TextSuggestionController> text_suggestion_controller_;

  int navigation_disable_count_;
  // TODO(dcheng): In theory, this could be replaced by checking the
  // FrameLoaderStateMachine if a real load has committed. Unfortunately, the
  // internal state tracked there is incorrect today. See
  // https://crbug.com/778318.
  bool should_send_resource_timing_info_to_parent_ = true;

  float page_zoom_factor_;
  float text_zoom_factor_;

  bool in_view_source_mode_;

  // True if this frame is heuristically determined to have been created for
  // advertising purposes. It's per-frame (as opposed to per-document) because
  // when an iframe is created on behalf of ad script that same frame is not
  // typically reused for non-ad purposes.
  bool is_ad_subframe_ = false;

  Member<CoreProbeSink> probe_sink_;
  scoped_refptr<InspectorTaskRunner> inspector_task_runner_;
  Member<PerformanceMonitor> performance_monitor_;
  Member<AdTracker> ad_tracker_;
  Member<IdlenessDetector> idleness_detector_;
  Member<InspectorTraceEvents> inspector_trace_events_;
  // SmoothScrollSequencer is only populated for local roots; all local frames
  // use the instance owned by their local root.
  Member<SmoothScrollSequencer> smooth_scroll_sequencer_;

  InterfaceRegistry* const interface_registry_;
  // This is declared mutable so that the service endpoint can be cached by
  // const methods.
  mutable mojom::blink::ReportingServiceProxyPtr reporting_service_;

  IntRect remote_viewport_intersection_;
  bool occluded_or_obscured_by_ancestor_ = false;
  std::unique_ptr<FrameResourceCoordinator> frame_resource_coordinator_;

  // Used to keep track of which ComputedAccessibleNodes have already been
  // instantiated in this frame to avoid constructing duplicates.
  HeapHashMap<AXID, Member<ComputedAccessibleNode>> computed_node_mapping_;

  // Per-frame URLLoader factory.
  std::unique_ptr<WebURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<mojom::blink::PreviewsResourceLoadingHintsReceiver>
      previews_resource_loading_hints_receiver_;

  ClientHintsPreferences client_hints_preferences_;
};

inline FrameLoader& LocalFrame::Loader() const {
  return loader_;
}

inline NavigationScheduler& LocalFrame::GetNavigationScheduler() const {
  DCHECK(navigation_scheduler_);
  return *navigation_scheduler_.Get();
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

inline SpellChecker& LocalFrame::GetSpellChecker() const {
  return *spell_checker_;
}

inline FrameConsole& LocalFrame::Console() const {
  return *console_;
}

inline InputMethodController& LocalFrame::GetInputMethodController() const {
  return *input_method_controller_;
}

inline TextSuggestionController& LocalFrame::GetTextSuggestionController()
    const {
  return *text_suggestion_controller_;
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

DEFINE_TYPE_CASTS(LocalFrame,
                  Frame,
                  localFrame,
                  localFrame->IsLocalFrame(),
                  localFrame.IsLocalFrame());

DECLARE_WEAK_IDENTIFIER_MAP(LocalFrame);

class FrameNavigationDisabler {
  STACK_ALLOCATED();

 public:
  explicit FrameNavigationDisabler(LocalFrame&);
  ~FrameNavigationDisabler();

 private:
  Member<LocalFrame> frame_;

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

  Member<LocalFrame> frame_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFrameBlamer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_H_
