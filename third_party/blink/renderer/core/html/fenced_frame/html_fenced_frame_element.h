// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_HTML_FENCED_FRAME_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_HTML_FENCED_FRAME_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class KURL;

// HTMLFencedFrameElement implements the <fencedframe> element, which hosts the
// main frame of a top-level browsing context in an isolated frame. This element
// is non-standard and is currently being developed in
// https://github.com/shivanigithub/fenced-frame. As a result, this element is
// not exposed by default, but can be enabled by one of the following:
// - Enabling the Fenced Frames about:flags entry
// - Passing --enable-features=FencedFrames
class CORE_EXPORT HTMLFencedFrameElement : public HTMLFrameOwnerElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // For a while there will be two underlying implementations of Fenced Frames:
  //   1.) The early Origin Trial implementation based on the ShadowDOM
  //       encapsulating a neutered <iframe> element
  //   2.) The MPArch implementation, which hosts a truly top-level FrameTree in
  //       the browser process, and relies on the MPArch long-tail feature work
  // For as long as both of these implementations need to exist, we abstract a
  // common API from them which is neatly captured by `FencedFrameDelegate`. The
  // actual implementation of this interface will be one of the options listed
  // above. See documentation above `FencedFrameMPArchDelegate` and
  // `FencedFrameShadowDOMDelegate`.
  class CORE_EXPORT FencedFrameDelegate
      : public GarbageCollected<FencedFrameDelegate> {
   public:
    static FencedFrameDelegate* Create(HTMLFencedFrameElement*);
    explicit FencedFrameDelegate(HTMLFencedFrameElement* outer_element)
        : outer_element_(outer_element) {}
    virtual ~FencedFrameDelegate();
    void Trace(Visitor* visitor) const;

    virtual void DidGetInserted() = 0;
    virtual void Navigate(const KURL&) = 0;

   protected:
    HTMLFencedFrameElement& GetElement() const { return *outer_element_; }

   private:
    Member<HTMLFencedFrameElement> outer_element_;
  };

  explicit HTMLFencedFrameElement(Document& document);
  ~HTMLFencedFrameElement() override;
  void Trace(Visitor* visitor) const override;

  // HTMLFrameOwnerElement overrides.
  void DisconnectContentFrame() override;
  FrameOwnerElementType OwnerType() const override {
    return FrameOwnerElementType::kFencedframe;
  }
  ParsedPermissionsPolicy ConstructContainerPolicy() const override {
    NOTREACHED();
    return ParsedPermissionsPolicy();
  }

  // HTMLElement overrides.
  bool IsHTMLFencedFrameElement() const final { return true; }

  // TODO(kojii): Currently followings members are valid only when non-MPArch.
  // They may better be moved to |FencedFrameDelegate| once how to achieve the
  // desired layout behavior on MPArch has been determined.

  // The frame size is "frozen" when the `src` attribute is set.
  // The frozen state is kept in this element so that it can survive across
  // reattaches.
  const absl::optional<PhysicalSize>& FrozenFrameSize() const {
    return frozen_frame_size_;
  }
  // True if the frame size should be frozen when the next resize completed.
  // When `src` is set but layout is not completed yet, the frame size is frozen
  // after the first layout.
  bool ShouldFreezeFrameSizeOnNextLayoutForTesting() const {
    return should_freeze_frame_size_on_next_layout_;
  }

  // Returns the inner `IFRAME` element. This element creates two boxes, the
  // outer container and the inner frame, so that the outer container can
  // respond to the size change requests from the containing layout algorithm,
  // while keeping the inner frame size unchanged.
  HTMLIFrameElement* InnerIFrameElement() const;

 private:
  // This method will only navigate the underlying frame if the element
  // `isConnected()`.
  void Navigate();

  // Node overrides.
  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void DidNotifySubtreeInsertionsToDocument() override;
  void RemovedFrom(ContainerNode& node) override;

  // Element overrides.
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsURLAttribute(const Attribute&) const override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  void AttachLayoutTree(AttachContext& context) override;
  bool SupportsFocus() const override;

  void FreezeFrameSize();

  void StartResizeObserver();
  void OnResize(const PhysicalRect& content_box);
  void UpdateInnerStyleOnFrozenInternalFrame();

  class ResizeObserverDelegate final : public ResizeObserver::Delegate {
   public:
    void OnResize(const HeapVector<Member<ResizeObserverEntry>>& entries) final;
  };

  // The underlying <fencedframe> implementation that we delegate all of the
  // important bits to. See the comment above this class declaration.
  Member<FencedFrameDelegate> frame_delegate_;
  Member<ResizeObserver> resize_observer_;
  // See |FrozenFrameSize| above.
  absl::optional<PhysicalSize> frozen_frame_size_;
  absl::optional<PhysicalRect> content_rect_;
  bool should_freeze_frame_size_on_next_layout_ = false;

  friend class ResizeObserverDelegate;
};

// Type casting. Custom since adoption could lead to an HTMLFencedFrameElement
// ending up in a document that doesn't have the Fenced Frame origin trial
// enabled, which would result in creation of an HTMLUnknownElement with the
// "fencedframe" tag name. We can't support casting those elements to
// HTMLFencedFrameElements because they are not fenced frame elements.
// TODO(crbug.com/1123606): Remove these custom helpers when the origin trial is
// over.
template <>
struct DowncastTraits<HTMLFencedFrameElement> {
  static bool AllowFrom(const HTMLElement& element) {
    return element.IsHTMLFencedFrameElement();
  }
  static bool AllowFrom(const Node& node) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(node))
      return html_element->IsHTMLFencedFrameElement();
    return false;
  }
  static bool AllowFrom(const Element& element) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(element))
      return html_element->IsHTMLFencedFrameElement();
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_HTML_FENCED_FRAME_ELEMENT_H_
