// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_WEB_IDENTITY_H_
#define CONTENT_PUBLIC_COMMON_WEB_IDENTITY_H_

#include <memory>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace url {
class Origin;
}  // namespace url

namespace blink::mojom {
enum class IdpSigninStatus;
}  // namespace blink::mojom

namespace content {

typedef base::RepeatingCallback<void(const url::Origin&,
                                     blink::mojom::IdpSigninStatus)>
    SetIdpStatusCallback;

CONTENT_EXPORT std::unique_ptr<blink::URLLoaderThrottle>
MaybeCreateIdentityUrlLoaderThrottle(SetIdpStatusCallback cb);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_WEB_IDENTITY_H_
