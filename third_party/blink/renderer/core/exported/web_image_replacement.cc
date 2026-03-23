// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_image_replacement.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/image_replacement/image_replacement.h"

namespace blink {

// static
base::expected<CrossVariantMojoRemote<mojom::ImageReplacementInterfaceBase>,
               WebString>
WebImageReplacement::CreateAndBindReceiver(const WebElement& element) {
  auto* image_element =
      DynamicTo<HTMLImageElement>(static_cast<Element*>(element));
  if (!image_element) {
    return base::unexpected(WebString("Not an HTMLImageElement"));
  }

  base::expected<mojo::PendingRemote<mojom::blink::ImageReplacement>, String>
      result = ImageReplacement::CreateAndBindReceiver(*image_element);
  if (!result.has_value()) {
    return base::unexpected(WebString(result.error()));
  }

  return base::ok(ToCrossVariantMojoType(std::move(result.value())));
}

}  // namespace blink
