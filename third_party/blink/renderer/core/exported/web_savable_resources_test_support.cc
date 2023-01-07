// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_savable_resources_test_support.h"

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/renderer/core/frame/savable_resources.h"

namespace blink {

WebString GetSubResourceLinkFromElementForTesting(const WebElement& element) {
  return WebString(SavableResources::GetSubResourceLinkFromElement(
      static_cast<Element*>(element)));
}

}  // namespace blink
