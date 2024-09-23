// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ISOLATION_INFO_H_
#define NET_BASE_ISOLATION_INFO_H_

#include <optional>
#include <set>
#include <string>

#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/site_for_cookies.h"
#include "url/origin.h"

namespace network::mojom {
class IsolationInfoDataView;
}  // namespace network::mojom

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo

namespace net {

// Class to store information about network stack requests based on the context
// in which they are made. It provides NetworkIsolationKeys, used to shard the
// HTTP cache, NetworkAnonymizationKeys, used to shard other network state, and
// SiteForCookies, used determine when to send same site cookies. The
// IsolationInfo is typically the same for all subresource requests made in the
// context of the same frame, but may be different for different frames within a
// page. The IsolationInfo associated with requests for frames may change as
// redirects are followed, and this class also contains the logic on how to do
// that.
//
// TODO(crbug.com/40093296): The SiteForCookies logic in this class is currently
// unused, but will eventually replace the logic in URLRequest/RedirectInfo for
// tracking and updating that value.
class NET_EXPORT IsolationInfo {
 public:
  // The update-on-redirect patterns.
  //
  // In general, almost everything should use kOther, as a
  // kMainFrame request accidentally sent or redirected to an attacker
  // allows cross-site tracking, and kSubFrame allows information
  // leaks between sites that iframe each other. Anything that uses
  // kMainFrame should be user triggered and user visible, like a main
  // frame navigation or downloads.
  //
  // The RequestType is a core part of an IsolationInfo, and using an
  // IsolationInfo with one value to create an IsolationInfo with another
  // RequestType is generally not a good idea, unless the RequestType of the
  // new IsolationInfo is kOther.
  enum class RequestType {
    // Updates top level origin, frame origin, and SiteForCookies on redirect.
    // These requests allow users to be recognized across sites on redirect, so
    // should not generally be used for anything other than navigations.
    kMainFrame,

    // Only updates frame origin on redirect.
    kSubFrame,

    // Updates nothing on redirect.
    kOther,
  };

  // Default constructor returns an IsolationInfo with empty origins, a null
  // SiteForCookies(), and a RequestType of kOther.
  IsolationInfo();
  IsolationInfo(const IsolationInfo&);
  IsolationInfo(IsolationInfo&&);
  ~IsolationInfo();

  IsolationInfo& operator=(const IsolationInfo&);
  IsolationInfo& operator=(IsolationInfo&&);

  // Simple constructor for internal requests. Sets |frame_origin| and
  // |site_for_cookies| match |top_frame_origin|. Sets |request_type| to
  // kOther. Will only send SameSite cookies to the site associated with
  // the passed in origin.
  static IsolationInfo CreateForInternalRequest(
      const url::Origin& top_frame_origin);

  // Creates a transient IsolationInfo. A transient IsolationInfo will not save
  // data to disk and not send SameSite cookies. Equivalent to calling
  // CreateForInternalRequest with a fresh opaque origin.
  static IsolationInfo CreateTransient();

  // Same as CreateTransient, with a `nonce` used to identify requests tagged
  // with this IsolationInfo in the network service. The `nonce` provides no
  // additional resource isolation, because the opaque origin in the resulting
  // IsolationInfo already represents a unique partition.
  static IsolationInfo CreateTransientWithNonce(
      const base::UnguessableToken& nonce);

  // Creates an IsolationInfo from the serialized contents. Returns a nullopt
  // if deserialization fails or if data is inconsistent.
  static std::optional<IsolationInfo> Deserialize(
      const std::string& serialized);

  // Creates an IsolationInfo with the provided parameters. If the parameters
  // are inconsistent, DCHECKs. In particular:
  // * If |request_type| is kMainFrame, |top_frame_origin| must equal
  //   |frame_origin|, and |site_for_cookies| must be either null or first party
  //   with respect to them.
  // * If |request_type| is kSubFrame, |top_frame_origin| must be
  //   first party with respect to |site_for_cookies|, or |site_for_cookies|
  //   must be null.
  // * If |request_type| is kOther, |top_frame_origin| and
  //   |frame_origin| must be first party with respect to |site_for_cookies|, or
  //   |site_for_cookies| must be null.
  // * If |nonce| is specified, then |top_frame_origin| must not be null.
  //
  // Note that the |site_for_cookies| consistency checks are skipped when
  // |site_for_cookies| is not HTTP/HTTPS.
  static IsolationInfo Create(
      RequestType request_type,
      const url::Origin& top_frame_origin,
      const url::Origin& frame_origin,
      const SiteForCookies& site_for_cookies,
      const std::optional<base::UnguessableToken>& nonce = std::nullopt);

