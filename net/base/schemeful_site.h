// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SCHEMEFUL_SITE_H_
#define NET_BASE_SCHEMEFUL_SITE_H_

#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "base/types/pass_key.h"
#include "net/base/net_export.h"
#include "url/origin.h"

class GURL;

namespace blink {
class BlinkSchemefulSite;
class StorageKey;
}  // namespace blink

namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

namespace network::mojom {
class SchemefulSiteDataView;
}  // namespace network::mojom

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo

namespace net {

class NetworkAnonymizationKey;
class SiteForCookies;

// Class which represents a scheme and etld+1 for an origin, as specified by
// https://html.spec.whatwg.org/multipage/origin.html#obtain-a-site.
//
// A SchemefulSite is obtained from an input origin by normalizing, such that:
// 1. Opaque origins have distinct SchemefulSites.
// 2. Origins whose schemes have network hosts have the same SchemefulSite iff
//    they share a scheme, and share a hostname or registrable domain. Origins
//    whose schemes have network hosts include http, https, ws, wss, file, etc.
// 3. Origins whose schemes do not have a network host have the same
//    SchemefulSite iff they share a scheme and host.
// 4. Origins which differ only by port have the same SchemefulSite.
// 5. Websocket origins cannot have a SchemefulSite (they trigger a DCHECK).
//
// Note that blink::BlinkSchemefulSite mirrors this class and needs to be kept
// in sync with any data member changes.
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
  SchemefulSite(SchemefulSite&& other) noexcept;

  SchemefulSite& operator=(const SchemefulSite& other);
  SchemefulSite& operator=(SchemefulSite&& other) noexcept;

  // Tries to construct an instance from a (potentially untrusted) value of the
  // internal `site_as_origin_` that got received over an RPC.
  //
  // Returns whether successful or not. Doesn't touch |*out| if false is
  // returned.  This returning |true| does not mean that whoever sent the values
  // did not lie, merely that they are well-formed.
  static bool FromWire(const url::Origin& site_as_origin, SchemefulSite* out);

  // Creates a SchemefulSite iff the passed-in origin has a registerable domain.
  static std::optional<SchemefulSite> CreateIfHasRegisterableDomain(
      const url::Origin&);

  // If the scheme is ws or wss, it is converted to http or https, respectively.
  // Has no effect on SchemefulSites with any other schemes.
  //
  // See Step 1 of algorithm "establish a WebSocket connection" in
  // https://fetch.spec.whatwg.org/#websocket-opening-handshake.
  void ConvertWebSocketToHttp();

  // Deserializes a string obtained from `Serialize()` to a `SchemefulSite`.
  // Returns an opaque `SchemefulSite` if the value was invalid in any way.
  static SchemefulSite Deserialize(std::string_view value);

  // Returns a serialized version of `site_as_origin_`. If the underlying origin
  // is invalid, returns an empty string. If serialization of opaque origins
  // with their associated nonce is necessary, see `SerializeWithNonce()`.
  std::string Serialize() const;

  // Serializes `site_as_origin_` in cases when it has a 'file' scheme but
  // we want to preserve the Origin's host.
  // This was added to serialize cookie partition keys, which may contain
  // file origins with a host.
  std::string SerializeFileSiteWithHost() const;

  std::string GetDebugString() const;

  // Gets the underlying site as a GURL. If the internal Origin is opaque,
  // returns an empty GURL.
  GURL GetURL() const;

  // Deserializes a string obtained from `SerializeWithNonce()` to a
  // `SchemefulSite`. Returns nullopt if the value was invalid in any way.
  static std::optional<SchemefulSite> DeserializeWithNonce(
      base::PassKey<NetworkAnonymizationKey>,
      std::string_view value);

  // Returns a serialized version of `site_as_origin_`. For an opaque
  // `site_as_origin_`, this serializes with the nonce.  See
  // `url::origin::SerializeWithNonce()` for usage information.
  std::optional<std::string> SerializeWithNonce(
      base::PassKey<NetworkAnonymizationKey>);

  bool opaque() const { return site_as_origin_.opaque(); }

  bool has_registrable_domain_or_host() const {
    return !registrable_domain_or_host().empty();
  }

  // Testing only function which allows tests to access the underlying
  // `site_as_origin_` in order to verify behavior.
  const url::Origin& GetInternalOriginForTesting() const;

  // Testing-only function which allows access to the private
  // `registrable_domain_or_host` method.
  std::string registrable_domain_or_host_for_testing() const {
    return registrable_domain_or_host();
  }

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  bool operator==(const SchemefulSite& other) const;

  bool operator!=(const SchemefulSite& other) const;

  bool operator<(const SchemefulSite& other) const;

 private:
  // IPC serialization code needs to access internal origin.
  friend struct mojo::StructTraits<network::mojom::SchemefulSiteDataView,
                                   SchemefulSite>;
  friend struct IPC::ParamTraits<net::SchemefulSite>;

  friend class blink::BlinkSchemefulSite;

  // Create SiteForCookies from SchemefulSite needs to access internal origin,
  // and SiteForCookies needs to access private method SchemelesslyEqual.
  friend class SiteForCookies;

  // Needed to create a bogus origin from a site.
  // TODO(crbug.com/40157262): Give IsolationInfos empty origins instead,
  // in this case, and unfriend IsolationInfo.
  friend class IsolationInfo;

  // Needed to create a bogus origin from a site.
  friend class URLRequest;

  // Needed for access to nonce for serialization.
  friend class blink::StorageKey;

  FRIEND_TEST_ALL_PREFIXES(SchemefulSiteTest, OpaqueSerialization);
  FRIEND_TEST_ALL_PREFIXES(SchemefulSiteTest, InternalValue);

  struct ObtainASiteResult;

  static ObtainASiteResult ObtainASite(const url::Origin&);

  explicit SchemefulSite(ObtainASiteResult, const url::Origin&);

  // Deserializes a string obtained from `SerializeWithNonce()` to a
  // `SchemefulSite`. Returns nullopt if the value was invalid in any way.
  static std::optional<SchemefulSite> DeserializeWithNonce(
      std::string_view value);

  // Returns a serialized version of `site_as_origin_`. For an opaque
  // `site_as_origin_`, this serializes with the nonce.  See
  // `url::origin::SerializeWithNonce()` for usage information.
  std::optional<std::string> SerializeWithNonce();

  // Returns whether `this` and `other` share a host or registrable domain.
  // Should NOT be used to check equality or equivalence. This is only used
  // for legacy same-site cookie logic that does not check schemes. Private to
  // restrict usage.
  bool SchemelesslyEqual(const SchemefulSite& other) const;

  // Returns the host of the underlying `origin`, which will usually be the
  // registrable domain. This is private because if it were public, it would
  // trivially allow circumvention of the "Schemeful"-ness of this class.
  std::string registrable_domain_or_host() const {
    return site_as_origin_.host();
  }

  // This should not be used casually, it's an opaque Origin or an scheme+eTLD+1
  // packed into an Origin. If you extract this value SchemefulSite is not
  // responsible for any unexpected friction you might encounter.
  const url::Origin& internal_value() const { return site_as_origin_; }

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
//
// Also used so that SchemefulSites can be the arguments of DCHECK_EQ.
NET_EXPORT std::ostream& operator<<(std::ostream& os, const SchemefulSite& ss);

}  // namespace net

#endif  // NET_BASE_SCHEMEFUL_SITE_H_
