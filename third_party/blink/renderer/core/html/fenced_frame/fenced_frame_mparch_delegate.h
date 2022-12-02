// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_MPARCH_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_MPARCH_DELEGATE_H_

#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

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

  void Navigate(const KURL&) override;
  void Dispose() override;
  void AttachLayoutTree() override;
  bool SupportsFocus() override;
  void FreezeFrameSize() override;
  void DidChangeFramePolicy(const FramePolicy&) override;

 private:
  mojo::AssociatedRemote<mojom::blink::FencedFrameOwnerHost> remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FENCED_FRAME_FENCED_FRAME_MPARCH_DELEGATE_H_
