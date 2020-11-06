// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SCHEMEFUL_SITE_H_
#define NET_BASE_SCHEMEFUL_SITE_H_

#include <ostream>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "net/base/net_export.h"
#include "url/origin.h"

class GURL;

namespace network {
namespace mojom {
class SchemefulSiteDataView;
}  // namespace mojom
}  // namespace network

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo

namespace net {

// Class which represents a scheme and etld+1 for an origin, as specified by
// https://html.spec.whatwg.org/multipage/origin.html#obtain-a-site.
//
// A SchemefulSite is obtained from an input origin by normalizing, such that:
// 1. Opaque origins have distinct SchemefulSites.
// 2. http(s) origins have the same SchemefulSite iff they share a scheme, and
//    share a hostname or registrable domain.
// 3. Non-http(s) origins have the same SchemefulSite iff they share a scheme
//    and host.
// 4. Origins which differ only by port have the same SchemefulSite.
// 5. Websocket origins cannot have a SchemefulSite (they trigger a DCHECK).
class NET_EXPORT SchemefulSite {
 public:
  SchemefulSite() = default;

  // The passed `origin` may not match the resulting internal representation in
  // certain circumstances. See the comment, below, on the `site_as_origin_`
  // member.
  explicit SchemefulSite(const url::Origin& origin);

  // Using the origin constructor is preferred as this is less efficient.
  // Should only be used if the origin for a given GURL is not readily
  // available.
  explicit SchemefulSite(const GURL& url);

  SchemefulSite(const SchemefulSite& other);
  SchemefulSite(SchemefulSite&& other);

  SchemefulSite& operator=(const SchemefulSite& other);
  SchemefulSite& operator=(SchemefulSite&& other);

  // Creates a SchemefulSite iff the passed-in origin has a registerable domain.
  static base::Optional<SchemefulSite> CreateIfHasRegisterableDomain(
      const url::Origin&);

  // Deserializes a string obtained from `Serialize()` to a `SchemefulSite`.
  // Returns an opaque `SchemefulSite` if the value was invalid in any way.
  static SchemefulSite Deserialize(const std::string& value);

  // Returns a serialized version of `site_as_origin_`. If the underlying origin
  // is invalid, returns an empty string. If serialization of opaque origins
  // with their associated nonce is necessary, see `SerializeWithNonce()`.
  std::string Serialize() const;

  std::string GetDebugString() const;

  bool opaque() const { return site_as_origin_.opaque(); }

  // Testing only function which allows tests to access the underlying
  // `site_as_origin_` in order to verify behavior.
  const url::Origin& GetInternalOriginForTesting() const;

  bool operator==(const SchemefulSite& other) const;

  bool operator!=(const SchemefulSite& other) const;

  bool operator<(const SchemefulSite& other) const;

 private:
  // Mojo serialization code needs to access internal origin.
  friend struct mojo::StructTraits<network::mojom::SchemefulSiteDataView,
                                   SchemefulSite>;

  FRIEND_TEST_ALL_PREFIXES(SchemefulSiteTest, OpaqueSerialization);

  struct ObtainASiteResult {
    url::Origin origin;
    bool used_registerable_domain;
  };

  static ObtainASiteResult ObtainASite(const url::Origin&);

  explicit SchemefulSite(ObtainASiteResult);

  // Deserializes a string obtained from `SerializeWithNonce()` to a
  // `SchemefulSite`. Returns nullopt if the value was invalid in any way.
  static base::Optional<SchemefulSite> DeserializeWithNonce(
      const std::string& value);

  // Returns a serialized version of `site_as_origin_`. For an opaque
  // `site_as_origin_`, this serializes with the nonce.  See
  // `url::origin::SerializeWithNonce()` for usage information.
  base::Optional<std::string> SerializeWithNonce();

  // Origin which stores the result of running the steps documented at
  // https://html.spec.whatwg.org/multipage/origin.html#obtain-a-site.
  // This is not an arbitrary origin. It must either be an opaque origin, or a
  // scheme + eTLD+1 + default port.
  //
  // The `origin` passed into the SchemefulSite(const url::Origin&) constructor
  // might not match this internal representation used by this class to track
  // the scheme and eTLD+1 representing a schemeful site. This may be the case
  // if, e.g., the passed `origin` has an eTLD+1 that is not equal to its
  // hostname, or if the port number is not the default port for its scheme.
  //
  // In general, this `site_as_origin_` used for the internal representation
  // should NOT be used directly by SchemefulSite consumers.
  url::Origin site_as_origin_;
};

// Provided to allow gtest to create more helpful error messages, instead of
// printing hex.
inline void PrintTo(const SchemefulSite& ss, std::ostream* os) {
  *os << ss.Serialize();
}

}  // namespace net

#endif  // NET_BASE_SCHEMEFUL_SITE_H_
