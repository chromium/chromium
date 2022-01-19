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

#include "base/i18n/rtl.h"
#include "base/unguessable_token.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/frame/frame_ad_evidence.h"
#include "third_party/blink/public/common/frame/user_activation_state.h"
#include "third_party/blink/public/common/frame/user_activation_update_source.h"
#include "third_party/blink/public/common/permissions_policy/document_policy_features.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/input/scroll_direction.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/frame_lifecycle.h"
#include "third_party/blink/renderer/core/frame/frame_view.h"
#include "third_party/blink/renderer/core/frame/navigation_rate_limiter.h"
#include "third_party/blink/renderer/core/frame/opened_frame_tracker.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace ui {
enum class ScrollGranularity : uint8_t;
}  // namespace ui

namespace blink {

class ChromeClient;
class DOMWindow;
class DOMWrapperWorld;
class Document;
class FrameClient;
class FrameOwner;
class FrameScheduler;
class FormSubmission;
class HTMLFrameOwnerElement;
class LayoutEmbeddedContent;
class LocalFrame;
class Page;
class SecurityContext;
class Settings;
class WindowProxy;
class WindowProxyManager;
struct FrameLoadRequest;
class WindowAgentFactory;
class WebFrame;

enum class FrameDetachType { kRemove, kSwap };

// kInsertLater will create a provisional frame, i.e. it will have a parent
// frame but not be inserted into the frame tree.
enum class FrameInsertType { kInsertInConstructor, kInsertLater };

// Frame is the base class of LocalFrame and RemoteFrame and should only contain
// functionality shared between both. In particular, any method related to
// input, layout, or painting probably belongs on LocalFrame.
class CORE_EXPORT Frame : public GarbageCollected<Frame> {
 public:
  // Returns the Frame instance for the given |frame_token|.
  // Note that this Frame can be either a LocalFrame or Remote instance.
  static Frame* ResolveFrame(const FrameToken& frame_token);

  virtual ~Frame();

  virtual void Trace(Visitor*) const;

  virtual bool IsLocalFrame() const = 0;
  virtual bool IsRemoteFrame() const = 0;

  virtual void Navigate(FrameLoadRequest&, WebFrameLoadType) = 0;

  // Releases the resources associated with a frame. Used for:
  // - closing a `WebView`, which detaches the main frame
  // - removing a `FrameOwner` from the DOM, which detaches the `FrameOwner`'s
  //   content frame
  // - preparing a frame to be replaced in `Frame::Swap()`.
  //
  // Since `Detach()` fires JS events and detaches all child frames, and JS can
  // modify the DOM in ways that trigger frame removal, it is possible to
  // reentrantly call `Detach() with `FrameDetachType::kRemove` before the
  // original invocation of `Detach()` has completed. In that case, the
  // interrupted invocation returns false to signal the interruption; otherwise,
  // on successful completion (e.g. `Detach()` runs all the way through to the
  // end), returns true.
  bool Detach(FrameDetachType);
  void DisconnectOwnerElement();
  virtual bool ShouldClose() = 0;
  virtual void HookBackForwardCacheEviction() = 0;
  virtual void RemoveBackForwardCacheEviction() = 0;

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

  // Returns true if and only if:
  // - this frame is a subframe
  // - it is cross-origin to the main frame
  //
  // Important notes:
  // - This function is not appropriate for determining if a subframe is
  //   cross-origin to its parent (see: |IsCrossOriginToParentFrame|).
  // - The return value must NOT be cached. A frame can be reused across
  //   navigations, so the return value can change over time.
  // - The return value is inaccurate for a detached frame: it always
  //   returns true when the frame is detached.
  // TODO(dcheng): Move this to LocalDOMWindow and figure out the right
  // behavior for detached windows.
  bool IsCrossOriginToMainFrame() const;
  // Returns true if this frame is a subframe and is cross-origin to the parent
  // frame. See |IsCrossOriginToMainFrame| for important notes.
  bool IsCrossOriginToParentFrame() const;

  FrameOwner* Owner() const;
  void SetOwner(FrameOwner*);
  HTMLFrameOwnerElement* DeprecatedLocalOwner() const;

  DOMWindow* DomWindow() const { return dom_window_; }

  FrameTree& Tree() const;
  ChromeClient& GetChromeClient() const;

  virtual const SecurityContext* GetSecurityContext() const = 0;

