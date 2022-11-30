// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_OWNER_ELEMENT_TYPE_MOJOM_TRAITS_H_
#define CONTENT_COMMON_FRAME_OWNER_ELEMENT_TYPE_MOJOM_TRAITS_H_

#include "content/common/content_export.h"
#include "content/common/frame.mojom-shared.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"

namespace mojo {

template <>
struct CONTENT_EXPORT EnumTraits<content::mojom::ChildFrameOwnerElementType,
                                 blink::FrameOwnerElementType> {
  static content::mojom::ChildFrameOwnerElementType ToMojom(
      blink::FrameOwnerElementType input);
  static bool FromMojom(content::mojom::ChildFrameOwnerElementType input,
                        blink::FrameOwnerElementType* output);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_FRAME_OWNER_ELEMENT_TYPE_MOJOM_TRAITS_H_
