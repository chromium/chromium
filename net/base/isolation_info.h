// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ISOLATION_INFO_H_
#define NET_BASE_ISOLATION_INFO_H_

#include <optional>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "net/base/network_isolation_partition.h"
#include "net/base/url_util.h"
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
//
// IsolationInfo has an optional `frame_ancestor_relation_` member, whose value
// represents the relationship between the request's `frame_origin`,
// `top_frame_origin`, and all other ancestor frame origins. A
// `frame_ancestor_relation` with a value of nullopt is used for requests where
// we do not know the requesting frame's relation to its ancestors. Note that
// this does not consider the origin of request itself in the computation, even
// if request is for a frame's root document.
//
// IsolationInfo has a `nonce_` member, which can be used to force a particular
// "shard" based upon that nonce. An IsolationInfo with an opaque origin will
// also have its own shard, but the nonce enables this behavior even for non-
// opaque origins. If multiple documents share the same nonce, they can
// therefore share the same shard, so it is possible to leak information between
// them via partitioned cookies, etc. This nonce is provided to many of the
// constructor/factory methods of this class; if an IsolationInfo is created in
// the context of a document with a partition nonce, then that partition nonce
// should be provided, to ensure information is only visible within the same
// partition. Currently, only fenced frames and credentailless iframes use a
// partition nonce.
//
// Even if full third-party cookie access is enabled, the `nonce` will force a
// cookie partition keyed using that nonce. Keep this in mind when using a
// nonced IsolationInfo for credentialed requests.
//
// In addition to the sharding described above, `IsolationInfo::nonce()` is also
// used to check if a given network request should be disallowed because the
// initiating fenced frame has revoked network access. More context on
// network revocation is in network_context.mojom in the comment for
// `NetworkContext::RevokeNetworkForNonces()`. Not providing the correct
// `nonce` will therefore lead to sending network requests that should
// have been blocked. See `RenderFrameHostImpl::ComputeNonce()` which
// computes the correct nonce for a given frame.
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
    kMax = kOther
  };

  // The FrameAncestorRelation describes the relationship that all the frame
  // ancestors (and ONLY the frame ancestors) of the current request have to
  // each other.
  //
  // Consumers of this class must construct an IsolationInfo with a nullopt
  // FrameAncestorRelation unless they have explicit knowledge of all of the
  // current frame's ancestors.  Note that kMainFrame RequestTypes will always
  // have a kSameOrigin FrameAncestorRelation.
  enum class FrameAncestorRelation {
    // Value for requests whose ancestor frames' origins all have a same-origin
    // relationship.
    kSameOrigin,
    // Value for requests whose ancestor frames' origins do not have a
    // same-origin relationship, but all share a common a scheme and site.
    kSameSite,
    // Value for requests whose ancestor frames' origins do not all share a
    // scheme and/or site.
    kCrossSite,
  };

  // Default constructor returns an IsolationInfo with empty origins, a null
  // SiteForCookies(), and a RequestType of kOther.
  IsolationInfo();
  IsolationInfo(const IsolationInfo&);
  IsolationInfo(IsolationInfo&&);
  ~IsolationInfo();

  IsolationInfo& operator=(const IsolationInfo&);
  IsolationInfo& operator=(IsolationInfo&&);

  // Returns the equivalent FrameAncestorRelation for a given
  // OriginRelationValue. The value returned is the same as finding the
  // FrameAncestorRelation for a set of two frame ancestors having the
  // OriginRelationValue of `origin_relation_value`.
  static std::optional<FrameAncestorRelation>
  OriginRelationToFrameAncestorRelation(
      std::optional<OriginRelation> origin_relation_value);

  // Returns the greater value of `cur_relation` and the FrameAncestorRelation
  // corresponding to the set of frame ancestors whose members are
  // `frame_origin` and `top_frame_origin`. If `cur_relation` is nullopt, a
  // nullopt will be returned.
  static std::optional<FrameAncestorRelation> ComputeNewFrameAncestorRelation(
      std::optional<FrameAncestorRelation> cur_relation,
      const url::Origin& frame_origin,
      const url::Origin& top_frame_origin);

  static std::string_view FrameAncestorRelationString(
      FrameAncestorRelation frame_ancestor_relation);

  // Simple constructor for internal requests. Sets |frame_origin| and
  // |site_for_cookies| match |top_frame_origin|. Sets |request_type| to
  // kOther. Will only send SameSite cookies to the site associated with
  // the passed in origin.
  static IsolationInfo CreateForInternalRequest(
      const url::Origin& top_frame_origin);

  // Creates a transient IsolationInfo. A transient IsolationInfo will not save
  // data to disk and not send SameSite cookies. When `nonce` is std::nullopt,
  // this is equivalent to calling CreateForInternalRequest with a fresh opaque
  // origin.

  // Because the origin of the returned IsolationInfo is opaque, all network
  // state partitioning outside of cookies will be unique (see the class
  // meta-comment for how cookies are affected).

  // Note: error pages resulting from a failed navigation should always use a
  // transient IsolationInfo with no nonce.
  static IsolationInfo CreateTransient(
      std::optional<base::UnguessableToken> nonce);

  // Creates an IsolationInfo from the serialized contents. Returns a nullopt
  // if deserialization fails or if data is inconsistent.
  static std::optional<IsolationInfo> Deserialize(
      const std::string& serialized);

  // Creates an IsolationInfo with the provided parameters. If the parameters
  // are inconsistent, DCHECKs. In particular:
  // * If `request_type` is kMainFrame, `top_frame_origin` must equal
  //   `frame_origin`, `site_for_cookies` must be either null or first party
  //   with respect to them, and `frame_ancestor_relation` must be kSameOrigin.
  // * If `request_type` is kSubFrame, `top_frame_origin` must be
  //   first party with respect to |site_for_cookies|, or `site_for_cookies`
  //   must be null.
  // * If `request_type` is kOther, `top_frame_origin` and
  //   `frame_origin` must be first party with respect to `site_for_cookies`, or
  //   `site_for_cookies` must be null. If `frame_ancestor_relation` is non-null
  //   and not kCrossSite, then the FrameAncestorRelation between
  //   `top_frame_origin` and `frame_origin` must not supersede.
  // * If `nonce` is specified, then `top_frame_origin` must not be null.
  //   Please see the meta-comment for this class for the `nonce` to provide.
  //
  // Note that the `site_for_cookies` consistency checks are skipped when
  // `site_for_cookies` is not HTTP/HTTPS.
  static IsolationInfo Create(
      RequestType request_type,
      url::Origin top_frame_origin,
      url::Origin frame_origin,
      SiteForCookies site_for_cookies,
      std::optional<base::UnguessableToken> nonce = std::nullopt,
      NetworkIsolationPartition network_isolation_partition =
          NetworkIsolationPartition::kGeneral,
      std::optional<FrameAncestorRelation> frame_ancestor_relation =
          std::nullopt);

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
      std::optional<url::Origin> top_frame_origin,
      std::optional<url::Origin> frame_origin,
      SiteForCookies site_for_cookies,
      std::optional<base::UnguessableToken> nonce = std::nullopt,
      NetworkIsolationPartition network_isolation_partition =
          NetworkIsolationPartition::kGeneral,
      std::optional<FrameAncestorRelation> frame_ancestor_relation =
          std::nullopt);

  // Create a new IsolationInfo for a redirect to the supplied origin. |this| is
  // unmodified.
  IsolationInfo CreateForRedirect(const url::Origin& new_origin) const;

  RequestType request_type() const { return data_->request_type(); }

  std::optional<FrameAncestorRelation> frame_ancestor_relation() const {
    return data_->frame_ancestor_relation();
  }

  bool IsMainFrameRequest() const {
    return RequestType::kMainFrame == request_type();
  }

  // If this request is associated with a outer most main frame. See
  // `RenderFrameHost::GetOutermostMainFrame` for more information.
  bool IsOutermostMainFrameRequest() const {
    return IsMainFrameRequest() && !nonce();
  }

  bool IsEmpty() const { return !top_frame_origin(); }

  // These may only be nullopt if created by the empty constructor. If one is
  // nullopt, both are, and SiteForCookies is null.
  //
  // Note that these are the values the IsolationInfo was created with. In the
  // case an IsolationInfo was created from a NetworkIsolationKey, they may be
  // scheme + eTLD+1 instead of actual origins.
  const std::optional<url::Origin>& top_frame_origin() const {
    return data_->top_frame_origin();
  }
  const std::optional<url::Origin>& frame_origin() const {
    return data_->frame_origin();
  }

  const NetworkIsolationKey& network_isolation_key() const {
    return data_->network_isolation_key();
  }

  const NetworkAnonymizationKey& network_anonymization_key() const {
    return data_->network_anonymization_key();
  }

  const std::optional<base::UnguessableToken>& nonce() const {
    return data_->network_isolation_key().GetNonce();
  }

  NetworkIsolationPartition GetNetworkIsolationPartition() const;

  // The value that should be consulted for the third-party cookie blocking
  // policy, as defined in Section 2.1.1 and 2.1.2 of
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site.
  //
  // WARNING: This value must only be used for the third-party cookie blocking
  //          policy. It MUST NEVER be used for any kind of SECURITY check.
  const SiteForCookies& site_for_cookies() const {
    return data_->site_for_cookies();
  }

  bool IsEqualForTesting(const IsolationInfo& other) const;

  // Serialize the `IsolationInfo` into a string. Fails if transient, returning
  // an empty string.
  std::string Serialize() const;

  std::string DebugString() const;

 private:
  // Holds all the data of an IsolationInfo. This is ref-counted to make copying
  // IsolationInfo objects cheaper.
  class Data : public base::RefCountedThreadSafe<Data> {
   public:
    Data(RequestType request_type,
         std::optional<url::Origin> top_frame_origin,
         std::optional<url::Origin> frame_origin,
         std::optional<FrameAncestorRelation> frame_ancestor_relation,
         SiteForCookies site_for_cookies,
         std::optional<base::UnguessableToken> nonce,
         NetworkIsolationPartition network_isolation_partition);

    RequestType request_type() const { return request_type_; }
    const std::optional<url::Origin>& top_frame_origin() const {
      return top_frame_origin_;
    }
    const std::optional<url::Origin>& frame_origin() const {
      return frame_origin_;
    }
    const std::optional<FrameAncestorRelation>& frame_ancestor_relation()
        const {
      return frame_ancestor_relation_;
    }
    const SiteForCookies& site_for_cookies() const { return site_for_cookies_; }

    const NetworkIsolationKey& network_isolation_key() const {
      return network_isolation_key_;
    }

    const NetworkAnonymizationKey& network_anonymization_key() const {
      return network_anonymization_key_;
    }

   private:
    friend class base::RefCountedThreadSafe<Data>;
    ~Data();

    const RequestType request_type_;
    const std::optional<url::Origin> top_frame_origin_;
    const std::optional<url::Origin> frame_origin_;
    const std::optional<FrameAncestorRelation> frame_ancestor_relation_;
    const SiteForCookies site_for_cookies_;
    const NetworkIsolationKey network_isolation_key_;
    const NetworkAnonymizationKey network_anonymization_key_;
  };

  IsolationInfo(RequestType request_type,
                std::optional<url::Origin> top_frame_origin,
                std::optional<url::Origin> frame_origin,
                SiteForCookies site_for_cookies,
                std::optional<base::UnguessableToken> nonce,
                NetworkIsolationPartition network_isolation_partition,
                std::optional<FrameAncestorRelation> frame_ancestor_relation);

  // This is never null.
  scoped_refptr<const Data> data_;

  // Mojo serialization code needs to access internal fields.
  friend struct mojo::StructTraits<network::mojom::IsolationInfoDataView,
                                   IsolationInfo>;
};

}  // namespace net

#endif  // NET_BASE_ISOLATION_INFO_H_
