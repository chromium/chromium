// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_URL_SEARCH_PARAMS_VIEW_H_
#define NET_BASE_URL_SEARCH_PARAMS_VIEW_H_

#include <stddef.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "url/gurl.h"

namespace net {

// Class that exposes the following functionality to parse a UTF-8, percent
// encoded url's `query`, without copying the strings.
//  - parse `query` parameters into a list of `(key,value)` pairs keeping the
//    same order as in `query`.
//  - stable sort of the `(key,value)` entries in the list based on the
//    URL-decoded value of `key`.
//  - deletion of all `(key,value)` pairs for which the URL-decoded value of
//    `key` is part of a set of specified `keys`
//  - deletion of all `(key, values)` pairs except pairs for which the
//    URL-decoded value of `key` is part of a set of specified `keys`.
//
// This class avoids allocating memory where possible and uses lazy decoding of
// values for efficiency.
//
// This is similar to the class UrlSearchParams, but unlike that class doesn't
// take ownership of parameter values. As a result, this version is more
// efficient, but not suitable for long-term storage.
class NET_EXPORT UrlSearchParamsView final {
 public:
  // This object retains a reference to the query part of `url`, so should be
  // destroyed before `url` is destroyed or modified. The LIFETIME_BOUND
  // annotation permits the compiler to catch obvious mistakes like constructing
  // the object from a temporary GURL.
  explicit UrlSearchParamsView(const GURL& url LIFETIME_BOUND);

  UrlSearchParamsView(const UrlSearchParamsView&) = delete;
  ~UrlSearchParamsView();
  UrlSearchParamsView& operator=(const UrlSearchParamsView&) = delete;

  // Returns true if all the keys and values in `other` are the same as in this
  // object and in the same order.
  bool operator==(const UrlSearchParamsView& other) const;

  // Runs a stable sort by URL-decoded key of all of the query search params.
  // The stable sort will keep the order of query search params with the same
  // key the same as in the original url.
  void Sort();

  // Deletes all query search params whose keys after URL-decoding match those
  // in `names`.
  void DeleteAllWithNames(const base::flat_set<std::string>& names);

  // Deletes all query search params except the ones whose keys after
  // URL-decoding match those in `names`.
  void DeleteAllExceptWithNames(const base::flat_set<std::string>& names);

  // Returns a serialized version of the query (not including the "?"), as a
  // UTF-8 string. To save memory, only a small number of characters are
  // %-escaped. In particular, top-bit-set characters are not %-escaped, so this
  // is not directly valid in a URL, although GURL can parse and canonicalize it
  // correctly. The output has the important property that `a.SerializeAsUtf8()
  // == b.SerializeAsUtf8()` if and only if `a == b`, which allows it to be used
  // as a hash key.
  std::string SerializeAsUtf8() const;

  // Return a vector of name, value pairs. Not at all efficient; only for
  // testing purposes.
  std::vector<std::pair<std::string, std::string>> GetDecodedParamsForTesting()
      const;

 private:
  // The number of params to store inline in this object before allocating heap
  // memory.
  static constexpr size_t kInlineParamCount = 16u;

  struct KeyValue {
    // The key is stored unescaped, as it needs to be read multiple times, and
    // is often short enough for the short-string optimization to apply.
    std::string unescaped_key;

    // The value is stored escaped, as it is only read 0 or 1 times, and often
    // too long for the short-string optimization to apply.
    std::string_view escaped_value;

    // Checks the the `unescaped_key` and `escaped_value` match after
    // unescaping.
    bool operator==(const KeyValue& other) const;
  };

  // Estimates the size of the return value of SerializeAsUtf8(). Precisely
  // measuring the size of the output string would be costly. Allocating the
  // maximum possible size would be wasteful. This gives a cheap estimate which
  // is good enough to get the string to about the right size and limit the
  // number of resizes that need to be performed.
  size_t EstimateSerializedOutputSize() const;

  // Keeps track of all key-value pairs representing all query search params.
  // The order from the original url is important.
  absl::InlinedVector<KeyValue, kInlineParamCount> params_;
};

}  // namespace net

#endif  // NET_BASE_URL_SEARCH_PARAMS_VIEW_H_
