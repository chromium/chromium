// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NO_VARY_SEARCH_DATA_H_
#define NET_HTTP_HTTP_NO_VARY_SEARCH_DATA_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "net/base/net_export.h"
#include "net/base/pickle_traits.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"

namespace net {

class HttpResponseHeaders;

// Data extracted from No-Vary-Search header.
//
// This data can be used to determine which parts of the URL search
// can be ignored when comparing a request with a cached response.
class NET_EXPORT_PRIVATE HttpNoVarySearchData {
 public:
  enum class ParseErrorEnum {
    kOk,             // There is no No-Vary-Search header.
    kDefaultValue,   // Parsing is correct but led to default value - the header
                     // could be removed.
    kNotDictionary,  // Header value is not a dictionary.
    kUnknownDictionaryKey,     // Header value contains unknown dictionary keys.
    kNonBooleanKeyOrder,       // `key-order` is non-boolean.
    kParamsNotStringList,      // `params` is not a string list.
    kExceptNotStringList,      // `expect` is not a string list.
    kExceptWithoutTrueParams,  // `expect` specified without params set to true.
  };
  HttpNoVarySearchData(const HttpNoVarySearchData&);
  HttpNoVarySearchData(HttpNoVarySearchData&&);
  ~HttpNoVarySearchData();
  HttpNoVarySearchData& operator=(const HttpNoVarySearchData&);
  HttpNoVarySearchData& operator=(HttpNoVarySearchData&&);

  // Create an HttpNoVarySearchData object as if by a "No-Vary-Search:
  // params=(`no_vary_params`)" header. If `vary_on_key_order` is false it is
  // equivalent to including "key-order" in the header. Since an
  // HttpNoVarySearchData object is required to have non-default behaviour,
  // either `no_vary_params` must be non-empty or `vary_on_key_order` must be
  // false.
  static HttpNoVarySearchData CreateFromNoVaryParams(
      const std::vector<std::string>& no_vary_params,
      bool vary_on_key_order);

  // Create an HttpNoVarySearchData object as if by a "No-Vary-Search: params,
  // except=(`vary_params`)" header. If `vary_on_key_order` is false it is
  // equivalent to including "key-order" in the header.
  static HttpNoVarySearchData CreateFromVaryParams(
      const std::vector<std::string>& vary_params,
      bool vary_on_key_order);

  // Parse No-Vary-Search from response headers.
  //
  // Returns HttpNoVarySearchData if a correct No-Vary-Search header is present
  // in the response headers or a ParseErrorEnum if the No-Vary-Search header is
  // incorrect. If no No-Vary-Search is found, returns ParseErrorEnum::kOk.
  static base::expected<HttpNoVarySearchData,
                        HttpNoVarySearchData::ParseErrorEnum>
  ParseFromHeaders(const HttpResponseHeaders& response_headers);

  bool operator==(const HttpNoVarySearchData& rhs) const;

  // HttpNoVarySearchData objects can be used as a key in a map.
  std::strong_ordering operator<=>(const HttpNoVarySearchData& rhs) const;

  // Returns true if urls `a` and `b` have the same base URL and their queries
  // are equivalent according to the rules stored in this class.
  bool AreEquivalent(const GURL& a, const GURL& b) const;

  // Returns a canonicalized version of the query part of `url` based on the
  // rules stored in this class. This has the
  // property that `AreEquivalent(a, b)` is true if and only if
  // `RemoveQueryAndFragment(a) == RemoveQueryAndFragment(b)` and
  // `CanonicalizeQuery(a) == CanonicalizeQuery(b)`. The return value is a
  // UTF-8 string (not necessarily ASCII) and may end in significant whitespace.
  std::string CanonicalizeQuery(const GURL& url) const;

  // Member accessor methods.
  // TODO(crbug.com/455304285): Stop exposing internals in API.
  const base::flat_set<std::string>& affected_params() const {
    return affected_params_;
  }
  bool vary_on_key_order() const { return vary_on_key_order_; }
  bool vary_by_default() const { return vary_by_default_; }

  // Direct access to the two AreEquivalent() implementations for tests and
  // benchmarking.
  bool AreEquivalentOldImplForTesting(const GURL& a, const GURL& b) const;
  bool AreEquivalentNewImplForTesting(const GURL& a, const GURL& b) const;

 private:
  friend struct PickleTraits<HttpNoVarySearchData>;

  // Permit HttpNoVarySearchData objects to be used as keys in Abseil maps.
  template <typename H>
  friend H AbslHashValue(H h, const HttpNoVarySearchData& data) {
    return H::combine(std::move(h), data.affected_params_,
                      data.vary_on_key_order_, data.vary_by_default_);
  }

  HttpNoVarySearchData();

  static base::expected<HttpNoVarySearchData,
                        HttpNoVarySearchData::ParseErrorEnum>
  ParseNoVarySearchDictionary(const structured_headers::Dictionary& dict);

  // The old implementation of AreEquivalent() using UrlSearchParams.
  // TODO(https://crbug.com/444335347): Remove this.
  bool AreEquivalentOldImpl(const GURL& a, const GURL& b) const;

  // The new implementation of AreEquivalent() using UrlSearchParamsView.
  bool AreEquivalentNewImpl(const GURL& a, const GURL& b) const;

  // LINT.IfChange(MagicNumber)

  // Magic number for serialization. This should be updated whenever the number
  // or types of member variables are changed. This will prevent accidental
  // misinterpretation of data from a previous version.
  //
  // If member variables are added, AbslHashValue() above must also be changed.
  //
  // Generated by the command:
  //   echo "HttpNoVarySearchData version 2" | md5sum | cut -b 1-8
  static constexpr uint32_t kMagicNumber = 0xfe1056f3;

  // The params that were listed in the No-Vary-Search header value. When
  //  `vary_by_default_` is true these parameters will be ignored when
  // determining if two queries are equivalent. When `vary_by_default_` is
  // false then only these headers will be checked when determining
  // equivalence.  When `vary_by_default_` and `vary_on_key_order_` are both
  // true it is invalid for this set to be empty.
  base::flat_set<std::string> affected_params_;

  // If false, parameters with distinct keys can be reordered in order to find a
  // cache hit.
  bool vary_on_key_order_ = true;

  // If true, parameters in `affected_params_` are ignored when checking
  // equivalence. If false, only parameters in `affected_params_` are used when
  // checking equivalence.
  bool vary_by_default_ = true;

  // LINT.ThenChange(//net/http/http_no_vary_search_data.cc:Serialization)
};

// Permit WriteToPickle() and ReadValueFromPickle() to be used with
// HttpNoVarySearchData objects.
template <>
struct NET_EXPORT_PRIVATE PickleTraits<HttpNoVarySearchData> {
  static void Serialize(base::Pickle& pickle,
                        const HttpNoVarySearchData& value);

  static std::optional<HttpNoVarySearchData> Deserialize(
      base::PickleIterator& iter);

  static size_t PickleSize(const HttpNoVarySearchData& value);
};

class
    NET_EXPORT_PRIVATE ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting {
 public:
  explicit ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting(
      bool use_new_implementation);
  ~ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting();
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NO_VARY_SEARCH_DATA_H_
