// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_BUFFER_SINK_H_
#define COMPONENTS_ZUCCHINI_BUFFER_SINK_H_

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <iterator>

#include "base/check_op.h"
#include "components/zucchini/buffer_view.h"

namespace zucchini {

// BufferSink acts like an output stream with convenience methods to serialize
// data into a contiguous sequence of raw data. The underlying MutableBufferView
// emulates a cursor to track current write position, and guards against buffer
// overrun. Where applicable, BufferSink should be passed by pointer to maintain
// cursor progress across writes.
class BufferSink : public MutableBufferView {
 public:
  using iterator = MutableBufferView::iterator;

  using MutableBufferView::MutableBufferView;
  BufferSink() = default;
  explicit BufferSink(MutableBufferView buffer);
  BufferSink(const BufferSink&) = default;
  BufferSink& operator=(BufferSink&&) = default;

  // If sufficient space is available, writes the binary representation of
  // |value| starting at the cursor, while advancing the cursor beyond the
  // written region, and returns true. Otherwise returns false.
  template <class T>
  bool PutValue(const T& value) {
    DCHECK_NE(begin(), nullptr);
    if (Remaining() < sizeof(T))
      return false;
    ::memcpy(begin(), &value, sizeof(T));
    remove_prefix(sizeof(T));
    return true;
  }

  // If sufficient space is available, writes the raw bytes [|first|, |last|)
  // starting at the cursor, while advancing the cursor beyond the written
  // region, and returns true. Otherwise returns false.
  template <class It>
  bool PutRange(It first, It last) {
    static_assert(sizeof(typename std::iterator_traits<It>::value_type) ==
                      sizeof(uint8_t),
                  "value_type should fit in uint8_t");
    DCHECK_NE(begin(), nullptr);
    DCHECK(last >= first);
    if (Remaining() < size_type(last - first))
      return false;
    std::copy(first, last, begin());
    remove_prefix(last - first);
    return true;
  }

  size_type Remaining() const { return size(); }
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_BUFFER_SINK_H_
