// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_MOJOM_FUCHSIA_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_
#define MEDIA_FUCHSIA_MOJOM_FUCHSIA_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_

#include <fuchsia/media/cpp/fidl.h>

#include "fuchsia/mojom/fidl_interface_request_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::AudioConsumerRequestDataView,
                    fidl::InterfaceRequest<fuchsia::media::AudioConsumer>>
    : public FidlInterfaceRequestStructTraits<
          media::mojom::AudioConsumerRequestDataView,
          fuchsia::media::AudioConsumer> {};

template <>
struct StructTraits<media::mojom::AudioCapturerRequestDataView,
                    fidl::InterfaceRequest<fuchsia::media::AudioCapturer>>
    : public FidlInterfaceRequestStructTraits<
          media::mojom::AudioCapturerRequestDataView,
          fuchsia::media::AudioCapturer> {};

}  // namespace mojo

#endif  // MEDIA_FUCHSIA_MOJOM_FUCHSIA_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_
