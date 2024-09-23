// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/patch_reader.h"

#include <type_traits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "components/zucchini/algorithm.h"
#include "components/zucchini/crc32.h"
#include "components/zucchini/element_detection.h"
#include "components/zucchini/version_info.h"

namespace zucchini {

namespace patch {

bool ParseElementMatch(BufferSource* source, ElementMatch* element_match) {
  PatchElementHeader unsafe_element_header;
  if (!source->GetValue(&unsafe_element_header)) {
    LOG(ERROR) << "Impossible to read ElementMatch from source.";
    return false;
  }
  ExecutableType exe_type =
      CastToExecutableType(unsafe_element_header.exe_type);
  if (exe_type == kExeTypeUnknown) {
    LOG(ERROR) << "Invalid ExecutableType found.";
    return false;
  }
  uint16_t element_version = DisassemblerVersionOfType(exe_type);
  if (element_version != unsafe_element_header.version) {
    LOG(ERROR) << "Element version doesn't match. Expected: " << element_version
               << ", Actual: " << unsafe_element_header.version << ".";
    return false;
  }
  if (!unsafe_element_header.old_length || !unsafe_element_header.new_length) {
    LOG(ERROR) << "Empty patch element found.";
    return false;
  }
  // |unsafe_element_header| is now considered to be safe as it has a valid
  // |exe_type| and the length fields are of sufficient size.
  const auto& element_header = unsafe_element_header;

  // Caveat: Element offsets and lengths can still be invalid (e.g., exceeding
  // archive bounds), but this will be checked later.
  element_match->old_element.offset = element_header.old_offset;
  element_match->old_element.size = element_header.old_length;
  element_match->new_element.offset = element_header.new_offset;
  element_match->new_element.size = element_header.new_length;
  element_match->old_element.exe_type = exe_type;
  element_match->new_element.exe_type = exe_type;
  return true;
}

bool ParseBuffer(BufferSource* source, BufferSource* buffer) {
  uint32_t unsafe_size = 0;  // Bytes.
  static_assert(sizeof(size_t) >= sizeof(unsafe_size),
                "size_t is expected to be larger than uint32_t.");
  if (!source->GetValue(&unsafe_size)) {
    LOG(ERROR) << "Impossible to read buffer size from source.";
    return false;
  }
  if (!source->GetRegion(static_cast<size_t>(unsafe_size), buffer)) {
    LOG(ERROR) << "Impossible to read buffer content from source.";
    return false;
  }
  // Caveat: |buffer| is considered to be safe as it was possible to extract it
  // from the patch. However, this does not mean its contents are safe and when
  // parsed must be validated if possible.
  return true;
}

}  // namespace patch

/******** EquivalenceSource ********/

EquivalenceSource::EquivalenceSource() = default;
EquivalenceSource::EquivalenceSource(const EquivalenceSource&) = default;
EquivalenceSource::~EquivalenceSource() = default;

bool EquivalenceSource::Initialize(BufferSource* source) {
  return patch::ParseBuffer(source, &src_skip_) &&
         patch::ParseBuffer(source, &dst_skip_) &&
         patch::ParseBuffer(source, &copy_count_);
}

std::optional<Equivalence> EquivalenceSource::GetNext() {
  if (src_skip_.empty() || dst_skip_.empty() || copy_count_.empty())
    return std::nullopt;

  Equivalence equivalence = {};

  uint32_t length = 0;
  if (!patch::ParseVarUInt<uint32_t>(&copy_count_, &length))
    return std::nullopt;
  equivalence.length = base::strict_cast<offset_t>(length);

  int32_t src_offset_diff = 0;  // Intentionally signed.
  if (!patch::ParseVarInt<int32_t>(&src_skip_, &src_offset_diff))
    return std::nullopt;
  base::CheckedNumeric<offset_t> src_offset =
      previous_src_offset_ + src_offset_diff;
  if (!src_offset.IsValid())
    return std::nullopt;

  equivalence.src_offset = src_offset.ValueOrDie();
  previous_src_offset_ = src_offset + equivalence.length;
  if (!previous_src_offset_.IsValid())
    return std::nullopt;

  uint32_t dst_offset_diff = 0;  // Intentionally unsigned.
  if (!patch::ParseVarUInt<uint32_t>(&dst_skip_, &dst_offset_diff))
    return std::nullopt;
  base::CheckedNumeric<offset_t> dst_offset =
      previous_dst_offset_ + dst_offset_diff;
  if (!dst_offset.IsValid())
    return std::nullopt;

  equivalence.dst_offset = dst_offset.ValueOrDie();
  previous_dst_offset_ = equivalence.dst_offset + equivalence.length;
  if (!previous_dst_offset_.IsValid())
    return std::nullopt;

  // Caveat: |equivalence| is assumed to be safe only once the
  // ValidateEquivalencesAndExtraData() method has returned true. Prior to this
  // any equivalence returned is assumed to be unsafe.
  return equivalence;
}

/******** ExtraDataSource ********/

ExtraDataSource::ExtraDataSource() = default;
ExtraDataSource::ExtraDataSource(const ExtraDataSource&) = default;
ExtraDataSource::~ExtraDataSource() = default;

bool ExtraDataSource::Initialize(BufferSource* source) {
  return patch::ParseBuffer(source, &extra_data_);
}

std::optional<ConstBufferView> ExtraDataSource::GetNext(offset_t size) {
  ConstBufferView buffer;
  if (!extra_data_.GetRegion(size, &buffer))
    return std::nullopt;
  // |buffer| is assumed to always be safe/valid.
  return buffer;
}

/******** RawDeltaSource ********/

RawDeltaSource::RawDeltaSource() = default;
RawDeltaSource::RawDeltaSource(const RawDeltaSource&) = default;
RawDeltaSource::~RawDeltaSource() = default;

bool RawDeltaSource::Initialize(BufferSource* source) {
  return patch::ParseBuffer(source, &raw_delta_skip_) &&
         patch::ParseBuffer(source, &raw_delta_diff_);
}

std::optional<RawDeltaUnit> RawDeltaSource::GetNext() {
  if (raw_delta_skip_.empty() || raw_delta_diff_.empty())
    return std::nullopt;

  RawDeltaUnit raw_delta = {};
  uint32_t copy_offset_diff = 0;
  if (!patch::ParseVarUInt<uint32_t>(&raw_delta_skip_, &copy_offset_diff))
    return std::nullopt;
  base::CheckedNumeric<offset_t> copy_offset =
      copy_offset_diff + copy_offset_compensation_;
  if (!copy_offset.IsValid())
    return std::nullopt;
  raw_delta.copy_offset = copy_offset.ValueOrDie();

  if (!raw_delta_diff_.GetValue<int8_t>(&raw_delta.diff))
    return std::nullopt;

  // A 0 value for a delta.diff is considered invalid since it has no meaning.
  if (!raw_delta.diff)
    return std::nullopt;

  // We keep track of the compensation needed for next offset, taking into
  // account delta encoding and bias of -1.
  copy_offset_compensation_ = copy_offset + 1;
  if (!copy_offset_compensation_.IsValid())
    return std::nullopt;
  // |raw_delta| is assumed to always be safe/valid.
  return raw_delta;
}

/******** ReferenceDeltaSource ********/

ReferenceDeltaSource::ReferenceDeltaSource() = default;
ReferenceDeltaSource::ReferenceDeltaSource(const ReferenceDeltaSource&) =
    default;
ReferenceDeltaSource::~ReferenceDeltaSource() = default;

bool ReferenceDeltaSource::Initialize(BufferSource* source) {
  return patch::ParseBuffer(source, &source_);
}

std::optional<int32_t> ReferenceDeltaSource::GetNext() {
  if (source_.empty())
    return std::nullopt;
  int32_t ref_delta = 0;
  if (!patch::ParseVarInt<int32_t>(&source_, &ref_delta))
    return std::nullopt;
  // |ref_delta| is assumed to always be safe/valid.
  return ref_delta;
}

/******** TargetSource ********/

TargetSource::TargetSource() = default;
TargetSource::TargetSource(const TargetSource&) = default;
TargetSource::~TargetSource() = default;

bool TargetSource::Initialize(BufferSource* source) {
  return patch::ParseBuffer(source, &extra_targets_);
}

std::optional<offset_t> TargetSource::GetNext() {
  if (extra_targets_.empty())
    return std::nullopt;

  uint32_t target_diff = 0;
  if (!patch::ParseVarUInt<uint32_t>(&extra_targets_, &target_diff))
    return std::nullopt;
  base::CheckedNumeric<offset_t> target = target_diff + target_compensation_;
  if (!target.IsValid())
    return std::nullopt;

  // We keep track of the compensation needed for next target, taking into
  // account delta encoding and bias of -1.
  target_compensation_ = target + 1;
  if (!target_compensation_.IsValid())
    return std::nullopt;
  // Caveat: |target| will be a valid offset_t, but it's up to the caller to
  // check whether it's a valid offset for an image.
  return offset_t(target.ValueOrDie());
}

/******** PatchElementReader ********/

PatchElementReader::PatchElementReader() = default;
PatchElementReader::PatchElementReader(PatchElementReader&&) = default;
PatchElementReader::~PatchElementReader() = default;

bool PatchElementReader::Initialize(BufferSource* source) {
  bool ok =
      patch::ParseElementMatch(source, &element_match_) &&
      equivalences_.Initialize(source) && extra_data_.Initialize(source) &&
      ValidateEquivalencesAndExtraData() && raw_delta_.Initialize(source) &&
      reference_delta_.Initialize(source);
  if (!ok)
    return false;
  uint32_t pool_count = 0;
  if (!source->GetValue(&pool_count)) {
    LOG(ERROR) << "Impossible to read pool_count from source.";
    return false;
  }
  for (uint32_t i = 0; i < pool_count; ++i) {
    uint8_t pool_tag_value = 0;
    if (!source->GetValue(&pool_tag_value)) {
      LOG(ERROR) << "Impossible to read pool_tag from source.";
      return false;
    }
    PoolTag pool_tag(pool_tag_value);
    if (pool_tag == kNoPoolTag) {
      LOG(ERROR) << "Invalid pool_tag encountered in ExtraTargetList.";
      return false;
    }
    auto insert_result = extra_targets_.insert({pool_tag, {}});
    if (!insert_result.second) {  // Element already present.
      LOG(ERROR) << "Multiple ExtraTargetList found for the same pool_tag.";
      return false;
    }
    if (!insert_result.first->second.Initialize(source))
      return false;
  }
  return true;
}

bool PatchElementReader::ValidateEquivalencesAndExtraData() {
  EquivalenceSource equivalences_copy = equivalences_;

  const size_t old_region_size = element_match_.old_element.size;
  const size_t new_region_size = element_match_.new_element.size;

  base::CheckedNumeric<uint32_t> total_length = 0;
  // Validate that each |equivalence| falls within the bounds of the
  // |element_match_| and are in order.
  offset_t prev_dst_end = 0;
  for (auto equivalence = equivalences_copy.GetNext(); equivalence.has_value();
       equivalence = equivalences_copy.GetNext()) {
    if (!RangeIsBounded(equivalence->src_offset, equivalence->length,
                        old_region_size) ||
        !RangeIsBounded(equivalence->dst_offset, equivalence->length,
                        new_region_size)) {
      LOG(ERROR) << "Out of bounds equivalence detected.";
      return false;
    }
    if (prev_dst_end > equivalence->dst_end()) {
      LOG(ERROR) << "Out of order equivalence detected.";
      return false;
    }
    prev_dst_end = equivalence->dst_end();
    total_length += equivalence->length;
  }
  if (!total_length.IsValid() ||
      element_match_.new_element.region().size < total_length.ValueOrDie() ||
      extra_data_.extra_data().size() !=
          element_match_.new_element.region().size -
              static_cast<size_t>(total_length.ValueOrDie())) {
    LOG(ERROR) << "Incorrect amount of extra_data.";
    return false;
  }
  return true;
}

/******** EnsemblePatchReader ********/

std::optional<EnsemblePatchReader> EnsemblePatchReader::Create(
    ConstBufferView buffer) {
  BufferSource source(buffer);
  EnsemblePatchReader patch;
  if (!patch.Initialize(&source))
    return std::nullopt;
  return patch;
}

EnsemblePatchReader::EnsemblePatchReader() = default;
EnsemblePatchReader::EnsemblePatchReader(EnsemblePatchReader&&) = default;
EnsemblePatchReader::~EnsemblePatchReader() = default;

bool EnsemblePatchReader::Initialize(BufferSource* source) {
  if (!source->GetValue(&header_)) {
    LOG(ERROR) << "Impossible to read header from source.";
    return false;
  }
  if (header_.magic != PatchHeader::kMagic) {
    LOG(ERROR) << "Patch contains invalid magic.";
    return false;
  }
  if (header_.major_version != kMajorVersion) {
    LOG(ERROR) << "Patch major version doesn't match. Expected: "
               << kMajorVersion << ", Actual: " << header_.major_version << ".";
    return false;
  }
  // |header_| is assumed to be safe from this point forward.

  uint32_t element_count = 0;
  if (!source->GetValue(&element_count)) {
    LOG(ERROR) << "Impossible to read element_count from source.";
    return false;
  }

  offset_t current_dst_offset = 0;
  for (uint32_t i = 0; i < element_count; ++i) {
    PatchElementReader element_patch;
    if (!element_patch.Initialize(source))
      return false;

    if (!element_patch.old_element().FitsIn(header_.old_size) ||
        !element_patch.new_element().FitsIn(header_.new_size)) {
      LOG(ERROR) << "Invalid element encountered.";
      return false;
    }

    if (element_patch.new_element().offset != current_dst_offset) {
      LOG(ERROR) << "Invalid element encountered.";
      return false;
    }
    current_dst_offset = element_patch.new_element().EndOffset();

    elements_.push_back(std::move(element_patch));
  }
  if (current_dst_offset != header_.new_size) {
    LOG(ERROR) << "Patch elements don't fully cover new image file.";
    return false;
  }

  if (!source->empty()) {
    LOG(ERROR) << "Patch was not fully consumed.";
    return false;
  }

  return true;
}

bool EnsemblePatchReader::CheckOldFile(ConstBufferView old_image) const {
  return old_image.size() == header_.old_size &&
         CalculateCrc32(old_image.begin(), old_image.end()) == header_.old_crc;
}

bool EnsemblePatchReader::CheckNewFile(ConstBufferView new_image) const {
  return new_image.size() == header_.new_size &&
         CalculateCrc32(new_image.begin(), new_image.end()) == header_.new_crc;
}

}  // namespace zucchini
