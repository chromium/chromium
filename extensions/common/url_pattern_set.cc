// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/url_pattern_set.h"

#include <iterator>
#include <ostream>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/url_pattern.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

const char kInvalidURLPatternError[] = "Invalid url pattern '*'";

}  // namespace

// static
URLPatternSet URLPatternSet::CreateDifference(const URLPatternSet& set1,
                                              const URLPatternSet& set2) {
  return URLPatternSet(base::STLSetDifference<std::set<URLPattern>>(
      set1.patterns_, set2.patterns_));
}

// static
URLPatternSet URLPatternSet::CreateIntersection(
    const URLPatternSet& set1,
    const URLPatternSet& set2,
    IntersectionBehavior intersection_behavior) {
  // Note: leverage return value optimization; always return the same object.
  URLPatternSet result;

  if (intersection_behavior == IntersectionBehavior::kStringComparison) {
    // String comparison just relies on STL set behavior, which looks at the
    // string representation.
    result = URLPatternSet(base::STLSetIntersection<std::set<URLPattern>>(
        set1.patterns_, set2.patterns_));
    return result;
  }

  // Look for a semantic intersection.

  // Step 1: Iterate over each set. Find any patterns that are completely
  // contained by the other (thus being necessarily present in any intersection)
  // and add them, collecting the others in a set of unique items.
  // Note: Use a collection of pointers for the uniques to avoid excessive
  // copies. Since these are owned by the URLPatternSet passed in, which is
  // const, this should be safe.
  std::vector<const URLPattern*> unique_set1;
  for (const URLPattern& pattern : set1) {
    if (set2.ContainsPattern(pattern)) {
      result.patterns_.insert(pattern);
    } else {
      unique_set1.push_back(&pattern);
    }
  }
  std::vector<const URLPattern*> unique_set2;
  for (const URLPattern& pattern : set2) {
    if (set1.ContainsPattern(pattern)) {
      result.patterns_.insert(pattern);
    } else {
      unique_set2.push_back(&pattern);
    }
  }

  // If we're just looking for patterns contained by both, we're done.
  if (intersection_behavior == IntersectionBehavior::kPatternsContainedByBoth) {
    return result;
  }

  DCHECK_EQ(IntersectionBehavior::kDetailed, intersection_behavior);

  // Step 2: Iterate over all the unique patterns and find the intersections
  // they have with the other patterns.
  for (const auto* pattern : unique_set1) {
    for (const auto* pattern2 : unique_set2) {
      std::optional<URLPattern> intersection =
          pattern->CreateIntersection(*pattern2);
      if (intersection) {
        result.patterns_.insert(std::move(*intersection));
      }
    }
  }

  return result;
}

// static
URLPatternSet URLPatternSet::CreateUnion(const URLPatternSet& set1,
                                         const URLPatternSet& set2) {
  return URLPatternSet(
      base::STLSetUnion<std::set<URLPattern>>(set1.patterns_, set2.patterns_));
}

URLPatternSet::URLPatternSet() = default;

URLPatternSet::URLPatternSet(URLPatternSet&& rhs) = default;

URLPatternSet::URLPatternSet(const std::set<URLPattern>& patterns)
    : patterns_(patterns) {}

URLPatternSet::~URLPatternSet() = default;

URLPatternSet& URLPatternSet::operator=(URLPatternSet&& rhs) = default;

bool URLPatternSet::operator==(const URLPatternSet& other) const {
  return patterns_ == other.patterns_;
}

std::ostream& operator<<(std::ostream& out,
                         const URLPatternSet& url_pattern_set) {
  out << "{ ";

  auto iter = url_pattern_set.patterns().cbegin();
  if (!url_pattern_set.patterns().empty()) {
    out << *iter;
    ++iter;
  }

  for (; iter != url_pattern_set.patterns().end(); ++iter) {
    out << ", " << *iter;
  }

  if (!url_pattern_set.patterns().empty()) {
    out << " ";
  }

  out << "}";
  return out;
}

URLPatternSet URLPatternSet::Clone() const {
  return URLPatternSet(patterns_);
}

bool URLPatternSet::is_empty() const {
  return patterns_.empty();
}

size_t URLPatternSet::size() const {
  return patterns_.size();
}

bool URLPatternSet::AddPattern(const URLPattern& pattern) {
  return patterns_.insert(pattern).second;
}

void URLPatternSet::AddPatterns(const URLPatternSet& set) {
  patterns_.insert(set.patterns().begin(),
                   set.patterns().end());
}

void URLPatternSet::ClearPatterns() {
  patterns_.clear();
}

