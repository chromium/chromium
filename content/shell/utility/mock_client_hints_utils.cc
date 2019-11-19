// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/utility/mock_client_hints_utils.h"

namespace content {

bool PersistClientHintsHelper(const GURL& url,
                              const blink::WebEnabledClientHints& client_hints,
                              base::TimeDelta expiration_duration,
                              ClientHintsContainer* container) {
  DCHECK(container);
  if (!content::IsOriginSecure(url) ||
      expiration_duration <= base::TimeDelta()) {
    return false;
  }
  ClientHintsPersistencyData data;
  data.expiration = base::Time::Now() + expiration_duration;
  data.client_hints = client_hints;
  const url::Origin origin = url::Origin::Create(url);
  (*container)[origin] = data;
  return true;
}

void GetAllowedClientHintsFromSourceHelper(
    const GURL& url,
    const ClientHintsContainer& container,
    blink::WebEnabledClientHints* client_hints) {
  const url::Origin origin = url::Origin::Create(url);
  const auto& it = container.find(origin);
  DCHECK(client_hints);
  if (it != container.end() && it->second.expiration >= base::Time::Now()) {
    *client_hints = it->second.client_hints;
  }
}

}  // namespace content
