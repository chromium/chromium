// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Serialization and deserialization code for some //url types that are used in
// //net. This can't be put in //url because //url can't depend on //net.

#ifndef NET_BASE_PICKLE_URL_TYPES_H_
#define NET_BASE_PICKLE_URL_TYPES_H_

#include <optional>
#include <string>

#include "net/base/pickle_traits.h"
#include "net/base/pickle.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

template <>
struct PickleTraits<url::Origin> {
  static void Serialize(base::Pickle& pickle, const url::Origin& origin) {
    bool is_opaque = origin.opaque();
    WriteToPickle(pickle, is_opaque);
    if (!is_opaque) {
      WriteToPickle(pickle, origin.scheme(), origin.host(), origin.port());
    }
  }

  static std::optional<url::Origin> Deserialize(base::PickleIterator& iter) {
    auto maybe_is_opaque = ReadValueFromPickle<bool>(iter);
    if (!maybe_is_opaque) {
      return std::nullopt;
    }

    if (*maybe_is_opaque) {
      return url::Origin();
    }

    std::string scheme;
    std::string host;
    uint16_t port;
    if (!ReadPickleInto(iter, scheme, host, port)) {
      return std::nullopt;
    }

    // Despite the alarming name, this is safe because it checks that `scheme`
    // and `host` are valid.
    return url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(
        scheme, host, port);
  }

  static size_t PickleSize(const url::Origin& origin) {
    const bool is_opaque = origin.opaque();
    return is_opaque ? EstimatePickleSize(is_opaque)
                     : EstimatePickleSize(is_opaque, origin.scheme(),
                                          origin.host(), origin.port());
  }
};

}  // namespace net

#endif  // NET_BASE_PICKLE_URL_TYPES_H_