bool URLPatternSet::AddOrigin(int valid_schemes, const GURL& origin) {
  if (origin.is_empty()) {
    return false;
  }
  const url::Origin real_origin = url::Origin::Create(origin);
  DCHECK(real_origin.IsSameOriginWith(
      url::Origin::Create(origin.DeprecatedGetOriginAsURL())));
  // TODO(devlin): Implement this in terms of the `AddOrigin()` call that takes
  // an url::Origin? It's interesting because this doesn't currently supply an
  // extra path, so if the GURL has not path ("https://example.com"), it would
  // fail to add - which is probably a bug.
  URLPattern origin_pattern(valid_schemes);
  // Origin adding could fail if |origin| does not match |valid_schemes|.
  if (origin_pattern.Parse(origin.spec()) !=
      URLPattern::ParseResult::kSuccess) {
    return false;
  }
  origin_pattern.SetPath("/*");
  return AddPattern(origin_pattern);
}

bool URLPatternSet::AddOrigin(int valid_schemes, const url::Origin& origin) {
  DCHECK(!origin.opaque());
  URLPattern origin_pattern(valid_schemes);
  // Origin adding could fail if |origin| does not match |valid_schemes|.
  std::string string_pattern = origin.Serialize() + "/*";
  if (origin_pattern.Parse(string_pattern) !=
      URLPattern::ParseResult::kSuccess) {
    return false;
  }
  return AddPattern(origin_pattern);
}

bool URLPatternSet::Contains(const URLPatternSet& other) const {
  for (const auto& it : other) {
    if (!ContainsPattern(it)) {
      return false;
    }
  }

  return true;
}

bool URLPatternSet::ContainsPattern(const URLPattern& pattern) const {
  for (const auto& it : *this) {
    if (it.Contains(pattern)) {
      return true;
    }
  }
  return false;
}

bool URLPatternSet::MatchesURL(const GURL& url) const {
  for (const auto& pattern : patterns_) {
    if (pattern.MatchesURL(url)) {
      return true;
    }
  }

  return false;
}

bool URLPatternSet::MatchesAllURLs() const {
  for (const auto& host : *this) {
    if (host.match_all_urls() ||
        (host.match_subdomains() && host.host().empty())) {
      return true;
    }
  }
  return false;
}

bool URLPatternSet::MatchesHost(const GURL& test,
                                bool require_match_subdomains) const {
  if (!test.is_valid()) {
    return false;
  }

  return std::any_of(
      patterns_.begin(), patterns_.end(),
      [&test, require_match_subdomains](const URLPattern& pattern) {
        return pattern.MatchesHost(test) &&
               (!require_match_subdomains || pattern.match_subdomains());
      });
}

bool URLPatternSet::MatchesSecurityOrigin(const GURL& origin) const {
  for (const auto& pattern : patterns_) {
    if (pattern.MatchesSecurityOrigin(origin)) {
      return true;
    }
  }

  return false;
}

bool URLPatternSet::OverlapsWith(const URLPatternSet& other) const {
  // Two extension extents overlap if there is any one URL that would match at
  // least one pattern in each of the extents.
  for (const auto& pattern : patterns_) {
    for (const auto& j : other.patterns()) {
      if (pattern.OverlapsWith(j)) {
        return true;
      }
    }
  }

  return false;
}

base::Value::List URLPatternSet::ToValue() const {
  base::Value::List result;
  for (const auto& pattern : patterns_) {
    base::Value pattern_str_value(pattern.GetAsString());
    if (!base::Contains(result, pattern_str_value)) {
      result.Append(std::move(pattern_str_value));
    }
  }
  return result;
}

bool URLPatternSet::Populate(const std::vector<std::string>& patterns,
                             int valid_schemes,
                             bool allow_file_access,
                             std::string* error) {
  ClearPatterns();
  for (const auto& i : patterns) {
    URLPattern pattern(valid_schemes);
    if (pattern.Parse(i) != URLPattern::ParseResult::kSuccess) {
      if (error) {
        *error = ErrorUtils::FormatErrorMessage(kInvalidURLPatternError, i);
      } else {
        LOG(ERROR) << "Invalid url pattern: " << i;
      }
      return false;
    }
    if (!allow_file_access && pattern.MatchesScheme(url::kFileScheme)) {
      pattern.SetValidSchemes(
          pattern.valid_schemes() & ~URLPattern::SCHEME_FILE);
    }
    AddPattern(pattern);
  }
  return true;
}

std::vector<std::string> URLPatternSet::ToStringVector() const {
  std::vector<std::string> result;
  for (const auto& pattern : patterns_) {
    result.push_back(pattern.GetAsString());
  }
  return result;
}

bool URLPatternSet::Populate(const base::Value::List& value,
                             int valid_schemes,
                             bool allow_file_access,
                             std::string* error) {
  std::vector<std::string> patterns;
  for (const base::Value& pattern : value) {
    const std::string* item = pattern.GetIfString();
    if (!item) {
      return false;
    }
    patterns.push_back(*item);
  }
  return Populate(patterns, valid_schemes, allow_file_access, error);
}

}  // namespace extensions
