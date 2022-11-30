// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INSTALL_INDEX_HELPER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INSTALL_INDEX_HELPER_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/ruleset_install_pref.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/common/install_warning.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {
class Extension;
namespace declarative_net_request {

// A class to help in indexing multiple rulesets at install time.
class InstallIndexHelper
    : public base::RefCountedThreadSafe<InstallIndexHelper> {
 public:
  struct Result {
    Result();
    ~Result();
    Result(Result&&);
    Result& operator=(Result&&);

    // Non-empty on failure.
    absl::optional<std::string> error;

    // Valid if |error| is absl::nullopt. Clients should not use these fields in
    // case of a failure since these may be partially populated.
    std::vector<InstallWarning> warnings;
    std::vector<RulesetInstallPref> ruleset_install_prefs;
  };

  // Starts indexing rulesets. Must be called on a sequence which supports file
  // IO. The |callback| will be dispatched to the same sequence on which
  // IndexStaticRulesets() is called.
  using IndexCallback = base::OnceCallback<void(Result result)>;
  static void IndexStaticRulesets(
      const Extension& extension,
      FileBackedRulesetSource::RulesetFilter ruleset_filter,
      uint8_t parse_flags,
      IndexCallback callback);

  // Synchronously indexes the static rulesets for an extension. Must be called
  // on a sequence which supports file IO. This is potentially unsafe since this
  // parses JSON in-process.
  static Result IndexStaticRulesetsUnsafe(
      const Extension& extension,
      FileBackedRulesetSource::RulesetFilter ruleset_filter,
      uint8_t parse_flags);

 private:
  friend class base::RefCountedThreadSafe<InstallIndexHelper>;
  using IndexResults = std::vector<std::pair<const FileBackedRulesetSource*,
                                             IndexAndPersistJSONRulesetResult>>;

  InstallIndexHelper(std::vector<FileBackedRulesetSource> sources,
                     IndexCallback callback);
  ~InstallIndexHelper();

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
