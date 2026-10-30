// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_DATA_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_DATA_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace extensions::declarative_net_request {

// Manifest data required for the kDeclarativeNetRequestKey manifest
// key.
class DNRManifestData : public Extension::ManifestData {
 public:
  static const char* kManifestDataKey;

  struct RulesetInfo {
    RulesetInfo();
    ~RulesetInfo();

    RulesetInfo(RulesetInfo&&);
    RulesetInfo& operator=(RulesetInfo&&);

    base::FilePath relative_path;

    // ID provided for the ruleset in the extension manifest. Uniquely
    // identifies the ruleset.
    // TODO(karandeepb): Rename to `public_id`.
    std::string manifest_id;

    // Uniquely identifies an extension ruleset. The order of rulesets within
    // the manifest defines the order for ids. In practice, this is equal to
    // kMinValidStaticRulesetID + the index of the ruleset within `rulesets_`.
    // Note: we introduce another notion of a ruleset ID in addition to
    // `manifest_id` since the id is also used as an input to preference keys
    // and indexed ruleset file paths, and integral IDs are easier to reason
    // about here. E.g. a string ID can have invalid file path characters.
    RulesetID id;

    // Whether the ruleset is enabled by default. Note that this value
    // corresponds to the one specified in the extension manifest. Extensions
    // may further dynamically toggle whether a ruleset is enabled or not.
    bool enabled = false;
  };

  explicit DNRManifestData(std::vector<RulesetInfo> rulesets);

  DNRManifestData(const DNRManifestData&) = delete;
  DNRManifestData& operator=(const DNRManifestData&) = delete;

  ~DNRManifestData() override;

  // Returns the RulesetInfo for the `extension`. For an extension, which didn't
  // specify a static ruleset, an empty span is returned.
  static base::span<const RulesetInfo> GetRulesets(
      const Extension& extension LIFETIME_BOUND);

  // Returns the RulesetInfo corresponding to the given `ruleset_id`. Must only
  // be called for a valid `ruleset_id`.
  static const RulesetInfo& GetRuleset(const Extension& extension
                                           LIFETIME_BOUND,
                                       RulesetID ruleset_id);

  // Returns the RulesetInfo corresponding to `manifest_id`, or nullptr if no
  // matching ruleset exists.
  const RulesetInfo* FindRulesetByManifestID(std::string_view manifest_id) const
      LIFETIME_BOUND;

  // Returns the RulesetInfo corresponding to `manifest_id` for `extension`, or
  // nullptr if the extension has no DNR manifest data or no matching ruleset.
  static const RulesetInfo* FindRulesetByManifestID(
      const Extension& extension LIFETIME_BOUND,
      std::string_view manifest_id);

 private:
  // Map from the manifest ID to the corresponding RulesetInfo.
  // The keys are `std::string_view`s since they are backed by the manifest ID
  // strings in the `rulesets_` vector stored within the same class, so they
  // share the same lifetime.
  using ManifestIDToRulesetMap =
      absl::flat_hash_map<std::string_view,
                          raw_ptr<const RulesetInfo, CtnExperimental>>;

  // Static rulesets specified by the extension in its manifest, in the order in
  // which they were specified.
  std::vector<RulesetInfo> rulesets_;

  ManifestIDToRulesetMap manifest_id_to_ruleset_map_;
};

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_DATA_H_
