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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_H_

#include "base/debug/stack_trace.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/user_activation_state.h"
#include "third_party/blink/public/common/frame/user_activation_update_source.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/frame/frame_lifecycle.h"
#include "third_party/blink/renderer/core/frame/frame_view.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ChromeClient;
class DOMWindow;
class DOMWrapperWorld;
class Document;
class FrameClient;
class FrameOwner;
class HTMLFrameOwnerElement;
class LayoutEmbeddedContent;
class LocalFrame;
class KURL;
class Page;
class SecurityContext;
class Settings;
class WindowProxy;
class WindowProxyManager;
enum class ReportOptions;
struct FrameLoadRequest;

enum class FrameDetachType { kRemove, kSwap };

// Status of user gesture.
enum class UserGestureStatus { kActive, kNone };


// Frame is the base class of LocalFrame and RemoteFrame and should only contain
// functionality shared between both. In particular, any method related to
// input, layout, or painting probably belongs on LocalFrame.
class CORE_EXPORT Frame : public GarbageCollectedFinalized<Frame> {
 public:
  virtual ~Frame();

  virtual void Trace(blink::Visitor*);

  virtual bool IsLocalFrame() const = 0;
  virtual bool IsRemoteFrame() const = 0;

  // Asynchronously schedules a task to begin a navigation: this roughly
  // corresponds to "queue a task to navigate the target browsing context to
  // resource" in https://whatwg.org/C/links.html#following-hyperlinks.
  //
  // Note that there's currently an exception for same-origin same-document
  // navigations: these are never scheduled and are always synchronously
  // processed.
  //
  // TODO(dcheng): Note that despite the comment, most navigations in the spec
  // are *not* currently queued. See https://github.com/whatwg/html/issues/3730.
  // How this discussion is resolved will affect how https://crbug.com/848171 is
  // eventually fixed.
  virtual void ScheduleNavigation(Document& origin_document,
                                  const KURL&,
                                  WebFrameLoadType,
                                  UserGestureStatus) = 0;
  // Synchronously begins a navigation.
  virtual void Navigate(const FrameLoadRequest&, WebFrameLoadType) = 0;

  void Detach(FrameDetachType);
  void DisconnectOwnerElement();
  virtual bool ShouldClose() = 0;
  virtual void DidFreeze() = 0;
  virtual void DidResume() = 0;

  FrameClient* Client() const;

  Page* GetPage() const;  // Null when the frame is detached.
  virtual FrameView* View() const = 0;

  // Before using this, make sure you really want the top-level frame in the
  // entire page, as opposed to a top-level local frame in a sub-tree, e.g.
  // one representing a cross-process iframe in a renderer separate from the
  // main frame's renderer. For layout and compositing code, often
  // LocalFrame::IsLocalRoot() is more appropriate. If you are unsure, please
  // reach out to site-isolation-dev@chromium.org.
  bool IsMainFrame() const;

  FrameOwner* Owner() const;
  void SetOwner(FrameOwner*);
  HTMLFrameOwnerElement* DeprecatedLocalOwner() const;

  DOMWindow* DomWindow() const { return dom_window_; }

  FrameTree& Tree() const;
  ChromeClient& GetChromeClient() const;

  virtual SecurityContext* GetSecurityContext() const = 0;

  Frame* FindUnsafeParentScrollPropagationBoundary();

  // This prepares the Frame for the next commit. It will detach children,
  // dispatch unload events, abort XHR requests and detach the document.
  // Returns true if the frame is ready to receive the next commit, or false
  // otherwise.
  virtual bool PrepareForCommit() = 0;

  // LayoutObject for the element that contains this frame.
  LayoutEmbeddedContent* OwnerLayoutObject() const;

  Settings* GetSettings() const;  // can be null

  // isLoading() is true when the embedder should think a load is in progress.
  // In the case of LocalFrames, it means that the frame has sent a
  // didStartLoading() callback, but not the matching didStopLoading(). Inside
  // blink, you probably want Document::loadEventFinished() instead.
  void SetIsLoading(bool is_loading) { is_loading_ = is_loading; }
  bool IsLoading() const { return is_loading_; }

  // Tells the frame to check whether its load has completed, based on the state
  // of its subframes, etc.
  virtual void CheckCompleted() = 0;

  WindowProxyManager* GetWindowProxyManager() const {
    return window_proxy_manager_;
  }
  WindowProxy* GetWindowProxy(DOMWrapperWorld&);

  virtual void DidChangeVisibilityState();

  // This should never be called from outside Frame or WebFrame.
  void NotifyUserActivationInLocalTree();

