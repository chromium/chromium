// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/referrer_type_converters.h"

namespace mojo {

// static
blink::mojom::ReferrerPtr
TypeConverter<blink::mojom::ReferrerPtr, content::Referrer>::Convert(
    const content::Referrer& input) {
  return blink::mojom::Referrer::New(input.url, input.policy);
}

// static
content::Referrer
TypeConverter<content::Referrer, blink::mojom::ReferrerPtr>::Convert(
    const blink::mojom::ReferrerPtr& input) {
  if (!input)
    return content::Referrer();
  return content::Referrer(input->url, input->policy);
}

}  // namespace mojo
