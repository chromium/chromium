// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FLAT_RULESET_INDEXER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FLAT_RULESET_INDEXER_H_

#include <stddef.h>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/span.h"
#include "base/macros.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/common/api/declarative_net_request.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace extensions {
namespace declarative_net_request {

struct IndexedRule;

// Helper class to index rules in the flatbuffer format for the Declarative Net
// Request API.
class FlatRulesetIndexer {
 public:
  FlatRulesetIndexer();
  ~FlatRulesetIndexer();

  // Adds |indexed_rule| to the ruleset.
  void AddUrlRule(const IndexedRule& indexed_rule);

  // Returns the number of rules added till now.
  size_t indexed_rules_count() const { return indexed_rules_count_; }

  // Finishes the ruleset construction.
  void Finish();

  // Returns the data buffer, which is still owned by FlatRulesetIndexer.
  // Finish() must be called prior to calling this.
  base::span<const uint8_t> GetData();

 private:
  using UrlPatternIndexBuilder = url_pattern_index::UrlPatternIndexBuilder;

  flat::ActionType GetActionType(const IndexedRule& indexed_rule) const;
  uint8_t GetRemoveHeadersMask(const IndexedRule& indexed_rule) const;
  std::vector<UrlPatternIndexBuilder*> GetBuilders(
      const IndexedRule& indexed_rule);
  std::vector<UrlPatternIndexBuilder*> GetRemoveHeaderBuilders(
      const std::set<api::declarative_net_request::RemoveHeaderType>& types);

  flatbuffers::FlatBufferBuilder builder_;

  // This will consist of |flat::ActionIndex_count| builders. We use unique_ptr
  // since UrlPatternIndexBuilder is a non-copyable and non-movable type.
  const std::vector<std::unique_ptr<UrlPatternIndexBuilder>> index_builders_;

  std::vector<flatbuffers::Offset<flat::UrlRuleMetadata>> metadata_;

  std::vector<flatbuffers::Offset<flat::RegexRule>> regex_rules_;

  size_t indexed_rules_count_ = 0;  // Number of rules indexed till now.
  bool finished_ = false;           // Whether Finish() has been called.

  DISALLOW_COPY_AND_ASSIGN(FlatRulesetIndexer);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_FLAT_RULESET_INDEXER_H_
