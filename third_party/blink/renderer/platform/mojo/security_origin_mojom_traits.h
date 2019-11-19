// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_SECURITY_ORIGIN_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_SECURITY_ORIGIN_MOJOM_TRAITS_H_

#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/mojom/origin.mojom-blink-forward.h"
#include "url/scheme_host_port.h"

namespace mojo {

struct UrlOriginAdapter {
  static base::Optional<base::UnguessableToken> nonce_if_opaque(
      const scoped_refptr<const ::blink::SecurityOrigin>& origin) {
    return origin->GetNonceForSerialization();
  }
  static scoped_refptr<blink::SecurityOrigin> CreateSecurityOrigin(
      const url::SchemeHostPort& tuple,
      const base::Optional<base::UnguessableToken>& nonce_if_opaque) {
    scoped_refptr<blink::SecurityOrigin> tuple_origin;
    if (!tuple.IsInvalid()) {
      // url::SchemeHostPort is percent encoded and SecurityOrigin is percent
      // decoded.
      String host = blink::DecodeURLEscapeSequences(
          String::FromUTF8(tuple.host()),
          url::DecodeURLMode::kUTF8OrIsomorphic);
      tuple_origin = blink::SecurityOrigin::Create(
          String::FromUTF8(tuple.scheme()), host, tuple.port());
    }

    if (nonce_if_opaque) {
      tuple_origin = blink::SecurityOrigin::CreateOpaque(
          url::Origin::Nonce(*nonce_if_opaque), tuple_origin.get());
    }
    return tuple_origin;
  }
  static const ::blink::SecurityOrigin* GetOriginOrPrecursorOriginIfOpaque(
      const scoped_refptr<const ::blink::SecurityOrigin>& origin) {
    return origin->GetOriginOrPrecursorOriginIfOpaque();
  }
};

template <>
struct StructTraits<url::mojom::blink::Origin::DataView,
                    scoped_refptr<const ::blink::SecurityOrigin>> {
  static WTF::String scheme(
      const scoped_refptr<const ::blink::SecurityOrigin>& origin) {
    return UrlOriginAdapter::GetOriginOrPrecursorOriginIfOpaque(origin)
        ->Protocol();
  }
  static WTF::String host(
      const scoped_refptr<const ::blink::SecurityOrigin>& origin) {
    return UrlOriginAdapter::GetOriginOrPrecursorOriginIfOpaque(origin)->Host();
  }
  static uint16_t port(
      const scoped_refptr<const ::blink::SecurityOrigin>& origin) {
    return UrlOriginAdapter::GetOriginOrPrecursorOriginIfOpaque(origin)
        ->EffectivePort();
  }
  static base::Optional<base::UnguessableToken> nonce_if_opaque(
      const scoped_refptr<const ::blink::SecurityOrigin>& origin) {
    return UrlOriginAdapter::nonce_if_opaque(origin);
  }
  static bool Read(url::mojom::blink::Origin::DataView data,
                   scoped_refptr<const ::blink::SecurityOrigin>* out) {
    // This implementation is very close to
    // SecurityOrigin::CreateFromUrlOrigin, so keep in sync if modifications
    // are made in that method.
    base::StringPiece scheme;
    base::StringPiece host;
    base::Optional<base::UnguessableToken> nonce_if_opaque;
    if (!data.ReadScheme(&scheme) || !data.ReadHost(&host) ||
        !data.ReadNonceIfOpaque(&nonce_if_opaque))
      return false;

    const url::SchemeHostPort& tuple =
        url::SchemeHostPort(scheme, host, data.port());
    if (tuple.IsInvalid()) {
      // If the tuple is invalid, it is a valid case if and only if it is an
      // opaque origin and the scheme, host, and port are empty.
      if (!nonce_if_opaque)
        return false;

      if (!scheme.empty() || !host.empty() || data.port() != 0)
        return false;
    }

    *out = UrlOriginAdapter::CreateSecurityOrigin(tuple, nonce_if_opaque);

    // If an opaque origin was created, there must be a valid
    // |nonce_if_opaque| value supplied.
    if ((*out)->IsOpaque() && !nonce_if_opaque.has_value())
      return false;

    return true;
  }

  static bool IsNull(
      const scoped_refptr<const ::blink::SecurityOrigin>& origin) {
    return !origin;
  }

  static void SetToNull(scoped_refptr<const ::blink::SecurityOrigin>* origin) {
    *origin = nullptr;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_SECURITY_ORIGIN_MOJOM_TRAITS_H_
