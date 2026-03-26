// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/frame_owner_element_type_mojom_traits.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-shared.h"

namespace mojo {

blink::mojom::FrameOwnerElementType EnumTraits<
    blink::mojom::FrameOwnerElementType,
    blink::FrameOwnerElementType>::ToMojom(blink::FrameOwnerElementType input) {
  switch (input) {
    case blink::FrameOwnerElementType::kIframe:
      return blink::mojom::FrameOwnerElementType::kIframe;
    case blink::FrameOwnerElementType::kObject:
      return blink::mojom::FrameOwnerElementType::kObject;
    case blink::FrameOwnerElementType::kEmbed:
      return blink::mojom::FrameOwnerElementType::kEmbed;
    case blink::FrameOwnerElementType::kFrame:
      return blink::mojom::FrameOwnerElementType::kFrame;
    case blink::FrameOwnerElementType::kFencedframe:
      return blink::mojom::FrameOwnerElementType::kFencedframe;
    case blink::FrameOwnerElementType::kNone:
      return blink::mojom::FrameOwnerElementType::kNone;
  }
  NOTREACHED();
}

blink::FrameOwnerElementType
EnumTraits<blink::mojom::FrameOwnerElementType, blink::FrameOwnerElementType>::
    FromMojom(blink::mojom::FrameOwnerElementType input) {
  switch (input) {
    case blink::mojom::FrameOwnerElementType::kIframe:
      return blink::FrameOwnerElementType::kIframe;
    case blink::mojom::FrameOwnerElementType::kObject:
      return blink::FrameOwnerElementType::kObject;
    case blink::mojom::FrameOwnerElementType::kEmbed:
      return blink::FrameOwnerElementType::kEmbed;
    case blink::mojom::FrameOwnerElementType::kFrame:
      return blink::FrameOwnerElementType::kFrame;
    case blink::mojom::FrameOwnerElementType::kFencedframe:
      return blink::FrameOwnerElementType::kFencedframe;
    case blink::mojom::FrameOwnerElementType::kNone:
      NOTREACHED();
  }
  NOTREACHED();
}
}  // namespace mojo
