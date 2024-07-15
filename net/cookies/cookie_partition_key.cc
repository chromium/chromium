// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_partition_key.h"

#include <ostream>
#include <tuple>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/types/optional_util.h"
#include "net/base/cronet_buildflags.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"

#if !BUILDFLAG(CRONET_BUILD)
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#endif

namespace net {

namespace {

base::unexpected<std::string> WarnAndCreateUnexpected(
    const std::string& message) {
  DLOG(WARNING) << message;
  return base::unexpected(message);
}

std::string SerializeSchemefulSite(const SchemefulSite& site) {
  return site.GetURL().SchemeIsFile() ? site.SerializeFileSiteWithHost()
                                      : site.Serialize();
}

}  // namespace

CookiePartitionKey::SerializedCookiePartitionKey::SerializedCookiePartitionKey(
    base::PassKey<CookiePartitionKey> key,
    const std::string& site,
    bool has_cross_site_ancestor)
    : top_level_site_(site),
      has_cross_site_ancestor_(has_cross_site_ancestor) {}

const std::string&
CookiePartitionKey::SerializedCookiePartitionKey::TopLevelSite() const {
  return top_level_site_;
}

std::string CookiePartitionKey::SerializedCookiePartitionKey::GetDebugString()
    const {
  std::string out = TopLevelSite();
  if (base::FeatureList::IsEnabled(
          features::kAncestorChainBitEnabledInPartitionedCookies)) {
    base::StrAppend(
        &out, {", ", has_cross_site_ancestor() ? "cross-site" : "same-site"});
  }
  return out;
}

#if !BUILDFLAG(CRONET_BUILD)
CookiePartitionKey::CookiePartitionKey(mojo::DefaultConstruct::Tag) {}
#endif
bool CookiePartitionKey::SerializedCookiePartitionKey::has_cross_site_ancestor()
    const {
  return has_cross_site_ancestor_;
}

// static
CookiePartitionKey::AncestorChainBit CookiePartitionKey::BoolToAncestorChainBit(
    bool cross_site) {
  return cross_site ? AncestorChainBit::kCrossSite
                    : AncestorChainBit::kSameSite;
}

CookiePartitionKey::CookiePartitionKey(
    const SchemefulSite& site,
    std::optional<base::UnguessableToken> nonce,
    AncestorChainBit ancestor_chain_bit)
    : site_(site), nonce_(nonce), ancestor_chain_bit_(ancestor_chain_bit) {
}

CookiePartitionKey::CookiePartitionKey(bool from_script)
    : from_script_(from_script) {}

CookiePartitionKey::CookiePartitionKey(const CookiePartitionKey& other) =
    default;

CookiePartitionKey::CookiePartitionKey(CookiePartitionKey&& other) = default;

CookiePartitionKey& CookiePartitionKey::operator=(
    const CookiePartitionKey& other) = default;

CookiePartitionKey& CookiePartitionKey::operator=(CookiePartitionKey&& other) =
    default;

CookiePartitionKey::~CookiePartitionKey() = default;

bool CookiePartitionKey::operator==(const CookiePartitionKey& other) const {
  AncestorChainBit this_bit = MaybeAncestorChainBit();
  AncestorChainBit other_bit = other.MaybeAncestorChainBit();

  return std::tie(site_, nonce_, this_bit) ==
         std::tie(other.site_, other.nonce_, other_bit);
}

bool CookiePartitionKey::operator!=(const CookiePartitionKey& other) const {
  return !(*this == other);
}

bool CookiePartitionKey::operator<(const CookiePartitionKey& other) const {
  AncestorChainBit this_bit = MaybeAncestorChainBit();
  AncestorChainBit other_bit = other.MaybeAncestorChainBit();
  return std::tie(site_, nonce_, this_bit) <
         std::tie(other.site_, other.nonce_, other_bit);
}

// static
base::expected<CookiePartitionKey::SerializedCookiePartitionKey, std::string>
CookiePartitionKey::Serialize(const std::optional<CookiePartitionKey>& in) {
  if (!in) {
    return base::ok(SerializedCookiePartitionKey(
        base::PassKey<CookiePartitionKey>(), kEmptyCookiePartitionKey, true));
  }

  if (!in->IsSerializeable()) {
    return WarnAndCreateUnexpected("CookiePartitionKey is not serializeable");
  }

  return base::ok(SerializedCookiePartitionKey(
      base::PassKey<CookiePartitionKey>(), SerializeSchemefulSite(in->site_),
      in->IsThirdParty()));
}

std::optional<CookiePartitionKey> CookiePartitionKey::FromNetworkIsolationKey(
    const NetworkIsolationKey& network_isolation_key,
    const SiteForCookies& site_for_cookies,
    const SchemefulSite& request_site,
    bool main_frame_navigation) {
  if (cookie_util::PartitionedCookiesDisabledByCommandLine()) {
    return std::nullopt;
  }

  const std::optional<base::UnguessableToken>& nonce =
      network_isolation_key.GetNonce();

  // Use frame site for nonced partitions. Since the nonce is unique, this
  // still creates a unique partition key. The reason we use the frame site is
  // to align CookiePartitionKey's implementation of nonced partitions with
  // StorageKey's. See https://crbug.com/1440765.
  const std::optional<SchemefulSite>& partition_key_site =
      nonce ? network_isolation_key.GetFrameSiteForCookiePartitionKey(
                  NetworkIsolationKey::CookiePartitionKeyPassKey())
            : network_isolation_key.GetTopFrameSite();
  if (!partition_key_site) {
    return std::nullopt;
  }

  // When a main_frame_navigation occurs, the ancestor chain bit value should
  // always be kSameSite, unless there is a nonce, since a main frame has no
  // ancestor, context: crbug.com/(337206302).
  AncestorChainBit ancestor_chain_bit;
  if (nonce) {
    ancestor_chain_bit = AncestorChainBit::kCrossSite;
  } else if (main_frame_navigation) {
    ancestor_chain_bit = AncestorChainBit::kSameSite;
  } else if (site_for_cookies.IsNull()) {
    ancestor_chain_bit = AncestorChainBit::kCrossSite;
  } else {
    ancestor_chain_bit = BoolToAncestorChainBit(
        !site_for_cookies.IsFirstParty(request_site.GetURL()));
  }

  return CookiePartitionKey(*partition_key_site, nonce, ancestor_chain_bit);
}

// static
std::optional<CookiePartitionKey> CookiePartitionKey::FromStorageKeyComponents(
    const SchemefulSite& site,
    AncestorChainBit ancestor_chain_bit,
    const std::optional<base::UnguessableToken>& nonce) {
  if (cookie_util::PartitionedCookiesDisabledByCommandLine()) {
    return std::nullopt;
  }
  return CookiePartitionKey::FromWire(site, ancestor_chain_bit, nonce);
}

// static
base::expected<std::optional<CookiePartitionKey>, std::string>
CookiePartitionKey::FromStorage(const std::string& top_level_site,
                                bool has_cross_site_ancestor) {
  if (top_level_site == kEmptyCookiePartitionKey) {
    return base::ok(std::nullopt);
  }

  base::expected<CookiePartitionKey, std::string> key = DeserializeInternal(
      top_level_site, BoolToAncestorChainBit(has_cross_site_ancestor),
      ParsingMode::kStrict);
  if (!key.has_value()) {
    DLOG(WARNING) << key.error();
  }

  return key;
}

// static
base::expected<CookiePartitionKey, std::string>
CookiePartitionKey::FromUntrustedInput(const std::string& top_level_site,
                                       bool has_cross_site_ancestor) {
  if (top_level_site.empty()) {
    return WarnAndCreateUnexpected("top_level_site is unexpectedly empty");
  }

  base::expected<CookiePartitionKey, std::string> key = DeserializeInternal(
      top_level_site, BoolToAncestorChainBit(has_cross_site_ancestor),
      ParsingMode::kLoose);
  if (!key.has_value()) {
    return WarnAndCreateUnexpected(key.error());
  }
  return key;
}

base::expected<CookiePartitionKey, std::string>
CookiePartitionKey::DeserializeInternal(
    const std::string& top_level_site,
    CookiePartitionKey::AncestorChainBit has_cross_site_ancestor,
    CookiePartitionKey::ParsingMode parsing_mode) {
  if (cookie_util::PartitionedCookiesDisabledByCommandLine()) {
    return WarnAndCreateUnexpected("Partitioned cookies are disabled");
  }

  auto schemeful_site = SchemefulSite::Deserialize(top_level_site);
  if (schemeful_site.opaque()) {
    return WarnAndCreateUnexpected(
        "Cannot deserialize opaque origin to CookiePartitionKey");
  } else if (parsing_mode == ParsingMode::kStrict &&
             SerializeSchemefulSite(schemeful_site) != top_level_site) {
    return WarnAndCreateUnexpected(
        "Cannot deserialize malformed top_level_site to CookiePartitionKey");
  }
  return base::ok(CookiePartitionKey(schemeful_site, std::nullopt,
                                     has_cross_site_ancestor));
}

bool CookiePartitionKey::IsSerializeable() const {
  // We should not try to serialize a partition key created by a renderer.
  DCHECK(!from_script_);
  return !site_.opaque() && !nonce_.has_value();
}

CookiePartitionKey::AncestorChainBit CookiePartitionKey::MaybeAncestorChainBit()
    const {
  return ancestor_chain_enabled_ ? ancestor_chain_bit_
                                 : AncestorChainBit::kCrossSite;
}

std::ostream& operator<<(std::ostream& os, const CookiePartitionKey& cpk) {
  os << cpk.site();
  if (cpk.nonce().has_value()) {
    os << ",nonced";
  }
  os << (cpk.IsThirdParty() ? ",cross_site" : ",same_site");
  return os;
}

}  // namespace net
