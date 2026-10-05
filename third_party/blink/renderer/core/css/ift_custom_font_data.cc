// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/ift_custom_font_data.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/platform/fonts/ift/ift_patcher.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

IftCustomFontData* IftCustomFontData::MaybeCreate(FontResource& font_resource) {
  std::unique_ptr<IftPatcher> patcher = font_resource.TakeIftPatcher();
  if (!patcher) {
    return nullptr;
  }
  return MakeGarbageCollected<IftCustomFontData>(
      base::PassKey<IftCustomFontData>(), std::move(patcher));
}

IftCustomFontData::IftCustomFontData(base::PassKey<IftCustomFontData>,
                                     std::unique_ptr<IftPatcher> patcher)
    : patcher_(std::move(patcher)),
      requested_subset_(std::make_unique<IftSubsetDefinition>()) {
  CHECK(patcher_);
}

IftCustomFontData::~IftCustomFontData() = default;

bool IftCustomFontData::IftRequireSubset(const StringView& text) const {
  for (UChar32 code_point : text) {
    requested_subset_->AddCodepoint(static_cast<uint32_t>(code_point));
  }
  // TODO(wmedrano): Extend the font with `patcher_` to support `text`. Until
  // then, report that the font does not support any requested text.
  return false;
}

}  // namespace blink
