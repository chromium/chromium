// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NO_VARY_SEARCH_DATA_H_
#define NET_HTTP_HTTP_NO_VARY_SEARCH_DATA_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "net/base/net_export.h"
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
    kOk,             // Parsing is correct. Also returned if there is no header.
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

  static HttpNoVarySearchData CreateFromNoVaryParams(
      const std::vector<std::string>& no_vary_params,
      bool vary_on_key_order);
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

  bool AreEquivalent(const GURL& a, const GURL& b) const;

  const base::flat_set<std::string>& no_vary_params() const;
  const base::flat_set<std::string>& vary_params() const;
  bool vary_on_key_order() const;
  bool vary_by_default() const;

 private:
  HttpNoVarySearchData();

  static base::expected<HttpNoVarySearchData,
                        HttpNoVarySearchData::ParseErrorEnum>
  ParseNoVarySearchDictionary(const structured_headers::Dictionary& dict);

  // Query parameters which should be ignored when comparing a request
  // to a cached response. This is empty if |vary_by_default_| is false.
  base::flat_set<std::string> no_vary_params_;
  // Query parameters which should be respected when comparing a request
  // to a cached response, even if |vary_by_default_| is false. This is empty
  // if |vary_by_default_| is true.
  base::flat_set<std::string> vary_params_;
  // If false, parameters with distinct keys can be reordered in order to find a
  // cache hit.
  bool vary_on_key_order_ = true;
  // If true, all parameters are significant except those in |no_vary_params_|.
  // If false, only parameters in |vary_params_| are significant.
  bool vary_by_default_ = true;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NO_VARY_SEARCH_DATA_H_
