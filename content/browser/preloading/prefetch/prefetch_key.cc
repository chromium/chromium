// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_key.h"

#include <variant>

#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "net/base/network_isolation_key.h"
#include "url/gurl.h"

namespace content {

PrefetchKey::PrefetchKey(
    std::optional<blink::DocumentToken> referring_document_token,
    GURL url)
    : referring_document_token_or_nik_(std::move(referring_document_token)),
      url_(std::move(url)) {
  CHECK(!PrefetchNIKScopeEnabled());
}

PrefetchKey::PrefetchKey(
    net::NetworkIsolationKey referring_network_isolation_key,
    GURL url)
    : referring_document_token_or_nik_(
          std::move(referring_network_isolation_key)),
      url_(std::move(url)) {
  CHECK(PrefetchNIKScopeEnabled());
}

PrefetchKey::~PrefetchKey() = default;

PrefetchKey::PrefetchKey(PrefetchKey&& other) = default;

PrefetchKey& PrefetchKey::operator=(PrefetchKey&& other) = default;

PrefetchKey::PrefetchKey(const PrefetchKey& other) = default;

PrefetchKey& PrefetchKey::operator=(const PrefetchKey& other) = default;

std::ostream& operator<<(std::ostream& ostream,
                         const PrefetchKey& prefetch_key) {
  ostream << "(";
  if (const auto* token = std::get_if<std::optional<blink::DocumentToken>>(
          &prefetch_key.referring_document_token_or_nik_)) {
    token->has_value() ? ostream << token->value()
                       : ostream << "(empty document token)";
  } else {
    ostream << std::get<net::NetworkIsolationKey>(
                   prefetch_key.referring_document_token_or_nik_)
                   .ToDebugString();
  }
  ostream << ", " << prefetch_key.url() << ")";
  return ostream;
}

}  // namespace content