  // TODO(crbug.com/344943210): Remove this and create a safer way to ensure
  // NIKs created from NAKs aren't used by accident.
  static IsolationInfo DoNotUseCreatePartialFromNak(
      const net::NetworkAnonymizationKey& network_anonymization_key);

  // Returns nullopt if the arguments are not consistent. Otherwise, returns a
  // fully populated IsolationInfo. Any IsolationInfo that can be created by
  // the other construction methods, including the 0-argument constructor, is
  // considered consistent.
  //
  // Intended for use by cross-process deserialization.
  static std::optional<IsolationInfo> CreateIfConsistent(
      RequestType request_type,
      const std::optional<url::Origin>& top_frame_origin,
      const std::optional<url::Origin>& frame_origin,
      const SiteForCookies& site_for_cookies,
      const std::optional<base::UnguessableToken>& nonce = std::nullopt);

  // Create a new IsolationInfo for a redirect to the supplied origin. |this| is
  // unmodified.
  IsolationInfo CreateForRedirect(const url::Origin& new_origin) const;

  RequestType request_type() const { return request_type_; }

  bool IsMainFrameRequest() const {
    return RequestType::kMainFrame == request_type_;
  }

  bool IsEmpty() const { return !top_frame_origin_; }

  // These may only be nullopt if created by the empty constructor. If one is
  // nullopt, both are, and SiteForCookies is null.
  //
  // Note that these are the values the IsolationInfo was created with. In the
  // case an IsolationInfo was created from a NetworkIsolationKey, they may be
  // scheme + eTLD+1 instead of actual origins.
  const std::optional<url::Origin>& top_frame_origin() const {
    return top_frame_origin_;
  }
  const std::optional<url::Origin>& frame_origin() const;

  const NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

  const NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
  }

  const std::optional<base::UnguessableToken>& nonce() const { return nonce_; }

  // The value that should be consulted for the third-party cookie blocking
  // policy, as defined in Section 2.1.1 and 2.1.2 of
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site.
  //
  // WARNING: This value must only be used for the third-party cookie blocking
  //          policy. It MUST NEVER be used for any kind of SECURITY check.
  const SiteForCookies& site_for_cookies() const { return site_for_cookies_; }

  bool IsEqualForTesting(const IsolationInfo& other) const;

  // Serialize the `IsolationInfo` into a string. Fails if transient, returning
  // an empty string.
  std::string Serialize() const;

  std::string DebugString() const;

 private:
  IsolationInfo(RequestType request_type,
                const std::optional<url::Origin>& top_frame_origin,
                const std::optional<url::Origin>& frame_origin,
                const SiteForCookies& site_for_cookies,
                const std::optional<base::UnguessableToken>& nonce);

  RequestType request_type_;

  std::optional<url::Origin> top_frame_origin_;
  std::optional<url::Origin> frame_origin_;

  // This can be deduced from the two origins above, but keep a cached version
  // to avoid repeated eTLD+1 calculations, when this is using eTLD+1.
  NetworkIsolationKey network_isolation_key_;

  NetworkAnonymizationKey network_anonymization_key_;

  SiteForCookies site_for_cookies_;

  // Having a nonce is a way to force a transient opaque `IsolationInfo`
  // for non-opaque origins.
  std::optional<base::UnguessableToken> nonce_;

  // Mojo serialization code needs to access internal fields.
  friend struct mojo::StructTraits<network::mojom::IsolationInfoDataView,
                                   IsolationInfo>;
};

}  // namespace net

#endif  // NET_BASE_ISOLATION_INFO_H_