  Frame* FindUnsafeParentScrollPropagationBoundary();

  // Similar to `Detach()`, except that it does not completely detach `this`:
  // instead, on successful completion (i.e. returns true), `this` will be ready
  // to be swapped out (if necessary) and to commit the next navigation.
  //
  // Note that the caveats about `Detach()` being interrupted by reentrant
  // removal also apply to this method; this method also returns false if
  // interrupted by reentrant removal of `this`. A return value of false
  // indicates that the caller should early return and skip any further work, as
  // there is no longer a frame to commit a navigation into.
  virtual bool DetachDocument() = 0;

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

  // Returns the transient user activation state of this frame.
  bool HasTransientUserActivation() const {
    return user_activation_state_.IsActive();
  }

  // Returns the sticky user activation state of this frame.
  bool HasStickyUserActivation() const {
    return user_activation_state_.HasBeenActive();
  }

  // Returns if the last user activation for this frame was restricted in
  // nature.
  bool LastActivationWasRestricted() const {
    return user_activation_state_.LastActivationWasRestricted();
  }

  // Resets the user activation state of this frame.
  void ClearUserActivation() { user_activation_state_.Clear(); }

  void SetHadStickyUserActivationBeforeNavigation(bool value) {
    had_sticky_user_activation_before_nav_ = value;
  }

  bool HadStickyUserActivationBeforeNavigation() const {
    return had_sticky_user_activation_before_nav_;
  }

  bool IsAttached() const {
    return lifecycle_.GetState() == FrameLifecycle::kAttached;
  }

  // Note that IsAttached() and IsDetached() are not strict opposites: frames
  // that are detaching are considered to be in neither state.
  bool IsDetached() const {
    return lifecycle_.GetState() == FrameLifecycle::kDetached;
  }

  // Whether the frame is considered to be an ad subframe by Ad Tagging. Returns
  // true for both root and child ad subframes.
  virtual bool IsAdSubframe() const = 0;

  // Called to make a frame inert or non-inert. A frame is inert when there
  // is a modal dialog displayed within an ancestor frame, and this frame
  // itself is not within the dialog.
  virtual void SetIsInert(bool) = 0;
  void UpdateInertIfPossible();

  // Changes the text direction of the selected input node.
  virtual void SetTextDirection(base::i18n::TextDirection) = 0;

  virtual void SetInheritedEffectiveTouchAction(TouchAction) = 0;
  void UpdateInheritedEffectiveTouchActionIfPossible();
  TouchAction InheritedEffectiveTouchAction() const {
    return inherited_effective_touch_action_;
  }

  // Continues to bubble logical scroll from |child| in this frame.
  // Returns true if the scroll was consumed locally.
  virtual bool BubbleLogicalScrollFromChildFrame(
      mojom::blink::ScrollDirection direction,
      ui::ScrollGranularity granularity,
      Frame* child) = 0;

  const base::UnguessableToken& GetDevToolsFrameToken() const {
    return devtools_frame_token_;
  }
  const std::string& ToTraceValue();

  void SetEmbeddingToken(const base::UnguessableToken& embedding_token);
  const absl::optional<base::UnguessableToken>& GetEmbeddingToken() const {
    return embedding_token_;
  }

  NavigationRateLimiter& navigation_rate_limiter() {
    return navigation_rate_limiter_;
  }

  // Called to get the opener's sandbox flags if any. This works with disowned
  // openers, i.e., even if WebFrame::Opener() is nullptr,
  network::mojom::blink::WebSandboxFlags OpenerSandboxFlags() const {
    return opener_sandbox_flags_;
  }

  // Sets the opener's sandbox_flags for the main frame. Once a non-empty
  // |opener_feature_state| is set, it can no longer be modified (due to the
  // fact that the original opener which passed down the FeatureState cannot be
  // modified either).
  void SetOpenerSandboxFlags(network::mojom::blink::WebSandboxFlags flags) {
    DCHECK(IsMainFrame());
    DCHECK_EQ(network::mojom::blink::WebSandboxFlags::kNone,
              opener_sandbox_flags_);
    opener_sandbox_flags_ = flags;
  }

  const DocumentPolicyFeatureState& GetRequiredDocumentPolicy() const {
    return required_document_policy_;
  }

  void SetRequiredDocumentPolicy(
      const DocumentPolicyFeatureState& required_document_policy) {
    required_document_policy_ = required_document_policy;
  }

