// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_REFERRER_STRUCT_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_REFERRER_STRUCT_TRAITS_H_

#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/platform/referrer.mojom-blink.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace mojo {

template <>
struct StructTraits<blink::mojom::ReferrerDataView, blink::Referrer> {
  static blink::KURL url(const blink::Referrer& referrer) {
    if (referrer.referrer == blink::Referrer::NoReferrer())
      return blink::KURL();

    return blink::KURL(blink::KURL(), referrer.referrer);
  }

  // Equality of values is asserted in //Source/web/AssertMatchingEnums.cpp.
  static network::mojom::ReferrerPolicy policy(
      const blink::Referrer& referrer) {
    return static_cast<network::mojom::ReferrerPolicy>(
        referrer.referrer_policy);
  }

  static bool Read(blink::mojom::ReferrerDataView data, blink::Referrer* out) {
    blink::KURL referrer;
    network::mojom::ReferrerPolicy referrer_policy;
    if (!data.ReadUrl(&referrer) || !data.ReadPolicy(&referrer_policy))
      return false;

    out->referrer_policy = static_cast<blink::ReferrerPolicy>(referrer_policy);
    if (referrer.GetString().IsEmpty())
      out->referrer = g_null_atom;
    else
      out->referrer = AtomicString(referrer.GetString());

    // Mimics the DCHECK() done in the blink::Referrer constructor.
    return referrer.IsValid() || out->referrer == blink::Referrer::NoReferrer();
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_REFERRER_STRUCT_TRAITS_H_
