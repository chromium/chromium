// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_owner_element_type_mojom_traits.h"
#include "third_party/blink/public/common/features.h"

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
    case blink::FrameOwnerElementType::kFencedframe:
      // Only Fenced Frames based on Shadow DOM may create child frames.
      DCHECK(blink::features::IsFencedFramesEnabled() &&
             blink::features::kFencedFramesImplementationTypeParam.Get() !=
                 blink::features::FencedFramesImplementationType::kMPArch);
      return content::mojom::ChildFrameOwnerElementType::kFencedframe;
    default:
      // `ChildFrameOwnerElementType` is a subset of
      // `blink::FrameOwnerElementType` and consists of only the values that
      // represent child frames. The `input` must represent a child frame to be
      // serializable. See the definition of `ChildFrameOwnerElementType`
      // for details.
      NOTREACHED();
      return content::mojom::ChildFrameOwnerElementType::kIframe;
  }
}

// static
bool EnumTraits<content::mojom::ChildFrameOwnerElementType,
                blink::FrameOwnerElementType>::
    FromMojom(content::mojom::ChildFrameOwnerElementType input,
              blink::FrameOwnerElementType* output) {
  switch (input) {
    case content::mojom::ChildFrameOwnerElementType::kIframe:
      *output = blink::FrameOwnerElementType::kIframe;
      return true;
    case content::mojom::ChildFrameOwnerElementType::kObject:
      *output = blink::FrameOwnerElementType::kObject;
      return true;
    case content::mojom::ChildFrameOwnerElementType::kEmbed:
      *output = blink::FrameOwnerElementType::kEmbed;
      return true;
    case content::mojom::ChildFrameOwnerElementType::kFrame:
      *output = blink::FrameOwnerElementType::kFrame;
      return true;
    case content::mojom::ChildFrameOwnerElementType::kFencedframe:
      // Only Fenced Frames based on Shadow DOM may create child frames.
      if (!blink::features::IsFencedFramesEnabled() ||
          blink::features::kFencedFramesImplementationTypeParam.Get() ==
              blink::features::FencedFramesImplementationType::kMPArch) {
        return false;
      }
      *output = blink::FrameOwnerElementType::kFencedframe;
      return true;
  }
  return false;
}

}  // namespace mojo
