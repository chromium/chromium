// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_WEBID_UTILS_H_
#define CONTENT_BROWSER_WEBID_WEBID_UTILS_H_

#include "url/gurl.h"
#include "url/origin.h"

namespace blink::mojom {
enum class IdpSigninStatus;
}  // namespace blink::mojom

namespace content {
class BrowserContext;
enum class IdpSigninStatus;

namespace webid {

void SetIdpSigninStatus(content::BrowserContext* context,
                        const url::Origin& origin,
                        blink::mojom::IdpSigninStatus status);

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_WEBID_UTILS_H_
