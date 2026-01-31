// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRECONNECT_REQUEST_H_
#define CONTENT_PUBLIC_BROWSER_PRECONNECT_REQUEST_H_

#include "base/types/optional_ref.h"
#include "content/common/content_export.h"
#include "net/base/network_anonymization_key.h"
#include "url/origin.h"

namespace content {

// Stores all values needed to trigger a preconnect/preresolve job to a single
// origin.
struct CONTENT_EXPORT PreconnectRequest {
  // |network_anonymization_key| specifies the key that network requests for the
  // preconnected URL are expected to use. If a request is issued with a
  // different key, it may not use the preconnected socket. It has no effect
  // when |num_sockets| == 0.
  // TODO(crbug.com/447954811): Provide network_restriction_id at relevant
  // call sites of PreconnectRequest. Also, consider not defaulting
  // network_restriction_id to nullopt. This will involve refactoring a bunch of
  // tests to declare an explicit value.
  PreconnectRequest(
      const url::Origin& origin,
      int num_sockets,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      base::optional_ref<base::UnguessableToken> network_restrictions_id =
          std::nullopt);
  PreconnectRequest(const PreconnectRequest&) = default;
  PreconnectRequest(PreconnectRequest&&) = default;
  PreconnectRequest& operator=(const PreconnectRequest&) = default;
  PreconnectRequest& operator=(PreconnectRequest&&) = default;

  url::Origin origin;
  // A zero-value means that we need to preresolve a host only.
  int num_sockets = 0;
  bool allow_credentials = true;
  net::NetworkAnonymizationKey network_anonymization_key;
  std::optional<base::UnguessableToken> network_restrictions_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRECONNECT_REQUEST_H_
