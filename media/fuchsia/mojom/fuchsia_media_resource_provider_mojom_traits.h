// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_MOJOM_FUCHSIA_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_
#define MEDIA_FUCHSIA_MOJOM_FUCHSIA_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_

#include <fuchsia/media/drm/cpp/fidl.h>

#include "fuchsia/mojom/fidl_interface_request_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<
    media::mojom::CdmRequestDataView,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>>
    : public FidlInterfaceRequestStructTraits<
          media::mojom::CdmRequestDataView,
          fuchsia::media::drm::ContentDecryptionModule> {};

}  // namespace mojo

#endif  // MEDIA_FUCHSIA_MOJOM_FUCHSIA_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_
