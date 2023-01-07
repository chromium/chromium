// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_COMMON_EXPANDED_VALUE_BASE_H_
#define MEDIA_CAST_COMMON_EXPANDED_VALUE_BASE_H_

#include <stdint.h>

#include <limits>

#include "base/check_op.h"

namespace media {
namespace cast {

// Abstract base template class for common "sequence value" data types such as
// RtpTimeTicks, FrameId, or PacketId which generally increment/decrement in
// predictable amounts as media is streamed, and which often need to be reliably
// truncated and re-expanded for over-the-wire transmission.
//
// FullWidthInteger should be a signed integer POD type that is of sufficiently
// high width (in bits) such that it is never expected to under/overflow during
// the longest reasonable length of continuous system operation.  Subclass is
// the class inheriting the common functionality provided in this template, and
// is used to provide operator overloads.  The Subclass must friend this class
// to enable these operator overloads.
//
// Please see RtpTimeTicks and unit test code for examples of how to define
// Subclasses and add features specific to their concrete data type, and how to
// use data types derived from ExpandedValueBase.  For example, a RtpTimeTicks
// adds math operators consisting of the meaningful and valid set of operations
// allowed for doing "time math."  On the other hand, FrameId only adds math
// operators for incrementing/decrementing since multiplication and division are
// meaningless.
template <typename FullWidthInteger, class Subclass>
class ExpandedValueBase {
  static_assert(std::numeric_limits<FullWidthInteger>::is_signed,
                "FullWidthInteger must be a signed integer.");
  static_assert(std::numeric_limits<FullWidthInteger>::is_integer,
                "FullWidthInteger must be a signed integer.");

 public:
  // Methods that return the lower bits of this value.  This should only be used
  // for serializing/wire-formatting, and not to subvert the restricted set of
  // operators allowed on this data type.
  uint8_t lower_8_bits() const { return static_cast<uint8_t>(value_); }
  uint16_t lower_16_bits() const { return static_cast<uint16_t>(value_); }
  uint32_t lower_32_bits() const { return static_cast<uint32_t>(value_); }

  // Compute the greatest value less than or equal to |this| value whose lower
  // bits are those of |x|.  The purpose of this method is to re-instantiate an
  // original value from its truncated form, usually when deserializing
  // off-the-wire, when |this| value is known to be the greatest possible valid
  // value.
  //
  // Use case example: Start with an original 32-bit value of 0x000001fe (510
  // decimal) and truncate, throwing away its upper 24 bits: 0xfe.  Now, send
  // this truncated value over-the-wire to a peer who needs to expand it back to
  // the original 32-bit value.  The peer knows that the greatest possible valid
  // value is 0x00000202 (514 decimal).  This method will initially attempt to
  // just concatenate the upper 24 bits of |this->value_| with |x| (the 8-bit
  // value), and get a result of 0x000002fe (766 decimal).  However, this is
  // greater than |this->value_|, so the upper 24 bits are subtracted by one to
  // get 0x000001fe, which is the original value.
  template <typename ShortUnsigned>
  Subclass ExpandLessThanOrEqual(ShortUnsigned x) const {
    static_assert(!std::numeric_limits<ShortUnsigned>::is_signed,
                  "|x| must be an unsigned integer.");
    static_assert(std::numeric_limits<ShortUnsigned>::is_integer,
                  "|x| must be an unsigned integer.");
    static_assert(sizeof(ShortUnsigned) <= sizeof(FullWidthInteger),
                  "|x| must fit within the FullWidthInteger.");

    if (sizeof(ShortUnsigned) < sizeof(FullWidthInteger)) {
      // Initially, the |result| is composed of upper bits from |value_| and
      // lower bits from |x|.
      const FullWidthInteger short_max =
          std::numeric_limits<ShortUnsigned>::max();
      FullWidthInteger result = (value_ & ~short_max) | x;

      // If the |result| is larger than |value_|, decrement the upper bits by
      // one.  In other words, |x| must always be interpreted as a truncated
      // version of a value less than or equal to |value_|.
      if (result > value_)
        result -= short_max + 1;

      return Subclass(result);
    } else {
      // Debug builds: Ensure the highest bit is not set (which would cause
      // overflow when casting to the signed integer).
      DCHECK_EQ(static_cast<ShortUnsigned>(0),
                x & (static_cast<ShortUnsigned>(1) << ((sizeof(x) * 8) - 1)));
      return Subclass(x);
    }
  }

  // Compute the value closest to |this| value whose lower bits are those of
  // |x|.  The result is always within |max_distance_for_expansion()| of |this|
  // value.  The purpose of this method is to re-instantiate an original value
  // from its truncated form, usually when deserializing off-the-wire.  See
  // comments for ExpandLessThanOrEqual() above for further explanation.
  template <typename ShortUnsigned>
  Subclass Expand(ShortUnsigned x) const {
    const Subclass maximum_possible_result(
        value_ + max_distance_for_expansion<ShortUnsigned>());
    return maximum_possible_result.ExpandLessThanOrEqual(x);
  }

  // Comparison operators.
  bool operator==(
      const ExpandedValueBase<FullWidthInteger, Subclass>& rhs) const {
    return value_ == rhs.value_;
  }
  bool operator!=(
      const ExpandedValueBase<FullWidthInteger, Subclass>& rhs) const {
    return value_ != rhs.value_;
  }
  bool operator<(
      const ExpandedValueBase<FullWidthInteger, Subclass>& rhs) const {
    return value_ < rhs.value_;
  }
  bool operator>(
      const ExpandedValueBase<FullWidthInteger, Subclass>& rhs) const {
    return value_ > rhs.value_;
  }
  bool operator<=(
      const ExpandedValueBase<FullWidthInteger, Subclass>& rhs) const {
    return value_ <= rhs.value_;
  }
  bool operator>=(
      const ExpandedValueBase<FullWidthInteger, Subclass>& rhs) const {
    return value_ >= rhs.value_;
  }

  // (De)Serialize for transmission over IPC.  Do not use these to subvert the
  // valid set of operators allowed by this class or its Subclass.
  uint64_t SerializeForIPC() const {
    static_assert(sizeof(uint64_t) >= sizeof(FullWidthInteger),
                  "Cannot serialize FullWidthInteger into an uint64_t.");
    return static_cast<uint64_t>(value_);
  }
  static Subclass DeserializeForIPC(uint64_t serialized) {
    return Subclass(static_cast<FullWidthInteger>(serialized));
  }

  // Design limit: Values that are truncated to the ShortUnsigned type must be
  // no more than this maximum distance from each other in order to ensure the
  // original value can be determined correctly.
  template <typename ShortUnsigned>
  static constexpr FullWidthInteger max_distance_for_expansion() {
    return std::numeric_limits<ShortUnsigned>::max() / 2;
  }

 protected:
  // Only subclasses are permitted to instantiate directly.
  explicit ExpandedValueBase(FullWidthInteger value) : value_(value) {}

  FullWidthInteger value_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_COMMON_EXPANDED_VALUE_BASE_H_
