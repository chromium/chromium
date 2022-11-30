// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_IMAGE_INDEX_H_
#define COMPONENTS_ZUCCHINI_IMAGE_INDEX_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/check_op.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/reference_set.h"
#include "components/zucchini/target_pool.h"

namespace zucchini {

class Disassembler;

// A class that holds annotations of an image, allowing quick access to its raw
// and reference content. The memory overhead of storing all references is
// relatively high, so this is only used during patch generation.
class ImageIndex {
 public:
  explicit ImageIndex(ConstBufferView image);
  ImageIndex(const ImageIndex&) = delete;
  ImageIndex(ImageIndex&&);
  ~ImageIndex();

  // Inserts all references read from |disasm|. This should be called exactly
  // once. If overlap between any two references of any type is encountered,
  // returns false and leaves the object in an invalid state. Otherwise,
  // returns true.
  // TODO(huangs): Refactor ReaderFactory and WriterFactory so
  // |const Disassembler&| can be used here.
  bool Initialize(Disassembler* disasm);

  // Returns the array size needed to accommodate all reference type values.
  size_t TypeCount() const {
    if (reference_sets_.empty())
      return 0U;
    return reference_sets_.rbegin()->first.value() + 1;  // Max key + 1.
  }

  // Returns the array size needed to accommodate all pool values.
  size_t PoolCount() const {
    if (target_pools_.empty())
      return 0U;
    return target_pools_.rbegin()->first.value() + 1;  // Max key + 1.
  }

  // Returns true if |image_[location]| is either:
  // - A raw value.
  // - The first byte of a reference.
  bool IsToken(offset_t location) const;

  // Returns true if |image_[location]| is part of a reference.
  bool IsReference(offset_t location) const {
    return LookupType(location) != kNoTypeTag;
  }

  // Returns the type tag of the reference covering |location|, or kNoTypeTag if
  // |location| is not part of a reference.
  TypeTag LookupType(offset_t location) const {
    DCHECK_LT(location, size());
    return type_tags_[location];
  }

  // Returns the raw value at |location|.
  uint8_t GetRawValue(offset_t location) const {
    DCHECK_LT(location, size());
    return image_[location];
  }

  const std::map<PoolTag, TargetPool>& target_pools() const {
    return target_pools_;
  }
  const std::map<TypeTag, ReferenceSet>& reference_sets() const {
    return reference_sets_;
  }

  const TargetPool& pool(PoolTag pool_tag) const {
    return target_pools_.at(pool_tag);
  }
  const ReferenceSet& refs(TypeTag type_tag) const {
    return reference_sets_.at(type_tag);
  }

  // Returns the size of the image.
  size_t size() const { return image_.size(); }

 private:
  // Inserts to |*this| index, all references described by |traits| read from
  // |ref_reader|, which gets consumed. This should be called exactly once for
  // each reference type. If overlap between any two references of any type is
  // encountered, returns false and leaves the object in an invalid state.
  // Otherwise, returns true.
  bool InsertReferences(const ReferenceTypeTraits& traits,
                        ReferenceReader&& ref_reader);

  const ConstBufferView image_;

  // Used for random access lookup of reference type, for each byte in |image_|.
  std::vector<TypeTag> type_tags_;

  std::map<PoolTag, TargetPool> target_pools_;
  std::map<TypeTag, ReferenceSet> reference_sets_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_IMAGE_INDEX_H_
