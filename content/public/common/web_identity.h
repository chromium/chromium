// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_WEB_IDENTITY_H_
#define CONTENT_PUBLIC_COMMON_WEB_IDENTITY_H_

#include <memory>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "net/http/structured_headers.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace url {
class Origin;
}  // namespace url

namespace blink::mojom {
enum class IdpSigninStatus;
}  // namespace blink::mojom

namespace content {

typedef base::RepeatingCallback<void(
    const std::optional<url::Origin>& initiator,
    const url::Origin& idp_origin,
    blink::mojom::IdpSigninStatus status)>
    SetIdpStatusCallback;

using ParseSetLoginHeaderCallback = base::RepeatingCallback<void(
    const std::string& header_value,
    base::OnceCallback<void(
        std::optional<net::structured_headers::ParameterizedItem> item)>
        callback)>;

CONTENT_EXPORT ParseSetLoginHeaderCallback GetSetLoginHeaderInProcessParser();
CONTENT_EXPORT ParseSetLoginHeaderCallback GetSetLoginHeaderDataDecoderParser();

CONTENT_EXPORT std::unique_ptr<blink::URLLoaderThrottle>
MaybeCreateIdentityUrlLoaderThrottle(SetIdpStatusCallback status_cb,
                                     ParseSetLoginHeaderCallback parse_cb);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_WEB_IDENTITY_H_
