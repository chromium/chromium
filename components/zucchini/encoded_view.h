// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ENCODED_VIEW_H_
#define COMPONENTS_ZUCCHINI_ENCODED_VIEW_H_

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/zucchini/image_index.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

// Zucchini-gen performs semantics-aware matching:
// - Same-typed reference target in "old" and "new" can be associated.
//   Associated targets are assigned an identifier called "label" (and for
//   unassociated targets, label = 0).
// - EncodedView maps each offset in "old" and "new" images to a "projected
//   value", which can be:
//   - Raw byte value (0-255) for non-references.
//   - Reference "projected value" (> 256) that depends on target {type, label}
//     at each reference's location (byte 0).
//   - Reference padding value (256) at the body of each reference (bytes 1+).
// - The projected values for "old" and "new" are used to build the equivalence
//   map.

constexpr size_t kReferencePaddingProjection = 256;
constexpr size_t kBaseReferenceProjection = 257;

// A Range (providing begin and end iterators) that adapts ImageIndex to make
// image data appear as an Encoded Image, that is encoded data under a higher
// level of abstraction than raw bytes. In particular:
// - First byte of each reference become a projection of its type and label.
// - Subsequent bytes of each reference becomes |kReferencePaddingProjection|.
// - Non-reference raw bytes remain as raw bytes.
class EncodedView {
 public:
  // RandomAccessIterator whose values are the results of Projection().
  class Iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = size_t;
    using pointer = size_t*;

    Iterator(const EncodedView* encoded_view, difference_type pos)
        : encoded_view_(encoded_view), pos_(pos) {}

    Iterator(const Iterator&) = default;

    Iterator& operator=(const Iterator&) = default;

    value_type operator*() const {
      return encoded_view_->Projection(static_cast<offset_t>(pos_));
    }

    value_type operator[](difference_type n) const {
      return encoded_view_->Projection(static_cast<offset_t>(pos_ + n));
    }

    Iterator& operator++() {
      ++pos_;
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp = *this;
      ++pos_;
      return tmp;
    }

    Iterator& operator--() {
      --pos_;
      return *this;
    }

    Iterator operator--(int) {
      Iterator tmp = *this;
      --pos_;
      return tmp;
    }

    Iterator& operator+=(difference_type n) {
      pos_ += n;
      return *this;
    }

    Iterator& operator-=(difference_type n) {
      pos_ -= n;
      return *this;
    }

    friend bool operator==(Iterator a, Iterator b) { return a.pos_ == b.pos_; }

    friend bool operator!=(Iterator a, Iterator b) { return !(a == b); }

    friend bool operator<(Iterator a, Iterator b) { return a.pos_ < b.pos_; }

    friend bool operator>(Iterator a, Iterator b) { return b < a; }

    friend bool operator<=(Iterator a, Iterator b) { return !(b < a); }

    friend bool operator>=(Iterator a, Iterator b) { return !(a < b); }

    friend difference_type operator-(Iterator a, Iterator b) {
      return a.pos_ - b.pos_;
    }

    friend Iterator operator+(Iterator it, difference_type n) {
      it += n;
      return it;
    }

    friend Iterator operator-(Iterator it, difference_type n) {
      it -= n;
      return it;
    }

   private:
    raw_ptr<const EncodedView> encoded_view_;
    difference_type pos_;
  };

  using value_type = size_t;
  using size_type = offset_t;
  using difference_type = ptrdiff_t;
  using const_iterator = Iterator;

  // |image_index| is the annotated image being adapted, and is required to
  // remain valid for the lifetime of the object.
  explicit EncodedView(const ImageIndex& image_index);
  EncodedView(const EncodedView&) = delete;
  const EncodedView& operator=(const EncodedView&) = delete;
  ~EncodedView();

  // Projects |location| to a scalar value that describes the content at a
  // higher level of abstraction.
  value_type Projection(offset_t location) const;

  bool IsToken(offset_t location) const {
    return image_index_->IsToken(location);
  }

  // Returns the cardinality of the projection, i.e., the upper bound on
  // values returned by Projection().
  value_type Cardinality() const;

  // Associates |labels| to targets for a given |pool|, replacing previous
  // association. Values in |labels| must be smaller than |bound|.
  void SetLabels(PoolTag pool, std::vector<uint32_t>&& labels, size_t bound);
  const ImageIndex& image_index() const { return *image_index_; }

  // Range functions.
  size_type size() const { return size_type(image_index_->size()); }
  const_iterator begin() const {
    return const_iterator{this, difference_type(0)};
  }
  const_iterator end() const {
    return const_iterator{this, difference_type(size())};
  }

 private:
  struct PoolInfo {
    PoolInfo();
    PoolInfo(PoolInfo&&);
    ~PoolInfo();

    // |labels| translates IndirectReference target_key to label.
    std::vector<uint32_t> labels;
    size_t bound = 0;
  };

  const raw_ref<const ImageIndex> image_index_;
  std::vector<PoolInfo> pool_infos_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ENCODED_VIEW_H_
