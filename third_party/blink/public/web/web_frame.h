/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FRAME_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FRAME_H_

#include <memory>
#include "cc/paint/paint_canvas.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_icon_url.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_tree_scope_type.h"
#include "v8/include/v8.h"

namespace blink {

class Frame;
class OpenedFrameTracker;
class Visitor;
class WebLocalFrame;
class WebRemoteFrame;
class WebSecurityOrigin;
class WebView;
enum class WebSandboxFlags;
struct FramePolicy;
struct WebFrameOwnerProperties;

// Frames may be rendered in process ('local') or out of process ('remote').
// A remote frame is always cross-site; a local frame may be either same-site or
// cross-site.
// WebFrame is the base class for both WebLocalFrame and WebRemoteFrame and
// contains methods that are valid on both local and remote frames, such as
// getting a frame's parent or its opener.
class BLINK_EXPORT WebFrame {
 public:
  // FIXME: We already have blink::TextGranularity. For now we support only
  // a part of blink::TextGranularity.
  // Ideally it seems blink::TextGranularity should be broken up into
  // blink::TextGranularity and perhaps blink::TextBoundary and then
  // TextGranularity enum could be moved somewhere to public/, and we could
  // just use it here directly without introducing a new enum.
  enum TextGranularity {
    kCharacterGranularity = 0,
    kWordGranularity,
    kTextGranularityLast = kWordGranularity,
  };

  // Returns the number of live WebFrame objects, used for leak checking.
  static int InstanceCount();

  virtual bool IsWebLocalFrame() const = 0;
  virtual WebLocalFrame* ToWebLocalFrame() = 0;
  virtual bool IsWebRemoteFrame() const = 0;
  virtual WebRemoteFrame* ToWebRemoteFrame() = 0;

  bool Swap(WebFrame*);

  // This method closes and deletes the WebFrame. This is typically called by
  // the embedder in response to a frame detached callback to the WebFrame
  // client.
  virtual void Close();

  // Called by the embedder when it needs to detach the subtree rooted at this
  // frame.
  void Detach();

  // Basic properties ---------------------------------------------------

  // The security origin of this frame.
  WebSecurityOrigin GetSecurityOrigin() const;

  // Updates the snapshotted policy attributes (sandbox flags and feature policy
  // container policy) in the frame's FrameOwner. This is used when this frame's
  // parent is in another process and it dynamically updates this frame's
  // sandbox flags or container policy. The new policy won't take effect until
  // the next navigation.
  void SetFrameOwnerPolicy(const FramePolicy&);

  // The frame's insecure request policy.
  WebInsecureRequestPolicy GetInsecureRequestPolicy() const;

  // The frame's upgrade insecure navigations set.
  WebVector<unsigned> GetInsecureRequestToUpgrade() const;

  // Updates this frame's FrameOwner properties, such as scrolling, margin,
  // or allowfullscreen.  This is used when this frame's parent is in
  // another process and it dynamically updates these properties.
  // TODO(dcheng): Currently, the update only takes effect on next frame
  // navigation.  This matches the in-process frame behavior.
  void SetFrameOwnerProperties(const WebFrameOwnerProperties&);

  // Whether to collapse the frame's owner element in the embedder document,
  // that is, to remove it from the layout as if it did not exist. Only works
  // for <iframe> owner elements.
  void Collapse(bool);

  // Hierarchy ----------------------------------------------------------

  // Returns the containing view.
  virtual WebView* View() const = 0;

  // Returns the frame that opened this frame or 0 if there is none.
  WebFrame* Opener() const;

  // Sets the frame that opened this one or 0 if there is none.
  void SetOpener(WebFrame*);

  // Reset the frame that opened this frame to 0.
  // This is executed between web tests runs
  void ClearOpener();

  // Returns the parent frame or 0 if this is a top-most frame.
  // TODO(sashab): "Virtual" is needed here temporarily to resolve linker errors
  // in core/. Remove the "virtual" keyword once WebFrame and WebLocalFrameImpl
  // have been moved to core/.
  virtual WebFrame* Parent() const;

  // Returns the top-most frame in the hierarchy containing this frame.
  WebFrame* Top() const;

  // Returns the first child frame.
  WebFrame* FirstChild() const;

  // Returns the next sibling frame.
  WebFrame* NextSibling() const;

  // Returns the next frame in "frame traversal order".
  WebFrame* TraverseNext() const;

  // Scripting ----------------------------------------------------------

  // Returns the global proxy object.
  virtual v8::Local<v8::Object> GlobalProxy() const = 0;

  // Returns true if the WebFrame currently executing JavaScript has access
  // to the given WebFrame, or false otherwise.
  static bool ScriptCanAccess(WebFrame*);

  // Navigation ----------------------------------------------------------
  // TODO(clamy): Remove the reload, reloadWithOverrideURL, and loadRequest
  // functions once RenderFrame only calls WebLoadFrame::load.

  // Stops any pending loads on the frame and its children.
  virtual void StopLoading() = 0;

  // Will return true if between didStartLoading and didStopLoading
  // notifications.
  virtual bool IsLoading() const;

  // Utility -------------------------------------------------------------

  // Returns the frame inside a given frame or iframe element. Returns 0 if
  // the given node is not a frame, iframe or if the frame is empty.
  static WebFrame* FromFrameOwnerElement(const WebNode&);

#if INSIDE_BLINK
  // TODO(mustaq): Should be named FromCoreFrame instead.
  static WebFrame* FromFrame(Frame*);
  static Frame* ToCoreFrame(const WebFrame&);

  bool InShadowTree() const { return scope_ == WebTreeScopeType::kShadow; }

  static void TraceFrames(Visitor*, WebFrame*);

  // Detaches a frame from its parent frame if it has one.
  void DetachFromParent();
#endif

 protected:
  explicit WebFrame(WebTreeScopeType);
  virtual ~WebFrame();

  // Sets the parent WITHOUT fulling adding it to the frame tree.
  // Used to lie to a local frame that is replacing a remote frame,
  // so it can properly start a navigation but wait to swap until
  // commit-time.
  void SetParent(WebFrame*);

  // Inserts the given frame as a child of this frame, so that it is the next
  // child after |previousSibling|, or first child if |previousSibling| is null.
  void InsertAfter(WebFrame* child, WebFrame* previous_sibling);

  // Adds the given frame as a child of this frame.
  void AppendChild(WebFrame*);

 private:
#if INSIDE_BLINK
  friend class OpenedFrameTracker;
  friend class WebFrameTest;

  static void TraceFrame(Visitor*, WebFrame*);
#endif

  // Removes the given child from this frame.
  void RemoveChild(WebFrame*);

  const WebTreeScopeType scope_;

  WebFrame* parent_;
  WebFrame* previous_sibling_;
  WebFrame* next_sibling_;
  WebFrame* first_child_;
  WebFrame* last_child_;

  WebFrame* opener_;
  std::unique_ptr<OpenedFrameTracker> opened_frame_tracker_;
};

}  // namespace blink

#endif
