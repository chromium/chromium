// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FIRST_PARTY_SETS_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_FIRST_PARTY_SETS_HANDLER_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class FirstPartySetEntry;
}

namespace content {

// The FirstPartySetsHandler class allows an embedder to provide
// First-Party Sets inputs from custom sources.
class CONTENT_EXPORT FirstPartySetsHandler {
 public:
  enum class ParseError {
    // The set definition was not the correct data type.
    kInvalidType,
    // A string in the set was not a registrable domain.
    kInvalidOrigin,
    // The set had no members.
    kSingletonSet,
    // The set was non-disjoint with other pre-existing sets.
    kNonDisjointSets,
    // The set repeated the same domain more than once in its definition.
    kRepeatedDomain,
  };

  enum class PolicySetType { kReplacement, kAddition };

  struct CONTENT_EXPORT PolicyParsingError {
    bool operator==(const PolicyParsingError& other) const;

    // The kind of error that was found when parsing the policy sets.
    ParseError error;
    // The field of the policy that was being parsed when the error was found.
    PolicySetType set_type;
    // The index of the set in the 'set_type' list where the error was found.
    int error_index;
  };

  // The keys are member sites and the values are their entries in the final
  // list of First-Party Sets that result from combining the public sets and
  // the per-profile Overrides policy. Entries of site -> absl::nullopt means
  // the key site is considered deleted from the existing First-Party Sets.
  using PolicyCustomization =
      base::flat_map<net::SchemefulSite,
                     absl::optional<net::FirstPartySetEntry>>;

  virtual ~FirstPartySetsHandler() = default;

  // Returns the singleton instance.
  static FirstPartySetsHandler* GetInstance();

  // Validates the First-Party Sets Overrides enterprise policy in `policy`,
  // and may return an error containing information about why the policy is
  // invalid.
  //
  // This validation only checks that all sets in this policy are valid
  // First-Party Sets and disjoint from each other. It doesn't require
  // disjointness with other sources, such as the public sets, since this policy
  // will be used override First-Party Sets in those sources.
  static absl::optional<PolicyParsingError> ValidateEnterprisePolicy(
      const base::Value::Dict& policy);

  // Returns whether First-Party Sets is enabled.
  //
  // Embedders can use this method to guard First-Party Sets related changes.
  virtual bool IsEnabled() const = 0;

  // Sets the First-Party Sets data from `sets_file` to initialize the
  // FirstPartySets instance. `sets_file` is expected to contain a sequence of
  // newline-delimited JSON records. Each record is a set declaration in the
  // format specified here: https://github.com/privacycg/first-party-sets.
  //
  // Embedder should call this method as early as possible during browser
  // startup if First-Party Sets are enabled, since no First-Party Sets queries
  // are answered until initialization is complete. Must not be called if
  // `ContentBrowserClient::WillProvidePublicFirstPartySets` returns false or
  // `ContentBrowserClient::IsFrstpartySetsEnabled` returns false.
  //
  // Must be called at most once.
  virtual void SetPublicFirstPartySets(base::File sets_file) = 0;

  // Resets the state on the instance for testing.
  virtual void ResetForTesting() = 0;

  // Computes a representation of the changes that need to be made to the
  // browser's list of First-Party Sets to respect the `policy` value of the
  // First-Party Sets Overrides enterprise policy.
  //
  // The customization is will be returned via `callback` since the
  // customization must be computed after the list of First-Party Sets is
  // initialized, which occurs asynchronously.
  virtual void GetCustomizationForPolicy(
      const base::Value::Dict& policy,
      base::OnceCallback<void(PolicyCustomization)> callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FIRST_PARTY_SETS_HANDLER_H_
