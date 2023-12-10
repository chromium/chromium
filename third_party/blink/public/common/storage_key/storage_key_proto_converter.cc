// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key_proto_converter.h"

#include "base/unguessable_token.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/storage_key/proto/storage_key.pb.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace storage_key_proto {

using BitType = storage_key_proto::StorageKey::AncestorChainBit::BitType;
using Scheme = storage_key_proto::StorageKey::Origin::Scheme;
using StorageKeyType = StorageKey::OneOfCase;
using UrlType = StorageKey::TopLevelSite::UrlType;

url::Origin MakeOrigin(
    const storage_key_proto::StorageKey::Origin& origin_proto) {
  std::string scheme;
  switch (origin_proto.scheme()) {
    case Scheme::StorageKey_Origin_Scheme_HTTP:
      scheme = "http";
      break;
    case Scheme::StorageKey_Origin_Scheme_HTTPS:
      scheme = "https";
      break;
  }
  std::vector<std::string> hosts = {
      "a.test",
      "b.test",
      "1.2.3.4",
      "127.0.0.1",
  };
  std::string host = hosts[origin_proto.host() % hosts.size()];
  return url::Origin::CreateFromNormalizedTuple(scheme, host,
                                                origin_proto.port());
}

blink::mojom::AncestorChainBit MakeAncestorChainBit(
    const storage_key_proto::StorageKey::AncestorChainBit& bit_proto,
    const url::Origin& origin,
    const net::SchemefulSite& top_level_site) {
  if (origin.opaque() || top_level_site != net::SchemefulSite(origin)) {
    return blink::mojom::AncestorChainBit::kCrossSite;
  }
  switch (bit_proto.bit()) {
    case BitType::StorageKey_AncestorChainBit_BitType_SAME_SITE:
      return blink::mojom::AncestorChainBit::kSameSite;
    case BitType::StorageKey_AncestorChainBit_BitType_CROSS_SITE:
      return blink::mojom::AncestorChainBit::kCrossSite;
  }
}

blink::StorageKey Convert(const storage_key_proto::StorageKey& storage_key) {
  url::Origin origin = MakeOrigin(storage_key.origin());
  switch (storage_key.OneOf_case()) {
    case StorageKeyType::ONEOF_NOT_SET:
      return blink::StorageKey::CreateFirstParty(origin);
    case StorageKeyType::kUnguessableToken:
      return blink::StorageKey::CreateWithNonce(
          origin, base::UnguessableToken::Create());
    case StorageKey::OneOfCase::kTopLevelSite:
      net::SchemefulSite top_level_site =
          net::SchemefulSite(MakeOrigin(storage_key.top_level_site().origin()));
      blink::mojom::AncestorChainBit ancestor_chain_bit = MakeAncestorChainBit(
          storage_key.ancestor_chain_bit(), origin, top_level_site);
      return blink::StorageKey::Create(origin, top_level_site,
                                       ancestor_chain_bit);
  }
}

}  // namespace storage_key_proto
