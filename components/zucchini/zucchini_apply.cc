// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/zucchini_apply.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/element_detection.h"
#include "components/zucchini/equivalence_map.h"
#include "components/zucchini/image_index.h"

namespace zucchini {

bool ApplyEquivalenceAndExtraData(ConstBufferView old_image,
                                  const PatchElementReader& patch_reader,
                                  MutableBufferView new_image) {
  EquivalenceSource equiv_source = patch_reader.GetEquivalenceSource();
  ExtraDataSource extra_data_source = patch_reader.GetExtraDataSource();
  MutableBufferView::iterator dst_it = new_image.begin();

  for (auto equivalence = equiv_source.GetNext(); equivalence.has_value();
       equivalence = equiv_source.GetNext()) {
    MutableBufferView::iterator next_dst_it =
        new_image.begin() + equivalence->dst_offset;
    CHECK(next_dst_it >= dst_it);

    offset_t gap = static_cast<offset_t>(next_dst_it - dst_it);
    std::optional<ConstBufferView> extra_data = extra_data_source.GetNext(gap);
    if (!extra_data) {
      LOG(ERROR) << "Error reading extra_data";
      return false;
    }
    // |extra_data| length is based on what was parsed from the patch so this
    // copy should be valid.
    dst_it = base::ranges::copy(*extra_data, dst_it);
    CHECK_EQ(dst_it, next_dst_it);
    dst_it = std::copy_n(old_image.begin() + equivalence->src_offset,
                         equivalence->length, dst_it);
    CHECK_EQ(dst_it, next_dst_it + equivalence->length);
  }
  offset_t gap = static_cast<offset_t>(new_image.end() - dst_it);
  std::optional<ConstBufferView> extra_data = extra_data_source.GetNext(gap);
  if (!extra_data) {
    LOG(ERROR) << "Error reading extra_data";
    return false;
  }
  base::ranges::copy(*extra_data, dst_it);
  if (!equiv_source.Done() || !extra_data_source.Done()) {
    LOG(ERROR) << "Found trailing equivalence and extra_data";
    return false;
  }
  return true;
}

bool ApplyRawDelta(const PatchElementReader& patch_reader,
                   MutableBufferView new_image) {
  EquivalenceSource equiv_source = patch_reader.GetEquivalenceSource();
  RawDeltaSource raw_delta_source = patch_reader.GetRawDeltaSource();
  // Traverse |equiv_source| and |raw_delta_source| in lockstep.
  auto equivalence = equiv_source.GetNext();
  offset_t base_copy_offset = 0;
  for (auto delta = raw_delta_source.GetNext(); delta.has_value();
       delta = raw_delta_source.GetNext()) {
    while (equivalence.has_value() &&
           base_copy_offset + equivalence->length <= delta->copy_offset) {
      base_copy_offset += equivalence->length;
      equivalence = equiv_source.GetNext();
    }
    if (!equivalence.has_value()) {
      LOG(ERROR) << "Error reading equivalences";
      return false;
    }
    CHECK_GE(delta->copy_offset, base_copy_offset);
    CHECK_LT(delta->copy_offset, base_copy_offset + equivalence->length);

    // Invert byte diff.
    new_image[equivalence->dst_offset - base_copy_offset +
              delta->copy_offset] += delta->diff;
  }
  if (!raw_delta_source.Done()) {
    LOG(ERROR) << "Found trailing raw_delta";
    return false;
  }
  return true;
}

bool ApplyReferencesCorrection(ExecutableType exe_type,
                               ConstBufferView old_image,
                               const PatchElementReader& patch,
                               MutableBufferView new_image) {
  auto old_disasm = MakeDisassemblerOfType(old_image, exe_type);
  auto new_disasm =
      MakeDisassemblerOfType(ConstBufferView(new_image), exe_type);
  if (!old_disasm || !new_disasm) {
    LOG(ERROR) << "Failed to create Disassembler";
    return false;
  }
  if (old_disasm->size() != old_image.size() ||
      new_disasm->size() != new_image.size()) {
    LOG(ERROR) << "Disassembler and element size mismatch";
    return false;
  }

  ReferenceDeltaSource ref_delta_source = patch.GetReferenceDeltaSource();
  std::map<PoolTag, std::vector<ReferenceGroup>> pool_groups;
  for (const auto& ref_group : old_disasm->MakeReferenceGroups())
    pool_groups[ref_group.pool_tag()].push_back(ref_group);

  OffsetMapper offset_mapper(patch.GetEquivalenceSource(),
                             base::checked_cast<offset_t>(old_image.size()),
                             base::checked_cast<offset_t>(new_image.size()));

  std::vector<ReferenceGroup> new_groups = new_disasm->MakeReferenceGroups();
  for (const auto& pool_and_sub_groups : pool_groups) {
    PoolTag pool_tag = pool_and_sub_groups.first;
    const std::vector<ReferenceGroup>& sub_groups = pool_and_sub_groups.second;

    TargetPool targets;
    // Load "old" targets, then filter and map them to "new" targets.
    for (ReferenceGroup group : sub_groups)
      targets.InsertTargets(std::move(*group.GetReader(old_disasm.get())));
    targets.FilterAndProject(offset_mapper);

    // Load extra targets from patch.
    TargetSource target_source = patch.GetExtraTargetSource(pool_tag);
    targets.InsertTargets(&target_source);
    if (!target_source.Done()) {
      LOG(ERROR) << "Found trailing extra_targets";
      return false;
    }

    // Correct all new references, and write results to |new_disasm|.
    for (ReferenceGroup group : sub_groups) {
      std::unique_ptr<ReferenceWriter> ref_writer =
          new_groups[group.type_tag().value()].GetWriter(new_image,
                                                         new_disasm.get());

      EquivalenceSource equiv_source = patch.GetEquivalenceSource();
      for (auto equivalence = equiv_source.GetNext(); equivalence.has_value();
           equivalence = equiv_source.GetNext()) {
        std::unique_ptr<ReferenceReader> ref_gen = group.GetReader(
            equivalence->src_offset, equivalence->src_end(), old_disasm.get());
        for (auto ref = ref_gen->GetNext(); ref.has_value();
             ref = ref_gen->GetNext()) {
          DCHECK_GE(ref->location, equivalence->src_offset);
          DCHECK_LT(ref->location, equivalence->src_end());

          offset_t projected_target =
              offset_mapper.ExtendedForwardProject(ref->target);
          offset_t expected_key = targets.KeyForNearestOffset(projected_target);
          auto delta = ref_delta_source.GetNext();
          if (!delta.has_value()) {
            LOG(ERROR) << "Error reading reference_delta";
            return false;
          }
          const key_t key = expected_key + delta.value();
          if (!targets.KeyIsValid(key)) {
            LOG(ERROR) << "Invalid reference_delta";
            return false;
          }
          ref->target = targets.OffsetForKey(expected_key + delta.value());
          ref->location =
              ref->location - equivalence->src_offset + equivalence->dst_offset;
          ref_writer->PutNext(*ref);
        }
      }
    }
  }
  if (!ref_delta_source.Done()) {
    LOG(ERROR) << "Found trailing ref_delta_source";
    return false;
  }
  return true;
}

bool ApplyElement(ExecutableType exe_type,
                  ConstBufferView old_image,
                  const PatchElementReader& patch_reader,
                  MutableBufferView new_image) {
  return ApplyEquivalenceAndExtraData(old_image, patch_reader, new_image) &&
         ApplyRawDelta(patch_reader, new_image) &&
         ApplyReferencesCorrection(exe_type, old_image, patch_reader,
                                   new_image);
}

/******** Exported Functions ********/

status::Code ApplyBuffer(ConstBufferView old_image,
                         const EnsemblePatchReader& patch_reader,
                         MutableBufferView new_image) {
  if (!patch_reader.CheckOldFile(old_image)) {
    LOG(ERROR) << "Invalid old_image.";
    return status::kStatusInvalidOldImage;
  }

  for (const auto& element_patch : patch_reader.elements()) {
    ElementMatch match = element_patch.element_match();
    if (!ApplyElement(match.exe_type(), old_image[match.old_element.region()],
                      element_patch, new_image[match.new_element.region()]))
      return status::kStatusFatal;
  }

  if (!patch_reader.CheckNewFile(ConstBufferView(new_image))) {
    LOG(ERROR) << "Invalid new_image.";
    return status::kStatusInvalidNewImage;
  }
  return status::kStatusSuccess;
}

}  // namespace zucchini