  WindowAgentFactory& window_agent_factory() const {
    return *window_agent_factory_;
  }

  // This identifier represents the stable identifier between a
  // LocalFrame  <--> RenderFrameHostImpl or a
  // RemoteFrame <--> RenderFrameProxyHost in the browser process.
  const FrameToken& GetFrameToken() const { return frame_token_; }

  bool GetVisibleToHitTesting() const { return visible_to_hit_testing_; }
  void UpdateVisibleToHitTesting();

  void ScheduleFormSubmission(FrameScheduler* scheduler,
                              FormSubmission* form_submission);
  void CancelFormSubmission();
  bool IsFormSubmissionPending();

  // Asks the browser process to activate the page associated to the current
  // Frame, reporting |originating_frame| as the local frame originating this
  // request.
  void FocusPage(LocalFrame* originating_frame);

  // Called when the focus controller changes the focus to this frame.
  virtual void DidFocus() = 0;

  virtual gfx::Size GetMainFrameViewportSize() const = 0;
  virtual gfx::Point GetMainFrameScrollOffset() const = 0;

  // Sets this frame's opener to another frame, or disowned the opener
  // if opener is null. See http://html.spec.whatwg.org/#dom-opener.
  virtual void SetOpener(Frame* opener) = 0;

  void SetOpenerDoNotNotify(Frame* opener);

  // Returns the frame that opened this frame or null if there is none.
  Frame* Opener() const { return opener_; }

  // Returns the parent frame or null if this is the top-most frame.
  Frame* Parent(FrameTreeBoundary frame_tree_boundary =
                    FrameTreeBoundary::kIgnoreFence) const;

  // Returns the top-most frame in the hierarchy containing this frame.
  Frame* Top(
      FrameTreeBoundary frame_tree_boundary = FrameTreeBoundary::kIgnoreFence);

  // Returns the first child frame.
  Frame* FirstChild(FrameTreeBoundary frame_tree_boundary =
                        FrameTreeBoundary::kIgnoreFence) const;

  // Returns the previous sibling frame.
  Frame* PreviousSibling() const { return previous_sibling_; }

  // Returns the next sibling frame.
  Frame* NextSibling() const { return next_sibling_; }

  // Returns the last child frame.
  Frame* LastChild() const { return last_child_; }

  // TODO(dcheng): these should probably all have restricted visibility. They
  // are not intended for general usage.
  // Detaches a frame from its parent frame if it has one.
  void DetachFromParent();

  bool Swap(WebFrame*);

  // Removes the given child from this frame.
  void RemoveChild(Frame* child);

  LocalFrame* ProvisionalFrame() const { return provisional_frame_; }
  void SetProvisionalFrame(LocalFrame* provisional_frame) {
    // There should only be null -> non-null or non-null -> null transitions
    // here. Anything else indicates a logic error in the code managing this
    // state.
    DCHECK_NE(!!provisional_frame, !!provisional_frame_);
    provisional_frame_ = provisional_frame;
  }

  // Returns false if fenced frames are disabled. Returns true if the
  // feature is enabled and if |this| or any of its ancestor nodes is a
  // fenced frame. For MPArch based fenced frames returns the value of
  // Page::IsMainFrameFencedFrameRoot and for shadowDOM based fenced frames
  // returns true, if the FrameTree that this frame is in is not the outermost
  // FrameTree.
  bool IsInFencedFrameTree() const;

 protected:
  // |inheriting_agent_factory| should basically be set to the parent frame or
  // opener's WindowAgentFactory. Pass nullptr if the frame is isolated from
  // other frames (i.e. if it and its child frames shall never be script
  // accessible from other frames), and a new WindowAgentFactory will be
  // created.
  Frame(FrameClient*,
        Page&,
        FrameOwner*,
        Frame* parent,
        Frame* previous_sibling,
        FrameInsertType insert_type,
        const FrameToken& frame_token,
        const base::UnguessableToken& devtools_frame_token,
        WindowProxyManager*,
        WindowAgentFactory* inheriting_agent_factory);

  // Perform initialization that must happen after the constructor has run so
  // that vtables are initialized.
  void Initialize();

  // DetachImpl() may be reentered if a frame is reentrantly removed whilst in
  // the process of detaching (for removal or swap). Overrides should return
  // false if interrupted by reentrant removal of `this`, and true otherwise.
  // See `Detach()` for more information.
  virtual bool DetachImpl(FrameDetachType) = 0;

