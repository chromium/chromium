// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_URL_SEARCH_PARAMS_H_
#define NET_BASE_URL_SEARCH_PARAMS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "net/base/net_export.h"
#include "url/gurl.h"

namespace net {

// Class that exposes the following functionality to parse a UTF-8, percent
// encoded url's `query`
//  - parse `query` parameters into a list of `(key,value)` pairs keeping the
//    same order as in `query`. While parsing the url's `query` the class does
//    percent decoding of both the `key` and `value`.
//  - stable sort of the `(key,value)` entries in the list based on `key`
//  - deletion of all `(key,value)` pairs for which `key`is part of a set of
//    specified `keys`
//  - deletion of all `(key, values)` pairs except pairs for which `key` is part
//    of a set of specified `keys`
class NET_EXPORT UrlSearchParams {
 public:
  explicit UrlSearchParams(const GURL& url);
  UrlSearchParams(const UrlSearchParams&) = delete;
  ~UrlSearchParams();
  UrlSearchParams& operator=(UrlSearchParams&) = delete;
  UrlSearchParams& operator=(const UrlSearchParams&) = delete;
  // Runs a stable sort by key of all of the query search params.
  // The stable sort will keep the order of query search params with the same
  // key the same as in the original url.
  void Sort();
  // Deletes all query search params with specified keys.
  void DeleteAllWithNames(const base::flat_set<std::string>& names);
  // Deletes all query search params except the ones with specified keys.
  void DeleteAllExceptWithNames(const base::flat_set<std::string>& names);
  // Returns a list of key-value pairs representing all query search params.
  const std::vector<std::pair<std::string, std::string>>& params() const;

 private:
  // Keeps track of all key-value pairs representing all query search params.
  // The order from the original url is important.
  std::vector<std::pair<std::string, std::string>> params_;
};

}  // namespace net
#endif  // NET_BASE_URL_SEARCH_PARAMS_H_
