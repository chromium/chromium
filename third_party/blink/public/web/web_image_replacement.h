// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_IMAGE_REPLACEMENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_IMAGE_REPLACEMENT_H_

#include "base/types/expected.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

class WebElement;

class BLINK_EXPORT WebImageReplacement {
 public:
  static base::expected<
      CrossVariantMojoRemote<mojom::ImageReplacementInterfaceBase>,
      WebString>
  CreateAndBindReceiver(const WebElement& element);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_IMAGE_REPLACEMENT_H_
