// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_REFERRER_TYPE_CONVERTERS_H_
#define CONTENT_PUBLIC_COMMON_REFERRER_TYPE_CONVERTERS_H_

#include "content/common/content_export.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

namespace mojo {
// TODO(leonhsl): Remove these converters once we remove content::Referrer.
template <>
struct CONTENT_EXPORT
    TypeConverter<blink::mojom::ReferrerPtr, content::Referrer> {
  static blink::mojom::ReferrerPtr Convert(const content::Referrer& input);
};

template <>
struct CONTENT_EXPORT
    TypeConverter<content::Referrer, blink::mojom::ReferrerPtr> {
  static content::Referrer Convert(const blink::mojom::ReferrerPtr& input);
};

}  // namespace mojo

#endif  // CONTENT_PUBLIC_COMMON_REFERRER_TYPE_CONVERTERS_H_
