// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_KEY_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_KEY_H_

#include <optional>
#include <variant>

#include "content/common/content_export.h"
#include "net/base/network_isolation_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace content {

// Key for managing and matching prefetches.
//
// This key can either represent
//
// - the key of a prefetch (typically named `prefetch_key`, and its URL is the
//   URL of the prefetched main resource); or
// - the key of a navigation (typically named `navigated_key`, and its URL is
//   the navigation request URL).
//
// TODO(crbug.com/364751887): This distinction is not perfect. Enforce it as
// much as possible.
//
// For prefetch, non URL part is given as the following:
//
// - If the prefetch is renderer-initiated, `DocumentToken` of the initiating
//   document is used.
// - If the prefetch is browser-initiated, `std::nullopt` (for
//   `referring_document_token`) is used.
// - If the prefetch is embedder-initiated, `net::NetworkIsolationKey` of the
//   embedder is used. See crbug.com/40942681.
//
// For navigation, `std::optional<DocumentToken>` of the initiating document
// of the navigation is used.
//
// See also the doc on crbug.com/40946257 for more context.
class CONTENT_EXPORT PrefetchKey final {
 public:
  PrefetchKey() = delete;
  PrefetchKey(net::NetworkIsolationKey nik, GURL url);
  PrefetchKey(std::optional<blink::DocumentToken> referring_document_token,
              GURL url);
  ~PrefetchKey();

  // Movable and copyable.
  PrefetchKey(PrefetchKey&& other);
  PrefetchKey& operator=(PrefetchKey&& other);
  PrefetchKey(const PrefetchKey& other);
  PrefetchKey& operator=(const PrefetchKey& other);

  bool operator==(const PrefetchKey& rhs) const = default;
  bool operator<(const PrefetchKey& rhs) const {
    if (referring_document_token_or_nik_ !=
        rhs.referring_document_token_or_nik_) {
      return referring_document_token_or_nik_ <
             rhs.referring_document_token_or_nik_;
    }
    return url_ < rhs.url_;
  }

  const GURL& url() const { return url_; }

  PrefetchKey WithNewUrl(const GURL& new_url) const {
    return std::visit([&](const auto& e) { return PrefetchKey(e, new_url); },
                      referring_document_token_or_nik_);
  }

  bool NonUrlPartIsSame(const PrefetchKey& other) const {
    return referring_document_token_or_nik_ ==
           other.referring_document_token_or_nik_;
  }

 private:
  friend CONTENT_EXPORT std::ostream& operator<<(
      std::ostream& ostream,
      const PrefetchKey& prefetch_key);

  std::variant<std::optional<blink::DocumentToken>, net::NetworkIsolationKey>
      referring_document_token_or_nik_;
  GURL url_;
};

CONTENT_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                        const PrefetchKey& prefetch_key);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_KEY_H_
