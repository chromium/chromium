// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_PARTITION_KEY_H_
#define NET_COOKIES_COOKIE_PARTITION_KEY_H_

#include <string>

#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
namespace mojom {
class CookiePartitionKeyDataView;
}  // namespace mojom
}  // namespace network

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo

namespace net {

class NET_EXPORT CookiePartitionKey {
 public:
  CookiePartitionKey();
  explicit CookiePartitionKey(const SchemefulSite& site);
  CookiePartitionKey(const CookiePartitionKey& other);
  CookiePartitionKey(CookiePartitionKey&& other);
  CookiePartitionKey& operator=(const CookiePartitionKey& other);
  CookiePartitionKey& operator=(CookiePartitionKey&& other);
  ~CookiePartitionKey();

  bool operator==(const CookiePartitionKey& other) const;
  bool operator<(const CookiePartitionKey& other) const;

  // Methods for serializing and deserializing a partition key to/from a string.
  // This will be used for Android, storing persistent partitioned cookies, and
  // loading partitioned cookies into Java code.
  //
  // This function returns true if the partition key is not opaque. We do not
  // want to serialize cookies with opaque origins in their partition key to
  // disk, because if the browser session ends we will not be able to attach the
  // saved cookie to any future requests. This is because opaque origins' nonces
  // are only stored in volatile memory.
  //
  // TODO(crbug.com/1225444) Investigate ways to persist partition keys with
  // opaque origins if a browser session is restored.
  static bool Serialize(const absl::optional<CookiePartitionKey>& in,
                        std::string& out) WARN_UNUSED_RESULT;
  // Deserializes the result of the method above.
  // If the result is absl::nullopt, the resulting cookie is not partitioned.
  //
  // Returns if the resulting partition key is valid.
  static bool Deserialize(const std::string& in,
                          absl::optional<CookiePartitionKey>& out)
      WARN_UNUSED_RESULT;

  static CookiePartitionKey FromURLForTesting(const GURL& url) {
    return CookiePartitionKey(url);
  }

  // Create a cookie partition key from a request's NetworkIsolationKey.
  //
  static absl::optional<CookiePartitionKey> FromNetworkIsolationKey(
      const NetworkIsolationKey& network_isolation_key);

  // Temporary method, used to mark the places where we need to supply the
  // cookie partition key to CanonicalCookie::Create.
  static absl::optional<CookiePartitionKey> Todo() { return absl::nullopt; }

 private:
  explicit CookiePartitionKey(const GURL& url);

  // IPC needs access to internal site.
  friend struct mojo::StructTraits<network::mojom::CookiePartitionKeyDataView,
                                   CookiePartitionKey>;

  SchemefulSite site_;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_PARTITION_KEY_H_
