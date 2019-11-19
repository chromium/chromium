// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LINK_LOAD_PARAMETERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LINK_LOAD_PARAMETERS_H_

#include "base/optional.h"
#include "base/unguessable_token.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/link_rel_attribute.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LinkHeader;

struct CORE_EXPORT LinkLoadParameters {
  LinkLoadParameters(const LinkRelAttribute&,
                     const CrossOriginAttributeValue&,
                     const String& type,
                     const String& as,
                     const String& media,
                     const String& nonce,
                     const String& integrity,
                     const String& importance,
                     network::mojom::ReferrerPolicy,
                     const KURL& href,
                     const String& image_srcset,
                     const String& image_sizes);
  LinkLoadParameters(const LinkHeader&, const KURL& base_url);

  LinkRelAttribute rel;
  CrossOriginAttributeValue cross_origin;
  String type;
  String as;
  String media;
  String nonce;
  String integrity;
  String importance;
  network::mojom::ReferrerPolicy referrer_policy;
  KURL href;
  String image_srcset;
  String image_sizes;
  base::Optional<base::UnguessableToken> recursive_prefetch_token;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LINK_LOAD_PARAMETERS_H_
