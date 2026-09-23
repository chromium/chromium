// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_HTML_FENCED_FRAME_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_HTML_FENCED_FRAME_ELEMENT_H_

#include "base/gtest_prod_util.h"
#include "base/notreached.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element_sandbox.h"
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
  using PassKey = base::PassKey<HTMLFencedFrameElement>;

 public:
  // This is the underlying implementation of the `HTMLFencedFrameElement`
  // interface, which creates a Fenced Frame via MPArch. It can be activated by
  // enabling the `blink::features::kFencedFrames` feature.
  class CORE_EXPORT FencedFrameDelegate
      : public GarbageCollected<FencedFrameDelegate> {
   public:
    static FencedFrameDelegate* Create(HTMLFencedFrameElement* outer_element);
    explicit FencedFrameDelegate(HTMLFencedFrameElement* outer_element);
    ~FencedFrameDelegate() = default;

    void Navigate(const KURL&);
    // This method is used to clean up all state in preparation for destruction,
    // even though the destruction may happen arbitrarily later during garbage
    // collection.
    void Dispose();

    void AttachLayoutTree();
    void MarkContainerSizeStale();
    void DidChangeFramePolicy(const FramePolicy& frame_policy);
    bool SupportsFocus();

    void Trace(Visitor* visitor) const;

   protected:
    HTMLFencedFrameElement& GetElement() const { return *outer_element_; }

   private:
    Member<HTMLFencedFrameElement> outer_element_;
  };

  explicit HTMLFencedFrameElement(Document& document);
  ~HTMLFencedFrameElement() override;
  void Trace(Visitor* visitor) const override;

  ElementType GetElementType() const final {
    return ElementType::kHTMLFencedFrameElement;
  }

  DOMTokenList* sandbox() const;

  // HTMLFrameOwnerElement overrides.
  void DisconnectContentFrame() override;
  FrameOwnerElementType OwnerType() const override {
    return FrameOwnerElementType::kFencedframe;
  }
  network::ParsedPermissionsPolicy ConstructContainerPolicy() const override;
  void SetCollapsed(bool) override;
  void DidChangeContainerPolicy() override;

  // HTMLElement overrides.
  bool IsHTMLFencedFrameElement() const final { return true; }

  FencedFrameConfig* config() const { return config_.Get(); }

  // Sets the FencedFrameConfig that this FencedFrame uses, and navigates the
  // frame to the config's URL. If `config` is null, navigates to about:blank.
  void setConfig(FencedFrameConfig* config);

 private:
  // This method will only navigate the underlying frame if the element
  // `isConnected()`. It will be deferred if the page is currently prerendering.
  void Navigate(
      const KURL& url,
      std::optional<gfx::Size> container_size = std::nullopt,
      std::optional<gfx::Size> content_size = std::nullopt);

  // This method delegates to `Navigate()` above only if `this` has a non-null
  // `config_`. If that's the case, this method pulls the appropriate URL off of
  // the config (either supplied by script, or the internal urn uuid that maps
  // to a resource in the browser process's `FencedFrameURLMapping`), and
  // navigates to it.
  void NavigateToConfig();

  // Delegate creation will be deferred if the page is currently prerendering.
  void CreateDelegateAndNavigate();

  // Node overrides.
  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void DidNotifySubtreeInsertionsToDocument() override;
  void RemovedFrom(ContainerNode& node) override;

  // Element overrides.
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      HeapVector<CSSPropertyValue, 8>&) override;
  bool LayoutObjectIsNeeded(const DisplayStyle&) const override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;
  void AttachLayoutTree(AttachContext& context) override;
  FocusableState SupportsFocus(UpdateBehavior update_behavior) const override;

  // Set the size of the fenced frame outer container. Used for container size
  // specified by FencedFrameConfig.
  void SetContainerSize(const gfx::Size& container_size);

  // The underlying <fencedframe> implementation that we delegate all of the
  // important bits to. See the comment above this class declaration.
  // Note: This is null when the document is sandboxed without
  // `kFencedFrameMandatoryUnsandboxedFlags`.
  Member<FencedFrameDelegate> frame_delegate_;
  Member<FencedFrameConfig> config_;
  bool collapsed_by_client_ = false;
  // This represents the element's `mode` attribute. We store it here instead of
  // always reading it off of the element, because after the first navigation it
  // is effectively frozen. Like the frozen size of the frame, it survives
  // element reattachments too. We maintain the `freeze_mode_attribute_`
  // variable below so we can know when to reject updates to `mode_`.
  blink::FencedFrame::DeprecatedFencedFrameMode mode_ =
      blink::FencedFrame::DeprecatedFencedFrameMode::kDefault;
  // Attributes that are modeled off of their iframe equivalents
  AtomicString allow_;
  Member<HTMLIFrameElementSandbox> sandbox_;
};

// Type casting. Custom since adoption could lead to an HTMLFencedFrameElement
// ending up in a document that doesn't have the Fenced Frame origin trial
// enabled, which would result in creation of an HTMLUnknownElement with the
// "fencedframe" tag name. We can't support casting those elements to
// HTMLFencedFrameElements because they are not fenced frame elements.
// See
// https://chromium.googlesource.com/chromium/src.git/+/main/docs/custom_type_helpers_for_origin_trial_elements.md
// for more details.
//
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
