// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/selector_fragment_anchor.h"

#include "base/feature_list.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "third_party/blink/renderer/core/fragment_directive/fragment_directive_utils.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

void SelectorFragmentAnchor::DidScroll(mojom::blink::ScrollType type) {
  if (type != mojom::blink::ScrollType::kUser &&
      type != mojom::blink::ScrollType::kCompositor) {
    return;
  }

  user_scrolled_ = true;
}

void SelectorFragmentAnchor::Trace(Visitor* visitor) const {
  FragmentAnchor::Trace(visitor);
}

bool SelectorFragmentAnchor::Invoke() {
  return InvokeSelector();
}

}  // namespace blink
