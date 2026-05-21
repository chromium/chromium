// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_

#include <stdint.h>

#include <string_view>
#include <vector>

#include "components/subresource_filter/core/common/scoped_rule.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"

namespace blink {

class WebURL;

class WebDocumentSubresourceFilter {
 public:
  // This builder class is created on the main thread and passed to a worker
  // thread to create the subresource filter for the worker thread.
  class Builder {
   public:
    virtual ~Builder() = default;
    virtual std::unique_ptr<WebDocumentSubresourceFilter> Build() = 0;
  };

  enum LoadPolicy { kAllow, kDisallow, kWouldDisallow };

  virtual ~WebDocumentSubresourceFilter() = default;
  virtual LoadPolicy GetLoadPolicy(
      const WebURL& resource_url,
      network::mojom::RequestDestination,
      subresource_filter::ScopedRule* out_rule) = 0;
  virtual LoadPolicy GetLoadPolicyForWebSocketConnect(const WebURL&) = 0;
  virtual LoadPolicy GetLoadPolicyForWebTransportConnect(const WebURL&) = 0;

  // Returns the style selectors that apply to the current document's
  // domain/origin. These are rules that are restricted to specific domains
  // (e.g. "example.com##.ad").
  virtual void GetDomainSelectors(
      std::vector<std::string_view>& out_selectors) = 0;

  // Returns true if the ruleset might contain style rules matching the given
  // `hash` (which is a hash of a class or ID name). This is a fast Bloom
  // filter check used to avoid expensive operations for classes/IDs that
  // definitely don't have associated selectors. `hash` must be the hash of the
  // id or class (computed via GetStyleRuleHash or AtomicString::Hash() for
  // ASCII).
  virtual bool MaybeHasStyleRule(uint32_t hash) = 0;

  // Appends to `out_selectors` the filterlist selectors that contain
  // `class_name` and apply to the current document's origin. `hash` must be the
  // hash of `class_name` (computed via GetStyleRuleHash or AtomicString::Hash()
  // for ASCII).
  virtual void GetSelectorsByClass(
      std::string_view class_name,
      uint32_t hash,
      std::vector<std::string_view>& out_selectors) = 0;

  // Appends to `out_selectors` the filterlist selectors that contain `id_name`
  // and apply to the current document's origin. `hash` must be the hash of
  // `id_name` (computed via GetStyleRuleHash or AtomicString::Hash() for
  // ASCII).
  virtual void GetSelectorsById(
      std::string_view id_name,
      uint32_t hash,
      std::vector<std::string_view>& out_selectors) = 0;

  // Returns true if the filter is operating in dry-run mode (i.e. it detects
  // ads but doesn't actually block them).
  virtual bool IsDryRun() = 0;

  // Returns the unique ID of the ruleset currently being used for filtering.
  // This is used by the renderer (e.g. in SubresourceStyleFilter) to partition
  // caches by ruleset version.
  virtual uint64_t GetRulesetId() const = 0;

  // Report that a resource loaded by the document (not a preload) was
  // disallowed.
  virtual void ReportDisallowedLoad() = 0;

  // Returns true if disallowed resource loads should be logged to the devtools
  // console.
  virtual bool ShouldLogToConsole() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_
