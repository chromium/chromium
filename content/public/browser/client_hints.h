// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_H_
#define CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_H_

#include "content/common/content_export.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"

namespace content {

class BrowserContext;

// Adds client hints headers for a prefetch navigation that is not associated
// with a frame. It must be a main frame navigation. |is_javascript_enabled| is
// whether JavaScript is enabled in blink or not.
CONTENT_EXPORT void AddClientHintsHeadersToPrefetchNavigation(
    const url::Origin& origin,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    bool is_javascript_enabled);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AcceptCHFrameRestart {
  kFramePresent = 0,
  kNavigationRestarted = 1,
  kRedirectOverflow = 2,
  kMaxValue = kRedirectOverflow,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_H_
