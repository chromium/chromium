// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_SERIALIZER_DELEGATE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_SERIALIZER_DELEGATE_IMPL_H_

#include "third_party/blink/renderer/core/frame/frame_serializer.h"

#include "third_party/blink/public/web/web_frame_serializer.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Frame;
class KURL;
class HTMLImageElement;

// An implementation of FrameSerializer's delegate which is used to serialize a
// frame to a MHTML file.
class FrameSerializerDelegateImpl final : public FrameSerializer::Delegate {
  STACK_ALLOCATED();

 public:
  // Returns a Content-ID to be used for the given frame.
  // See rfc2557 - section 8.3 - "Use of the Content-ID header and CID URLs".
  // Format note - the returned string should be of the form "<foo@bar.com>"
  // (i.e. the strings should include the angle brackets).
  static String GetContentID(Frame* frame);

  FrameSerializerDelegateImpl(WebFrameSerializer::MHTMLPartsGenerationDelegate&,
                              HeapHashSet<WeakMember<const Element>>&);
  FrameSerializerDelegateImpl(const FrameSerializerDelegateImpl&) = delete;
  FrameSerializerDelegateImpl& operator=(const FrameSerializerDelegateImpl&) =
      delete;
  ~FrameSerializerDelegateImpl() override = default;

  // FrameSerializer::Delegate implementation.
  bool ShouldIgnoreElement(const Element&) override;
  bool ShouldIgnoreAttribute(const Element&, const Attribute&) override;
  bool RewriteLink(const Element&, String& rewritten_link) override;
  bool ShouldSkipResourceWithURL(const KURL&) override;
  Vector<Attribute> GetCustomAttributes(const Element&) override;
  std::pair<Node*, Element*> GetAuxiliaryDOMTree(const Element&) const override;

 private:
  bool ShouldIgnoreHiddenElement(const Element&);
  bool ShouldIgnoreMetaElement(const Element&);
  bool ShouldIgnorePopupOverlayElement(const Element&);
  void GetCustomAttributesForImageElement(const HTMLImageElement&,
                                          Vector<Attribute>*);

  WebFrameSerializer::MHTMLPartsGenerationDelegate& web_delegate_;
  HeapHashSet<WeakMember<const Element>>& shadow_template_elements_;
  bool popup_overlays_skipped_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_SERIALIZER_DELEGATE_IMPL_H_
