// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_MOJOM_WEB_ENGINE_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_
#define FUCHSIA_WEB_WEBENGINE_MOJOM_WEB_ENGINE_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_

#include <fuchsia/media/cpp/fidl.h>

#include "mojo/public/cpp/base/fuchsia/fidl_interface_request_mojom_traits.h"

namespace mojo {

template <>
struct StructTraits<mojom::AudioConsumerRequestDataView,
                    fidl::InterfaceRequest<fuchsia::media::AudioConsumer>>
    : public FidlInterfaceRequestStructTraits<
          mojom::AudioConsumerRequestDataView,
          fuchsia::media::AudioConsumer> {};

}  // namespace mojo

#endif  // FUCHSIA_WEB_WEBENGINE_MOJOM_WEB_ENGINE_MEDIA_RESOURCE_PROVIDER_MOJOM_TRAITS_H_