  // This should never be called from outside Frame or WebFrame.
  bool ConsumeTransientUserActivationInLocalTree();

  bool HasBeenActivated() const {
    return user_activation_state_.HasBeenActive();
  }

  void ClearActivation() { user_activation_state_.Clear(); }

  void SetDocumentHasReceivedUserGestureBeforeNavigation(bool value) {
    has_received_user_gesture_before_nav_ = value;
  }

  bool HasReceivedUserGestureBeforeNavigation() const {
    return has_received_user_gesture_before_nav_;
  }

  bool IsAttached() const {
    return lifecycle_.GetState() == FrameLifecycle::kAttached;
  }

  // Tests whether the policy-controlled feature is enabled in this frame.
  // Optionally sends a report to any registered reporting observers or
  // Report-To endpoints, via ReportFeaturePolicyViolation(), if the feature is
  // disabled.
  // TODO(iclelland): Replace these with methods on SecurityContext/Document
  bool DeprecatedIsFeatureEnabled(mojom::FeaturePolicyFeature) const;
  bool DeprecatedIsFeatureEnabled(mojom::FeaturePolicyFeature,
                                  ReportOptions report_on_failure) const;
  virtual void DeprecatedReportFeaturePolicyViolation(
      mojom::FeaturePolicyFeature) const {}

  // Called to make a frame inert or non-inert. A frame is inert when there
  // is a modal dialog displayed within an ancestor frame, and this frame
  // itself is not within the dialog.
  virtual void SetIsInert(bool) = 0;
  void UpdateInertIfPossible();

  virtual void SetInheritedEffectiveTouchAction(TouchAction) = 0;
  void UpdateInheritedEffectiveTouchActionIfPossible();
  TouchAction InheritedEffectiveTouchAction() const {
    return inherited_effective_touch_action_;
  }

  // Continues to bubble logical scroll from |child| in this frame.
  // Returns true if the scroll was consumed locally.
  virtual bool BubbleLogicalScrollFromChildFrame(ScrollDirection direction,
                                                 ScrollGranularity granularity,
                                                 Frame* child) = 0;

  const base::UnguessableToken& GetDevToolsFrameToken() const {
    return devtools_frame_token_;
  }
  const CString& ToTraceValue();

  // TODO(dcheng): temporary for debugging https://crbug.com/838348.
  const base::debug::StackTrace& CreateStackForDebugging() {
    return create_stack_;
  }
  const base::debug::StackTrace& DetachStackForDebugging() {
    return detach_stack_;
  }

 protected:
  Frame(FrameClient*, Page&, FrameOwner*, WindowProxyManager*);

  // DetachImpl() may be re-entered multiple times, if a frame is detached while
  // already being detached.
  virtual void DetachImpl(FrameDetachType) = 0;

  // Note that IsAttached() and IsDetached() are not strict opposites: frames
  // that are detaching are considered to be in neither state.
  bool IsDetached() const {
    return lifecycle_.GetState() == FrameLifecycle::kDetached;
  }

  mutable FrameTree tree_node_;

  Member<Page> page_;
  Member<FrameOwner> owner_;
  Member<DOMWindow> dom_window_;

  // The user activation state of the current frame.  See
  // FrameTreeNode::user_activation_state_ for details.
  UserActivationState user_activation_state_;

  bool has_received_user_gesture_before_nav_ = false;

  // This is set to true if this is a subframe, and the frame element in the
  // parent frame's document becomes inert. This should always be false for
  // the main frame.
  bool is_inert_ = false;

  TouchAction inherited_effective_touch_action_ = TouchAction::kTouchActionAuto;

 private:
  Member<FrameClient> client_;
  const Member<WindowProxyManager> window_proxy_manager_;
  FrameLifecycle lifecycle_;
  // TODO(sashab): Investigate if this can be represented with m_lifecycle.
  bool is_loading_;
  base::UnguessableToken devtools_frame_token_;
  base::Optional<CString> trace_value_;

  base::debug::StackTrace create_stack_;
  base::debug::StackTrace detach_stack_;
};

inline FrameClient* Frame::Client() const {
  return client_;
}

inline FrameOwner* Frame::Owner() const {
  return owner_;
}

inline FrameTree& Frame::Tree() const {
  return tree_node_;
}

// Allow equality comparisons of Frames by reference or pointer,
// interchangeably.
DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(Frame)

// This method should be used instead of Frame* pointer
// in a TRACE_EVENT_XXX macro. Example:
//
// TRACE_EVENT1("category", "event_name", "frame", ToTraceValue(GetFrame()));
static inline CString ToTraceValue(Frame* frame) {
  return frame ? frame->ToTraceValue() : CString();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_H_
