/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace cc {
class AnimationHost;
class ScrollbarLayerBase;
}  // namespace cc

namespace blink {
class CompositorAnimationTimeline;
class LocalFrame;
class LocalFrameView;
class Page;
class PaintLayerScrollableArea;
class ScrollableArea;

using MainThreadScrollingReasons = uint32_t;
using ScrollbarId = uint64_t;

// ScrollingCoordinator is a page-level object that mediates interactions
// between Blink and the compositor's scroll-related APIs on the composited
// layer representing the scrollbar.
//
// It's responsible for propagating scroll offsets, main-thread scrolling
// reasons, touch action regions, and non-fast-scrollable regions into the
// compositor, as well as creating and managing scrollbar layers.
class CORE_EXPORT ScrollingCoordinator final
    : public GarbageCollected<ScrollingCoordinator>,
      public CompositorScrollCallbacks {
 public:
  explicit ScrollingCoordinator(Page*);
  ~ScrollingCoordinator() override;
  void Trace(Visitor*) const;

  // The LocalFrameView argument is optional, nullptr causes the the scrolling
  // animation host and timeline to be owned by the ScrollingCoordinator. When
  // not null, the host and timeline are attached to the specified
  // LocalFrameView. A LocalFrameView only needs to own them when it is the view
  // for an OOPIF.
  void AnimationHostInitialized(cc::AnimationHost&, LocalFrameView*);
  void WillCloseAnimationHost(LocalFrameView*);

  void WillBeDestroyed();

  void WillDestroyScrollableArea(ScrollableArea*);

  // Updates scroll offset in cc scroll tree immediately. We don't wait for
  // a full document lifecycle update to propagate the scroll offset from blink
  // paint properties to cc paint properties because cc needs the scroll offset
  // to apply the impl-side scroll delta correctly at the beginning of the next
  // frame. The scroll offset in the transform node will still be updated
  // in normal document lifecycle update instead of here.
  // Returns whether the update is successful.
  bool UpdateCompositorScrollOffset(const LocalFrame&, const ScrollableArea&);

  // Updates composited layers after changes to scrollable area  properties
  // like content and container sizes, scrollbar existence, scrollability, etc.
  // Scroll offset changes are updated by UpdateCompositedScrollOffset.
  // TODO(pdr): Factor the container bounds change out of this function. The
  // compositor tracks scroll container bounds on the scroll layer whereas
  // blink uses a separate layer. To ensure the compositor scroll layer has the
  // updated scroll container bounds, this needs to be called when the scrolling
  // contents layer is resized.
  void ScrollableAreaScrollLayerDidChange(PaintLayerScrollableArea*);
  void ScrollableAreaScrollbarLayerDidChange(PaintLayerScrollableArea*,
                                             ScrollbarOrientation);

  cc::AnimationHost* GetCompositorAnimationHost() { return animation_host_; }
  CompositorAnimationTimeline* GetCompositorAnimationTimeline() {
    return programmatic_scroll_animator_timeline_.get();
  }

  // Traverses the frame tree to find the scrollable area using the element id.
  // This function only checks the local frames. This function does not check
  // the VisualViewport element id.
  ScrollableArea* ScrollableAreaWithElementIdInAllLocalFrames(
      const CompositorElementId&);

  // ScrollCallbacks implementation
  void DidScroll(CompositorElementId,
                 const gfx::ScrollOffset&,
                 const base::Optional<cc::TargetSnapAreaElementIds>&) override;
  void DidChangeScrollbarsHidden(CompositorElementId, bool hidden) override;

  base::WeakPtr<ScrollingCoordinator> GetWeakPtr() {
    DCHECK(page_);
    return weak_ptr_factory_.GetWeakPtr();
  }

  // For testing purposes only. This ScrollingCoordinator is reused between
  // web tests, and must be reset for the results to be valid.
  void Reset(LocalFrame*);

 protected:
  bool IsForMainFrame(ScrollableArea*) const;

  Member<Page> page_;

 private:
  void SetScrollbarLayer(ScrollableArea*,
                         ScrollbarOrientation,
                         scoped_refptr<cc::ScrollbarLayerBase>);
  cc::ScrollbarLayerBase* GetScrollbarLayer(ScrollableArea*,
                                            ScrollbarOrientation);
  void RemoveScrollbarLayer(ScrollableArea*, ScrollbarOrientation);

  cc::AnimationHost* animation_host_ = nullptr;
  std::unique_ptr<CompositorAnimationTimeline>
      programmatic_scroll_animator_timeline_;

  using ScrollbarMap = HeapHashMap<Member<ScrollableArea>,
                                   scoped_refptr<cc::ScrollbarLayerBase>>;
  ScrollbarMap horizontal_scrollbars_;
  ScrollbarMap vertical_scrollbars_;

  base::WeakPtrFactory<ScrollingCoordinator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ScrollingCoordinator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_H_
