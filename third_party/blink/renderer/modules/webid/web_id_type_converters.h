// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_WEB_ID_TYPE_CONVERTERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_WEB_ID_TYPE_CONVERTERS_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink-forward.h"

namespace blink {
class WebIdLogoutRequest;
}  // namespace blink

namespace mojo {

template <>
struct TypeConverter<blink::mojom::blink::LogoutRequestPtr,
                     blink::WebIdLogoutRequest> {
  static blink::mojom::blink::LogoutRequestPtr Convert(
      const blink::WebIdLogoutRequest&);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBID_WEB_ID_TYPE_CONVERTERS_H_
