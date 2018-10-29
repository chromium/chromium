// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_REFERRER_STRUCT_TRAITS_H_
#define CONTENT_PUBLIC_COMMON_REFERRER_STRUCT_TRAITS_H_

#include "content/common/content_export.h"
#include "content/public/common/referrer.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "third_party/blink/public/platform/referrer.mojom.h"

namespace mojo {

template <>
struct CONTENT_EXPORT
    StructTraits<::blink::mojom::ReferrerDataView, content::Referrer> {
  static const GURL& url(const content::Referrer& r) {
    return r.url;
  }

  static ::network::mojom::ReferrerPolicy policy(const content::Referrer& r) {
    return r.policy;
  }

  static bool Read(::blink::mojom::ReferrerDataView data,
                   content::Referrer* out);
};

}

#endif  // CONTENT_PUBLIC_COMMON_REFERRER_STRUCT_TRAITS_H_
