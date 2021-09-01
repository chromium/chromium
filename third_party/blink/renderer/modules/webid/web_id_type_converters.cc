// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webid/web_id_type_converters.h"

#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_id_logout_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace mojo {

using blink::mojom::blink::LogoutRequest;
using blink::mojom::blink::LogoutRequestPtr;

// static
LogoutRequestPtr
TypeConverter<LogoutRequestPtr, blink::WebIdLogoutRequest>::Convert(
    const blink::WebIdLogoutRequest& request) {
  auto mojo_request = LogoutRequest::New();

  mojo_request->endpoint = blink::KURL(request.endpoint());
  mojo_request->account_id = request.accountId();
  return mojo_request;
}

}  // namespace mojo
