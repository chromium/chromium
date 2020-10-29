// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SCHEMEFUL_SITE_H_
#define NET_BASE_SCHEMEFUL_SITE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "net/base/net_export.h"
#include "url/origin.h"

class GURL;

namespace net {

// Class which represents a scheme and etld+1 for an origin, as specified by
// https://html.spec.whatwg.org/multipage/origin.html#obtain-a-site.
class NET_EXPORT SchemefulSite {
 public:
  SchemefulSite() = default;
  explicit SchemefulSite(const url::Origin& origin);

  // Using the origin constructor is preferred as this is less efficient.
  // Should only be used if the origin for a given GURL is not readily
  // available.
  explicit SchemefulSite(const GURL& url);

  SchemefulSite(const SchemefulSite& other);
  SchemefulSite(SchemefulSite&& other);

  SchemefulSite& operator=(const SchemefulSite& other);
  SchemefulSite& operator=(SchemefulSite&& other);

  // Deserializes a string obtained from `Serialize()` to a `SchemefulSite`.
  // Returns an opaque `SchemefulSite` if the value was invalid in any way.
  static SchemefulSite Deserialize(const std::string& value);

  // Returns a serialized version of `origin_`. If the underlying origin is
  // invalid, returns an empty string. If serialization of opaque origins with
  // their associated nonce is necessary, see `SerializeWithNonce()`.
  std::string Serialize() const;

  std::string GetDebugString() const;

  bool opaque() const { return origin_.opaque(); }

  // Testing only function which allows tests to access the underlying `origin_`
  // in order to verify behavior.
  const url::Origin& GetInternalOriginForTesting() const;

  bool operator==(const SchemefulSite& other) const;

  bool operator!=(const SchemefulSite& other) const;

  bool operator<(const SchemefulSite& other) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SchemefulSiteTest, OpaqueSerialization);

  // Deserializes a string obtained from `SerializeWithNonce()` to a
  // `SchemefulSite`. Returns nullopt if the value was invalid in any way.
  static base::Optional<SchemefulSite> DeserializeWithNonce(
      const std::string& value);

  // Returns a serialized version of `origin_`. For an opaque `origin_`, this
  // serializes with the nonce.  See `url::origin::SerializeWithNonce()` for
  // usage information.
  base::Optional<std::string> SerializeWithNonce();

  // Origin which stores the result of running the steps documented at
  // https://html.spec.whatwg.org/multipage/origin.html#obtain-a-site.
  url::Origin origin_;
};

}  // namespace net

#endif  // NET_BASE_SCHEMEFUL_SITE_H_
