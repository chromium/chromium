// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_owner_element_type_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

// static
content::mojom::ChildFrameOwnerElementType EnumTraits<
    content::mojom::ChildFrameOwnerElementType,
    blink::FrameOwnerElementType>::ToMojom(blink::FrameOwnerElementType input) {
  switch (input) {
    case blink::FrameOwnerElementType::kIframe:
      return content::mojom::ChildFrameOwnerElementType::kIframe;
    case blink::FrameOwnerElementType::kObject:
      return content::mojom::ChildFrameOwnerElementType::kObject;
    case blink::FrameOwnerElementType::kEmbed:
      return content::mojom::ChildFrameOwnerElementType::kEmbed;
    case blink::FrameOwnerElementType::kFrame:
      return content::mojom::ChildFrameOwnerElementType::kFrame;
    default:
      // `ChildFrameOwnerElementType` is a subset of
      // `blink::FrameOwnerElementType` and consists of only the values that
      // represent child frames. The `input` must represent a child frame to be
      // serializable. See the definition of `ChildFrameOwnerElementType`
      // for details.
      NOTREACHED();
  }
}

// static
blink::FrameOwnerElementType
EnumTraits<content::mojom::ChildFrameOwnerElementType,
           blink::FrameOwnerElementType>::
    FromMojom(content::mojom::ChildFrameOwnerElementType input) {
  switch (input) {
    case content::mojom::ChildFrameOwnerElementType::kIframe:
      return blink::FrameOwnerElementType::kIframe;
    case content::mojom::ChildFrameOwnerElementType::kObject:
      return blink::FrameOwnerElementType::kObject;
    case content::mojom::ChildFrameOwnerElementType::kEmbed:
      return blink::FrameOwnerElementType::kEmbed;
    case content::mojom::ChildFrameOwnerElementType::kFrame:
      return blink::FrameOwnerElementType::kFrame;
  }
  NOTREACHED();
}

}  // namespace mojo
