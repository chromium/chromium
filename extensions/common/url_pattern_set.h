// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_URL_PATTERN_SET_H_
#define EXTENSIONS_COMMON_URL_PATTERN_SET_H_

#include <stddef.h>

#include <iosfwd>
#include <set>

#include "base/values.h"
#include "extensions/common/url_pattern.h"

class GURL;

namespace url {
class Origin;
}

namespace extensions {

// Represents the set of URLs an extension uses for web content.
class URLPatternSet {
 public:
  using const_iterator = std::set<URLPattern>::const_iterator;
  using iterator = std::set<URLPattern>::iterator;

  // Returns |set1| - |set2|.
  static URLPatternSet CreateDifference(const URLPatternSet& set1,
                                        const URLPatternSet& set2);

  enum class IntersectionBehavior {
    // For the following descriptions, consider the two URLPatternSets:
    // Set 1: {"https://example.com/*", "https://*.google.com/*", "http://*/*"}
    // Set 2: {"https://example.com/*", "https://google.com/maps",
    //         "*://chromium.org/*"}

    // Only includes patterns that are exactly in both sets. The intersection of
    // the two sets above is {"https://example.com/*"}, since that is the only
    // pattern that appears exactly in each.
    kStringComparison,

    // Includes patterns that are effectively contained by both sets. The
    // intersection of the two sets above is
    // {
    //   "https://example.com/*" (contained exactly by each set)
    //   "https://google.com/maps" (contained exactly by set 2 and a strict
    //                              subset of https://*.google.com/* in set 1)
    // }
    kPatternsContainedByBoth,

    // Includes patterns that are contained by both sets and creates new
    // patterns to represent the intersection of any others. The intersection of
    // the two sets above is
    // {
    //   "https://example.com/*" (contained exactly by each set)
    //   "https://google.com/maps" (contained exactly by set 2 and a strict
    //                              subset of https://*.google.com/* in set 1)
    //   "http://chromium.org/*" (the overlap between "http://*/*" in set 1 and
    //                            *://chromium.org/*" in set 2).
    // }
    // Note that this is the most computationally expensive - potentially
    // O(n^2) - since it can require comparing each pattern in one set to every
    // pattern in the other set.
    kDetailed,
  };

  // Returns the intersection of |set1| and |set2| according to
  // |intersection_behavior|.
  static URLPatternSet CreateIntersection(
      const URLPatternSet& set1,
      const URLPatternSet& set2,
      IntersectionBehavior intersection_behavior);

  // Returns the union of |set1| and |set2|.
  static URLPatternSet CreateUnion(const URLPatternSet& set1,
                                   const URLPatternSet& set2);

  URLPatternSet();
  URLPatternSet(URLPatternSet&& rhs);
  explicit URLPatternSet(const std::set<URLPattern>& patterns);

  URLPatternSet(const URLPatternSet&) = delete;
  URLPatternSet& operator=(const URLPatternSet&) = delete;

  ~URLPatternSet();

  URLPatternSet& operator=(URLPatternSet&& rhs);
  bool operator==(const URLPatternSet& rhs) const;

  bool is_empty() const;
  size_t size() const;
  const std::set<URLPattern>& patterns() const { return patterns_; }
  const_iterator begin() const { return patterns_.begin(); }
  const_iterator end() const { return patterns_.end(); }
  iterator erase(iterator iter) { return patterns_.erase(iter); }

  // Returns a copy of this URLPatternSet; not instrumented as a copy
  // constructor to avoid accidental/unnecessary copies.
  URLPatternSet Clone() const;

  // Adds a pattern to the set. Returns true if a new pattern was inserted,
  // false if the pattern was already in the set.
  bool AddPattern(const URLPattern& pattern);

  // Adds all patterns from |set| into this.
  void AddPatterns(const URLPatternSet& set);

  void ClearPatterns();

  // Adds a pattern based on |origin| to the set.
  bool AddOrigin(int valid_schemes, const GURL& origin);
  bool AddOrigin(int valid_schemes, const url::Origin& origin);

  // Returns true if every URL that matches |set| is matched by this. In other
  // words, if every pattern in |set| is encompassed by a pattern in this.
  bool Contains(const URLPatternSet& set) const;

  // Returns true if any pattern in this set encompasses |pattern|.
  bool ContainsPattern(const URLPattern& pattern) const;

  // Test if the extent contains a URL.
  bool MatchesURL(const GURL& url) const;

  // Test if the extent matches all URLs (for example, <all_urls>).
  bool MatchesAllURLs() const;

  // Returns true if any pattern in this set matches the host in |test|, plus
  // all subdomains of |test| if |require_match_subdomains| is true,
  bool MatchesHost(const GURL& test, bool require_match_subdomains) const;

  bool MatchesSecurityOrigin(const GURL& origin) const;

  // Returns true if there is a single URL that would be in two extents.
  bool OverlapsWith(const URLPatternSet& other) const;

  // Converts to and from Value for serialization to preferences.
  base::Value::List ToValue() const;
  bool Populate(const base::Value::List& value,
                int valid_schemes,
                bool allow_file_access,
                std::string* error);

  // Converts to and from a vector of strings.
  std::vector<std::string> ToStringVector() const;
  bool Populate(const std::vector<std::string>& patterns,
                int valid_schemes,
                bool allow_file_access,
                std::string* error);

 private:
  // The list of URL patterns that comprise the extent.
  std::set<URLPattern> patterns_;
};

std::ostream& operator<<(std::ostream& out,
                         const URLPatternSet& url_pattern_set);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_URL_PATTERN_SET_H_
