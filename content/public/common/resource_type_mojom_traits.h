// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_MOJOM_TRAITS_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_MOJOM_TRAITS_H_

#include "content/public/common/resource_type.h"
#include "content/public/common/resource_type.mojom-shared.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<content::mojom::ResourceType, content::ResourceType> {
  static content::mojom::ResourceType ToMojom(content::ResourceType input);
  static bool FromMojom(content::mojom::ResourceType input,
                        content::ResourceType* output);
};
}  // namespace mojo

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_TYPE_MOJOM_TRAITS_H_
