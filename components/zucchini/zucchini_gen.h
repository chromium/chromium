// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ZUCCHINI_GEN_H_
#define COMPONENTS_ZUCCHINI_ZUCCHINI_GEN_H_

#include <vector>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/zucchini.h"

namespace zucchini {

class EquivalenceMap;
class OffsetMapper;
class ImageIndex;
class PatchElementWriter;
class ReferenceDeltaSink;
class ReferenceSet;
class TargetPool;

// Extract all targets in |new_targets| with no associated target in
// |projected_old_targets| and returns these targets in a new vector.
std::vector<offset_t> FindExtraTargets(const TargetPool& projected_old_targets,
                                       const TargetPool& new_targets);

// Creates an EquivalenceMap from "old" image to "new" image and returns the
// result. The params |*_image_index|:
// - Provide "old" and "new" raw image data and references.
// - Mediate Label matching, which links references between "old" and "new", and
//   guides EquivalenceMap construction.
EquivalenceMap CreateEquivalenceMap(const ImageIndex& old_image_index,
                                    const ImageIndex& new_image_index);

// Writes equivalences from |equivalence_map|, and extra data from |new_image|
// found in gaps between equivalences to |patch_writer|.
bool GenerateEquivalencesAndExtraData(ConstBufferView new_image,
                                      const EquivalenceMap& equivalence_map,
                                      PatchElementWriter* patch_writer);

// Writes raw delta between |old_image| and |new_image| matched by
// |equivalence_map| to |patch_writer|, using |new_image_index| to ignore
// reference bytes.
bool GenerateRawDelta(
    ConstBufferView old_image,
    ConstBufferView new_image,
    const EquivalenceMap& equivalence_map,
    const ImageIndex& new_image_index,
    const std::map<TypeTag, std::unique_ptr<ReferenceMixer>>& reference_mixers,
    PatchElementWriter* patch_writer);

// Writes reference delta between references from |old_refs| and from
// |new_refs| to |patch_writer|. |projected_target_pool| contains projected
// targets from old to new image for references pool associated with |new_refs|.
bool GenerateReferencesDelta(const ReferenceSet& src_refs,
                             const ReferenceSet& dst_refs,
                             const TargetPool& projected_target_pool,
                             const OffsetMapper& offset_mapper,
                             const EquivalenceMap& equivalence_map,
                             ReferenceDeltaSink* reference_delta_sink);

// Writes |extra_targets| associated with |pool_tag| to |patch_writer|.
bool GenerateExtraTargets(const std::vector<offset_t>& extra_targets,
                          PoolTag pool_tag,
                          PatchElementWriter* patch_writer);

// Generates raw patch element data between |old_image| and |new_image|, and
// writes them to |patch_writer|. |old_sa| is the suffix array for |old_image|.
bool GenerateRawElement(const std::vector<offset_t>& old_sa,
                        ConstBufferView old_image,
                        ConstBufferView new_image,
                        PatchElementWriter* patch_writer);

// Generates patch element of type |exe_type| from |old_image| to |new_image|,
// and writes it to |patch_writer|.
bool GenerateExecutableElement(ExecutableType exe_type,
                               ConstBufferView old_image,
                               ConstBufferView new_image,
                               PatchElementWriter* patch_writer);

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ZUCCHINI_GEN_H_
