// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_MOJOM_FUCHSIA_MEDIA_MOJOM_TRAITS_H_
#define MEDIA_MOJO_MOJOM_FUCHSIA_MEDIA_MOJOM_TRAITS_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/media/drm/cpp/fidl.h>

#include "mojo/public/cpp/base/fuchsia/fidl_interface_request_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<
    media::mojom::CdmRequestDataView,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>>
    : public FidlInterfaceRequestStructTraits<
          media::mojom::CdmRequestDataView,
          fuchsia::media::drm::ContentDecryptionModule> {};

template <>
struct StructTraits<media::mojom::StreamProcessorRequestDataView,
                    fidl::InterfaceRequest<fuchsia::media::StreamProcessor>>
    : public FidlInterfaceRequestStructTraits<
          media::mojom::StreamProcessorRequestDataView,
          fuchsia::media::StreamProcessor> {};

}  // namespace mojo

#endif  // MEDIA_MOJO_MOJOM_FUCHSIA_MEDIA_MOJOM_TRAITS_H_
