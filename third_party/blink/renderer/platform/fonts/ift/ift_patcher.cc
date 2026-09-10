// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/ift/ift_patcher.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

IftSubsetDefinition::IftSubsetDefinition() = default;

IftSubsetDefinition::~IftSubsetDefinition() = default;

IftSubsetDefinition::IftSubsetDefinition(IftSubsetDefinition&&) = default;

IftSubsetDefinition& IftSubsetDefinition::operator=(IftSubsetDefinition&&) =
    default;

IftSubsetDefinition::IftSubsetDefinition(const IftSubsetDefinition&) = default;

IftSubsetDefinition& IftSubsetDefinition::operator=(
    const IftSubsetDefinition&) = default;

IftSubsetDefinition::IftSubsetDefinition(
    ift_patcher_rs::IftSubsetDefinition subset)
    : subset_(std::move(subset)) {}

bool IftSubsetDefinition::AddCodepoint(uint32_t codepoint) {
  return subset_.add_codepoint(codepoint);
}

bool IftSubsetDefinition::AddFeatureTag(uint32_t tag) {
  return subset_.add_feature_tag(tag);
}

bool IftSubsetDefinition::AddDesignSpace(uint32_t tag,
                                         double min_value,
                                         double max_value) {
  return subset_.add_design_space(tag, min_value, max_value);
}

void IftSubsetDefinition::Union(const IftSubsetDefinition& other) {
  subset_.union_(other.subset_);
}

IftPatcher::IftPatcher(ift_patcher_rs::IftPatcher patcher)
    : patcher_(std::move(patcher)) {}

IftPatcher::~IftPatcher() = default;

IftPatcher::IftPatcher(IftPatcher&&) = default;

IftPatcher& IftPatcher::operator=(IftPatcher&&) = default;

std::unique_ptr<IftPatcher> IftPatcher::Create(
    base::span<const uint8_t> font_data) {
  if (!RuntimeEnabledFeatures::IncrementalFontTransferEnabled()) {
    return nullptr;
  }
  if (!ift_patcher_rs::IftPatcher::is_ift(font_data)) {
    return nullptr;
  }
  return base::WrapUnique(
      new IftPatcher(ift_patcher_rs::IftPatcher(font_data)));
}

base::span<const uint8_t> IftPatcher::GetFontData() const {
  rs_std::SliceRef<const ::std::uint8_t> font_data = patcher_.font_data();
  return base::span(font_data.to_span());
}

Vector<String> IftPatcher::RequestPatches(const IftSubsetDefinition& subset) {
  rs_std::Vec<::rs::alloc::string::String> urls =
      patcher_.request_patches(subset.subset_);
  Vector<String> ret;
  ret.ReserveInitialCapacity(base::checked_cast<wtf_size_t>(urls.size()));
  for (const ::rs::alloc::string::String& url : urls) {
    ret.push_back(String::FromUtf8(url.as_str()));
  }
  return ret;
}

void IftPatcher::AddPatchData(const String& url,
                              base::span<const uint8_t> data) {
  std::string utf8_url = url.Utf8();
  patcher_.add_patch_data(rs_std::StrRef::FromUtf8Unchecked(utf8_url), data);
}

IftPatcher::ApplyStatus IftPatcher::ApplyPatches(
    const IftSubsetDefinition& subset) {
  ift_patcher_rs::IftApplyStatus status =
      patcher_.apply_patches(subset.subset_);
  switch (status.tag) {
    case ift_patcher_rs::IftApplyStatus::Tag::Success:
      return ApplyStatus::kSuccess;
    case ift_patcher_rs::IftApplyStatus::Tag::InvalidFont:
      return ApplyStatus::kInvalidFont;
    case ift_patcher_rs::IftApplyStatus::Tag::PatchGroupError:
      return ApplyStatus::kPatchGroupError;
    case ift_patcher_rs::IftApplyStatus::Tag::PatchError:
      return ApplyStatus::kPatchError;
    case ift_patcher_rs::IftApplyStatus::Tag::MissingPatches:
      return ApplyStatus::kMissingPatches;
  }
}

bool IftPatcher::HasPendingPatchRequests() const {
  return patcher_.has_pending_patch_requests();
}

}  // namespace blink
