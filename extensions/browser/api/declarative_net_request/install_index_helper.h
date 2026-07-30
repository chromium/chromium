// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INSTALL_INDEX_HELPER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INSTALL_INDEX_HELPER_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"

namespace extensions {
class Extension;
struct RulesetParseResult;

namespace declarative_net_request {

// A class to help in indexing multiple rulesets at install time.
class InstallIndexHelper
    : public base::RefCountedThreadSafe<InstallIndexHelper> {
 public:
  using IndexCallback = base::OnceCallback<void(RulesetParseResult)>;

  // Indexes the static rulesets for an extension. Must be called on a sequence
  // which supports file IO. The `callback` will be dispatched to the same
  // sequence on which IndexStaticRulesets() is called.
  static void IndexStaticRulesets(
      const Extension& extension,
      FileBackedRulesetSource::RulesetFilter ruleset_filter,
      uint8_t parse_flags,
      IndexCallback callback);

  // Reads the Declarative Net Request JSON rulesets for the extension, if it
  // provided any, and persists the indexed rulesets, returning the result on
  // completion. Must be called on a sequence where file IO is allowed.
  static RulesetParseResult IndexAndPersistRulesOnInstall(
      const Extension& extension);

 private:
  friend class base::RefCountedThreadSafe<InstallIndexHelper>;

  using IndexResults = std::vector<std::pair<const FileBackedRulesetSource*,
                                             IndexAndPersistJSONRulesetResult>>;

  InstallIndexHelper(std::vector<FileBackedRulesetSource> sources,
                     uint8_t parse_flags,
                     bool log_histograms,
                     IndexCallback callback);
  ~InstallIndexHelper();

  // Indexes `source` and appends the result to `results`. Returns true if
  // indexing should continue, or false if a fatal error occurred.
  static bool IndexRuleset(const FileBackedRulesetSource& source,
                           uint8_t parse_flags,
                           IndexResults& results);

  void Start(size_t start_index = 0);
  void OnIndexingFinished();

  const std::vector<FileBackedRulesetSource> sources_;
  const uint8_t parse_flags_;
  const bool log_histograms_;
  IndexCallback callback_;

  IndexResults results_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INSTALL_INDEX_HELPER_H_
