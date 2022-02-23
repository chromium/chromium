// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_CLIENT_HINTS_UTILS_H_
#define CONTENT_TEST_MOCK_CLIENT_HINTS_UTILS_H_

#include <map>

#include "content/public/common/origin_util.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/public/platform/web_url.h"

namespace content {

struct ClientHintsPersistencyData {
  blink::EnabledClientHints client_hints;
};

using ClientHintsContainer =
    std::map<const url::Origin, ClientHintsPersistencyData>;

bool PersistClientHintsHelper(
    const GURL& url,
    const blink::EnabledClientHints& enabled_client_hints,
    ClientHintsContainer* container);

void GetAllowedClientHintsFromSourceHelper(
    const GURL& url,
    const ClientHintsContainer& container,
    blink::EnabledClientHints* client_hints);

}  // namespace content

#endif  // CONTENT_TEST_MOCK_CLIENT_HINTS_UTILS_H_
