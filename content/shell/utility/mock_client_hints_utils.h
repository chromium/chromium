// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_UTILITY_MOCK_CLIENT_HINTS_UTILS_H_
#define CONTENT_SHELL_UTILITY_MOCK_CLIENT_HINTS_UTILS_H_

#include "content/public/common/origin_util.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "third_party/blink/public/platform/web_url.h"

namespace content {

struct ClientHintsPersistencyData {
  blink::WebEnabledClientHints client_hints;
  base::Time expiration;
};

using ClientHintsContainer =
    std::map<const url::Origin, ClientHintsPersistencyData>;

bool PersistClientHintsHelper(
    const GURL& url,
    const blink::WebEnabledClientHints& enabled_client_hints,
    base::TimeDelta expiration_duration,
    ClientHintsContainer* container);

void GetAllowedClientHintsFromSourceHelper(
    const GURL& url,
    const ClientHintsContainer& container,
    blink::WebEnabledClientHints* client_hints);

}  // namespace content

#endif  // CONTENT_SHELL_UTILITY_MOCK_CLIENT_HINTS_UTILS_H_
