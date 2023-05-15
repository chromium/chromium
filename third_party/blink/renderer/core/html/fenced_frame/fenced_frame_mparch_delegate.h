// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_MPARCH_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_MPARCH_DELEGATE_H_

#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"

namespace blink {

class KURL;

// This is the underlying implementations of the `HTMLFencedFrameElement`
// interface. It can be activated by enabling the
// `blink::features::kFencedFrames` feature. See the documentation above
// `FencedFrameDelegate`.
class CORE_EXPORT FencedFrameMPArchDelegate
    : public HTMLFencedFrameElement::FencedFrameDelegate {
 public:
  explicit FencedFrameMPArchDelegate(HTMLFencedFrameElement* outer_element);

  void Navigate(const KURL&, const String&) override;
  void Dispose() override;
  void AttachLayoutTree() override;
  bool SupportsFocus() override;
  void MarkFrozenFrameSizeStale() override;
  void MarkContainerSizeStale() override;
  void DidChangeFramePolicy(const FramePolicy&) override;

  void Trace(Visitor* visitor) const override;

 private:
  HeapMojoAssociatedRemote<mojom::blink::FencedFrameOwnerHost> remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_MPARCH_DELEGATE_H_
