// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_client_hints_utils.h"

#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace content {

bool PersistClientHintsHelper(const GURL& url,
                              const blink::EnabledClientHints& client_hints,
                              ClientHintsContainer* container) {
  DCHECK(container);
  if (!network::IsUrlPotentiallyTrustworthy(url)) {
    return false;
  }
  ClientHintsPersistencyData data;
  data.client_hints = client_hints;
  const url::Origin origin = url::Origin::Create(url);
  (*container)[origin] = data;
  return true;
}

void GetAllowedClientHintsFromSourceHelper(
    const GURL& url,
    const ClientHintsContainer& container,
    blink::EnabledClientHints* client_hints) {
  const url::Origin origin = url::Origin::Create(url);
  const auto& it = container.find(origin);
  DCHECK(client_hints);
  if (it != container.end()) {
    *client_hints = it->second.client_hints;
  }
}

}  // namespace content
