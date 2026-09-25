// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FIRST_PARTY_SETS_HANDLER_H_
#define CONTENT_PUBLIC_BROWSER_FIRST_PARTY_SETS_HANDLER_H_

#include <optional>
#include <set>
#include <string>
#include <variant>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "base/version.h"
#include "content/common/content_export.h"

namespace net {
class FirstPartySetEntry;
class FirstPartySetMetadata;
class SchemefulSite;
}  // namespace net

namespace content {

// The FirstPartySetsHandler class allows an embedder to provide
// First-Party Sets inputs from custom sources.
class CONTENT_EXPORT FirstPartySetsHandler {
 public:
  enum class ParseErrorType {
    // The set definition was not the correct data type.
    kInvalidType,
    // A string in the set could not be parsed as a URL or origin.
    kInvalidOrigin,
    // An origin in the set was specified using a scheme other than HTTPS.
    kNonHttpsScheme,
    // A origin in the set did not use a valid a registrable domain.
    kInvalidDomain,
    // The set had no members.
    kSingletonSet,
    // The set was non-disjoint with other pre-existing sets.
    kNonDisjointSets,
    // The set repeated the same domain more than once in its definition.
    kRepeatedDomain,
  };

  enum class ParseWarningType {
    // The "ccTLDs" map has a key that isn't a canonical site (present in the
    // set).
    kCctldKeyNotCanonical,
    // The "ccTLDs" maps a site that differs from its canonical key beyond just
    // TLD.
    kAliasNotCctldVariant,
  };

  // Contains metadata about an <T> issue for later use in outputting
  // descriptive messages about the issue.
  template <typename T>
  class IssueWithMetadata {
   public:
    IssueWithMetadata(
        const T& issue_type,
        const std::vector<std::variant<int, std::string>>& issue_path)
        : issue_type_(issue_type), issue_path_(issue_path) {}
    ~IssueWithMetadata() = default;
    IssueWithMetadata(const IssueWithMetadata<T>&) = default;

    bool operator==(const IssueWithMetadata<T>& other) const = default;

    // Inserts path_prefix at the beginning of the path stored for this issue.
    void PrependPath(
        const std::vector<std::variant<int, std::string>>& path_prefix) {
      issue_path_.insert(issue_path_.begin(), path_prefix.begin(),
                         path_prefix.end());
    }

    // The type of issue that was found when parsing the policy sets.
    T type() const { return issue_type_; }

    // The path within the policy that was being parsed when the issue was
    // found. Based on the policy::PolicyErrorPath type defined in
    // components/policy.
    std::vector<std::variant<int, std::string>> path() const {
      return issue_path_;
    }

   private:
    T issue_type_;
    std::vector<std::variant<int, std::string>> issue_path_;
  };

  using ParseError = IssueWithMetadata<ParseErrorType>;
  using ParseWarning = IssueWithMetadata<ParseWarningType>;

  virtual ~FirstPartySetsHandler() = default;

  // Overrides the singleton with caller-owned |test_instance|. Callers in tests
  // are responsible for resetting this to null on cleanup.
  static void SetInstanceForTesting(FirstPartySetsHandler* test_instance);

  // Returns the singleton instance.
  static FirstPartySetsHandler* GetInstance();

  // Returns whether First-Party Sets is enabled.
  //
  // Embedders can use this method to guard First-Party Sets related changes.
  virtual bool IsEnabled() const = 0;

  // Sets the First-Party Sets data from `sets_file` to initialize the
  // FirstPartySets instance. `sets_file` is expected to contain a sequence of
  // newline-delimited JSON records. Each record is a set declaration in the
  // format specified here: https://github.com/privacycg/first-party-sets.
  // `version` is the version of the sets file used for comparison. If the
  // file changed between browser runs, those files must have different
  // associated `base::Version`.
  //
  // Embedder should call this method as early as possible during browser
  // startup if First-Party Sets are enabled, since no First-Party Sets queries
  // are answered until initialization is complete.
  //
  // If this is called when First-Party Sets are enabled, or the embedder has
  // not indicated it will provide the public First-Party Sets, the call is
  // ignored.
  //
  // If this is called more than once, all but the first call are ignored.
  virtual void SetPublicFirstPartySets(const base::Version& version,
                                       base::File sets_file) = 0;

  // Looks up `site` in the global First-Party Sets and `config` to find its
  // associated FirstPartySetEntry.
  //
  // This will return nullopt if:
  // - First-Party Sets is disabled or
  // - the list of First-Party Sets isn't initialized yet or
  // - `site` isn't in the global First-Party Sets
  virtual std::optional<net::FirstPartySetEntry> FindEntry(
      const net::SchemefulSite& site) const = 0;

  // Returns whether the global First-Party Sets data is already fully
  // initialized. Returns true if the data is already available, or false and
  // asynchronously invokes `callback` if the data is not ready yet.
  //
  // If `callback` is null, it will not be invoked, even if the First-Party Sets
  // data is not ready yet.
  //
  // If First-Party Sets is disabled, this returns true.
  [[nodiscard]] virtual bool WhenInitComplete(base::OnceClosure callback) = 0;

  // Computes the First-Party Set metadata related to the given request context,
  // and invokes `callback` with the result.
  //
  // This may invoke `callback` synchronously.
  virtual void ComputeFirstPartySetMetadata(
      const net::SchemefulSite& site,
      base::optional_ref<const net::SchemefulSite> top_frame_site,
      base::OnceCallback<void(net::FirstPartySetMetadata)> callback) = 0;

  // Synchronously iterates over all the effective entries (i.e. anything that
  // could be returned by `FindEntry` given the global First-Party Sets), and
  // invokes `f` on each entry formed as a net::SchemefulSite and a
  // net::FirstPartySetEntry. If any of these invocations returns false, then
  // ForEachEffectiveSetEntry stops iterating over the entries and returns false
  // to its caller. Otherwise, if each call to `f` returns true, then
  // ForEachEffectiveSetEntry returns true.
  //
  // Also returns false if First-Party Sets was not yet initialized. No
  // guarantees are made re: iteration order.
  virtual bool ForEachEffectiveSetEntry(
      base::FunctionRef<bool(const net::SchemefulSite&,
                             const net::FirstPartySetEntry&)> f) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FIRST_PARTY_SETS_HANDLER_H_
