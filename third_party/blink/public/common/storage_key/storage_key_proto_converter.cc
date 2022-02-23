// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key_proto_converter.h"

#include "base/unguessable_token.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/storage_key/proto/storage_key.pb.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace storage_key_proto {

using UrlType = StorageKey::TopLevelSite::UrlType;
using StorageKeyType = StorageKey::OneOfCase;

url::Origin MakeOrigin(
    const storage_key_proto::StorageKey::Origin& origin_proto) {
  using Scheme = storage_key_proto::StorageKey::Origin::Scheme;

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

  uint16_t port = origin_proto.port();
  if (port == 0)
    port = 1;

  return url::Origin::CreateFromNormalizedTuple(scheme, host,
                                                origin_proto.port());
}

blink::StorageKey Convert(const storage_key_proto::StorageKey& storage_key) {
  url::Origin origin = MakeOrigin(storage_key.origin());

  StorageKey::OneOfCase storage_key_type = storage_key.OneOf_case();

  if (storage_key_type == StorageKeyType::ONEOF_NOT_SET)
    return blink::StorageKey(origin);

  if (storage_key_type == StorageKeyType::kUnguessableToken) {
    return blink::StorageKey::CreateWithNonce(origin,
                                              base::UnguessableToken::Create());
  }

  if (storage_key_type == StorageKey::OneOfCase::kTopLevelSite) {
    StorageKey::TopLevelSite top_level_site_proto =
        storage_key.top_level_site();
    url::Origin top_level_site = MakeOrigin(top_level_site_proto.origin());

    switch (top_level_site_proto.url_type()) {
      case UrlType::StorageKey_TopLevelSite_UrlType_ORIGIN:
        return blink::StorageKey(origin, top_level_site);
      case UrlType::StorageKey_TopLevelSite_UrlType_SCHEMEFUL_SITE:
        return blink::StorageKey(origin, net::SchemefulSite(top_level_site));
    }
  }

  return blink::StorageKey();
}

}  // namespace storage_key_proto
