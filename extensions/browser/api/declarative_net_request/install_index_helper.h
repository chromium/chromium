// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INSTALL_INDEX_HELPER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INSTALL_INDEX_HELPER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/types/expected.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace extensions {
class Extension;
struct RulesetParseResult;
namespace declarative_net_request {

// A class to help in indexing multiple rulesets at install time.
class InstallIndexHelper
    : public base::RefCountedThreadSafe<InstallIndexHelper> {
 public:
  // Starts indexing rulesets. Must be called on a sequence which supports file
  // IO. The |callback| will be dispatched to the same sequence on which
  // IndexStaticRulesets() is called.
  using IndexCallback = base::OnceCallback<void(RulesetParseResult result)>;
  static void IndexStaticRulesets(
      const Extension& extension,
      FileBackedRulesetSource::RulesetFilter ruleset_filter,
      uint8_t parse_flags,
      IndexCallback callback);

  // Reads the Declarative Net Request JSON rulesets for the extension, if it
  // provided any, and persists the indexed rulesets. Returns the ruleset
  // install prefs on success and an error on failure.
  // Must be called on a sequence where file IO is allowed.
  static base::expected<base::Value::Dict, std::string>
  IndexAndPersistRulesOnInstall(Extension& extension);

 private:
  friend class base::RefCountedThreadSafe<InstallIndexHelper>;
  using IndexResults = std::vector<std::pair<const FileBackedRulesetSource*,
                                             IndexAndPersistJSONRulesetResult>>;

  InstallIndexHelper(std::vector<FileBackedRulesetSource> sources,
                     IndexCallback callback);
  ~InstallIndexHelper();

  // Synchronously indexes the static rulesets for an extension. Must be called
  // on a sequence which supports file IO. This is potentially unsafe since this
  // parses JSON in-process.
  static RulesetParseResult IndexStaticRulesetsUnsafe(
      const Extension& extension,
      FileBackedRulesetSource::RulesetFilter ruleset_filter,
      uint8_t parse_flags);

  // Starts indexing the rulesets.
  void Start(uint8_t parse_flags);

  // Callback invoked when indexing of all rulesets is completed.
  void OnAllRulesetsIndexed();

  // Callback invoked when indexing of a single ruleset is completed.
  // |source_index| is the index of the FileBackedRulesetSource within
  // |sources_|.
  void OnRulesetIndexed(base::OnceClosure ruleset_done_closure,
                        size_t source_index,
                        IndexAndPersistJSONRulesetResult result);

  const std::vector<FileBackedRulesetSource> sources_;
  IndexCallback callback_;

  // Stores the result for each FileBackedRulesetSource in |sources_|.
  IndexResults results_;

  // We use a single shared Data Decoder service instance to process all of the
  // rulesets for this InstallIndexHelper.
  data_decoder::DataDecoder decoder_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INSTALL_INDEX_HELPER_H_