  virtual void DidChangeVisibleToHitTesting() = 0;

  void FocusImpl();

  void ApplyFrameOwnerProperties(
      mojom::blink::FrameOwnerPropertiesPtr properties);

  void NotifyUserActivationInFrameTree(
      mojom::blink::UserActivationNotificationType notification_type);
  bool ConsumeTransientUserActivationInFrameTree();
  void ClearUserActivationInFrameTree();

  void RenderFallbackContent();
  void RenderFallbackContentWithResourceTiming(
      mojom::blink::ResourceTimingInfoPtr timing,
      const String& server_timing_values);

  mutable FrameTree tree_node_;

  Member<Page> page_;
  Member<FrameOwner> owner_;
  Member<DOMWindow> dom_window_;

  // This is set to true if this is a subframe, and the frame element in the
  // parent frame's document becomes inert. This should always be false for
  // the main frame.
  bool is_inert_ = false;

  TouchAction inherited_effective_touch_action_ = TouchAction::kAuto;

  bool visible_to_hit_testing_ = true;

 private:
  // Inserts the given frame as a child of this frame, so that it is the next
  // child after |previous_sibling|, or first child if |previous_sibling| is
  // null. The child frame's parent must be set in the constructor.
  void InsertAfter(Frame* new_child, Frame* previous_sibling);

  Member<FrameClient> client_;
  const Member<WindowProxyManager> window_proxy_manager_;
  FrameLifecycle lifecycle_;

  Member<Frame> opener_;
  Member<Frame> parent_;
  Member<Frame> previous_sibling_;
  Member<Frame> next_sibling_;
  Member<Frame> first_child_;
  Member<Frame> last_child_;

  Member<LocalFrame> provisional_frame_;

  NavigationRateLimiter navigation_rate_limiter_;

  // Sandbox flags inherited from an opener. It is always empty for child
  // frames.
  network::mojom::blink::WebSandboxFlags opener_sandbox_flags_;

  // The required document policy for any subframes of this frame.
  // Note: current frame's document policy might not conform to
  // |required_document_policy_| here, as the Require-Document-Policy HTTP
  // header can specify required document policy which only takes effect for
  // subtree frames.
  DocumentPolicyFeatureState required_document_policy_;

  Member<WindowAgentFactory> window_agent_factory_;

  // TODO(sashab): Investigate if this can be represented with m_lifecycle.
  bool is_loading_;
  // Contains token to be used as a frame id in the devtools protocol.
  base::UnguessableToken devtools_frame_token_;
  absl::optional<std::string> trace_value_;

  // Embedding token, if existing, associated to this frame. For local frames
  // this will only be valid if the frame has committed a navigation and will
  // change when a new document is committed. For remote frames this will only
  // be valid when owned by an HTMLFrameOwnerElement.
  absl::optional<base::UnguessableToken> embedding_token_;

  // The user activation state of the current frame.  See |UserActivationState|
  // for details on how this state is maintained.
  UserActivationState user_activation_state_;

  // The sticky user activation state of the current frame before eTLD+1
  // navigation.  This is used in autoplay.
  bool had_sticky_user_activation_before_nav_ = false;

  // This identifier represents the stable identifier between a
  // LocalFrame  <--> RenderFrameHostImpl or a
  // RemoteFrame <--> RenderFrameProxyHost in the browser process.
  // Note that this identifier is unique per render process and
  // browser relationship. ie. If this is a LocalFrame, RemoteFrames that
  // represent this frame node in other processes will *not* have the same
  // identifier. Similarly, if this is a RemoteFrame, the LocalFrame and
  // other RemoteFrames that represent this frame node in other processes
  // will *not* have the same identifier. This is different than the
  // |devtools_frame_token_| in which all representations of this frame node
  // have the same value in all processes.
  FrameToken frame_token_;

  // This task is used for the async step in form submission when a form is
  // targeting this frame. http://html.spec.whatwg.org/C/#plan-to-navigate
  // The reason it is stored here is so that it can handle both LocalFrames and
  // RemoteFrames, and so it can be canceled by FrameLoader.
  TaskHandle form_submit_navigation_task_;

  OpenedFrameTracker opened_frame_tracker_;
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
static inline std::string ToTraceValue(Frame* frame) {
  return frame ? frame->ToTraceValue() : std::string();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_H_
