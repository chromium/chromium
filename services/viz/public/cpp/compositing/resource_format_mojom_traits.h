// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_FORMAT_MOJOM_TRAITS_H_
#define SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_FORMAT_MOJOM_TRAITS_H_

#include "components/viz/common/resources/resource_format.h"
#include "services/viz/public/mojom/compositing/resource_format.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<viz::mojom::ResourceFormat, viz::ResourceFormat> {
  static viz::mojom::ResourceFormat ToMojom(viz::ResourceFormat type);

  static bool FromMojom(viz::mojom::ResourceFormat input,
                        viz::ResourceFormat* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PUBLIC_CPP_COMPOSITING_RESOURCE_FORMAT_MOJOM_TRAITS_H_
