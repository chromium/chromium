// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_LIST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_LIST_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "services/network/public/cpp/cors/origin_access_entry.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "url/origin.h"

namespace network {

namespace cors {

// A class to manage origin access allow / block lists. If these lists conflict,
// blacklisting is respected. These lists are managed per source-origin basis.
class COMPONENT_EXPORT(NETWORK_CPP) OriginAccessList {
 public:
  OriginAccessList();
  ~OriginAccessList();

  // Clears the old allow list for |source_origin|, and set |patterns| to the
  // allow list.
  void SetAllowListForOrigin(
      const url::Origin& source_origin,
      const std::vector<mojom::CorsOriginPatternPtr>& patterns);

  // Adds a matching pattern for |protocol|, |domain|, and |allow_subdomains|
  // to the allow list. When two or more entries in a list match the entry
  // with the higher |priority| takes precedence.
  void AddAllowListEntryForOrigin(
      const url::Origin& source_origin,
      const std::string& protocol,
      const std::string& domain,
      bool allow_subdomains,
      const network::mojom::CORSOriginAccessMatchPriority priority);

  // Clears the old allow list.
  void ClearAllowList();

  // Clears the old block list for |source_origin| and set |patterns| to the
  // block list.
  void SetBlockListForOrigin(
      const url::Origin& source_origin,
      const std::vector<mojom::CorsOriginPatternPtr>& patterns);

  // Adds a matching pattern for |protocol|, |domain|, and |allow_subdomains|
  // to the block list. When two or more entries in a list match the entry
  // with the higher |priority| takes precedence.
  void AddBlockListEntryForOrigin(
      const url::Origin& source_origin,
      const std::string& protocol,
      const std::string& domain,
      bool allow_subdomains,
      const network::mojom::CORSOriginAccessMatchPriority priority);

  // Clears the old block list.
  void ClearBlockList();

  // Returns true if |destination| is in the allow list, and not in the block
  // list of the |source_origin|.
  bool IsAllowed(const url::Origin& source_origin,
                 const GURL& destination) const;

 private:
  using Patterns = std::vector<OriginAccessEntry>;
  using PatternMap = std::map<std::string, Patterns>;

  static void SetForOrigin(
      const url::Origin& source_origin,
      const std::vector<mojom::CorsOriginPatternPtr>& patterns,
      PatternMap* map,
      const network::mojom::CORSOriginAccessMatchPriority priority);
  static void AddForOrigin(
      const url::Origin& source_origin,
      const std::string& protocol,
      const std::string& domain,
      bool allow_subdomains,
      PatternMap* map,
      const network::mojom::CORSOriginAccessMatchPriority priority);
  static network::mojom::CORSOriginAccessMatchPriority
  GetHighestPriorityOfRuleForOrigin(const std::string& source,
                                    const url::Origin& destination_origin,
                                    const PatternMap& map);

  PatternMap allow_list_;
  PatternMap block_list_;

  DISALLOW_COPY_AND_ASSIGN(OriginAccessList);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_ORIGIN_ACCESS_LIST_H_
