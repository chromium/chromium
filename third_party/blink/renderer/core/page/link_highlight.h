// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_LINK_HIGHLIGHT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_LINK_HIGHLIGHT_H_

#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace cc {
class AnimationHost;
}

namespace blink {
class GraphicsContext;
class Page;
class LinkHighlightImpl;
class CompositorAnimationTimeline;
class LocalFrame;
class LayoutObject;

class CORE_EXPORT LinkHighlight final : public GarbageCollected<LinkHighlight> {
 public:
  explicit LinkHighlight(Page&);
  virtual ~LinkHighlight();

  virtual void Trace(blink::Visitor*);

  void ResetForPageNavigation();

  void SetTapHighlight(Node*);

  void StartHighlightAnimationIfNeeded();

  void AnimationHostInitialized(cc::AnimationHost&);
  void WillCloseAnimationHost();

  bool NeedsHighlightEffect(const LayoutObject& object) const {
    return impl_ && NeedsHighlightEffectInternal(object);
  }

  void UpdateBeforePrePaint();
  void UpdateAfterPrePaint();
  void Paint(GraphicsContext&) const;

 private:
  friend class LinkHighlightImplTest;

  void RemoveHighlight();

  LocalFrame* MainFrame() const;

  Page& GetPage() const {
    DCHECK(page_);
    return *page_;
  }

  bool NeedsHighlightEffectInternal(const LayoutObject& object) const;

  Member<Page> page_;
  std::unique_ptr<LinkHighlightImpl> impl_;
  cc::AnimationHost* animation_host_ = nullptr;
  std::unique_ptr<CompositorAnimationTimeline> timeline_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_LINK_HIGHLIGHT_H_
