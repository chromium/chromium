// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_PATCH_WRITER_H_
#define COMPONENTS_ZUCCHINI_PATCH_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "components/zucchini/buffer_sink.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/patch_utils.h"

namespace zucchini {

namespace patch {

// If sufficient space is available, serializes |element_match| into |sink| and
// returns true. Otherwise returns false, and |sink| will be in an undefined
// state.
bool SerializeElementMatch(const ElementMatch& element_match, BufferSink* sink);

// Returns the size in bytes required to serialize |element_match|.
size_t SerializedElementMatchSize(const ElementMatch& element_match);

// If sufficient space is available, serializes |buffer| into |sink| and returns
// true. Otherwise returns false, and |sink| will be in an undefined state.
bool SerializeBuffer(const std::vector<uint8_t>& buffer, BufferSink* sink);

// Returns the size in bytes required to serialize |buffer|.
size_t SerializedBufferSize(const std::vector<uint8_t>& buffer);

}  // namespace patch

// Each of *Sink classes below has an associated "main type", and performs the
// following:
// - Receives multiple "main type" elements (hence "Sink" in the name).
// - Encodes list of received data, and writes them to internal storage (e.g.,
//   applying delta encoding).
// - Writes encoded data to BufferSink.
//
// Common "core functions" implemented for *Sink classes are:
// - void PutNext(const MAIN_TYPE& inst): Encodes and writes an instance of
//   MAIN_TYPE to internal storage. Assumptions may be applied to successive
//   |inst| provided.
// - size_t SerializedSize() const: Returns the serialized size in bytes of
//   internal storage.
// - bool SerializeInto(BufferSink* sink) const: If |sink| has enough space,
//   serializes internal storage into |sink|, and returns true. Otherwise
//   returns false.
//
// Usage of *Sink instances don't mix, and PuttNext() have dissimilar
// interfaces. Therefore we do not use inheritance to relate *Sink classes,
// simply implement "core functions" with matching names.

// Sink for equivalences.
class EquivalenceSink {
 public:
  EquivalenceSink();
  EquivalenceSink(const std::vector<uint8_t>& src_skip,
                  const std::vector<uint8_t>& dst_skip,
                  const std::vector<uint8_t>& copy_count);

  EquivalenceSink(EquivalenceSink&&);
  ~EquivalenceSink();

  // Core functions.
  // Equivalences must be given by increasing |Equivalence::dst_offset|.
  void PutNext(const Equivalence& equivalence);
  size_t SerializedSize() const;
  bool SerializeInto(BufferSink* sink) const;

 private:
  // Offset in source, delta-encoded starting from end of last equivalence, and
  // stored as signed varint.
  std::vector<uint8_t> src_skip_;
  // Offset in destination, delta-encoded starting from end of last equivalence,
  // and stored as unsigned varint.
  std::vector<uint8_t> dst_skip_;
  // Length of equivalence stored as unsigned varint.
  // TODO(etiennep): Investigate on bias.
  std::vector<uint8_t> copy_count_;

  offset_t src_offset_ = 0;  // Last offset in source.
  offset_t dst_offset_ = 0;  // Last offset in destination.
};

// Sink for extra data.
class ExtraDataSink {
 public:
  ExtraDataSink();
  explicit ExtraDataSink(const std::vector<uint8_t>& extra_data);
  ExtraDataSink(ExtraDataSink&&);
  ~ExtraDataSink();

  // Core functions.
  void PutNext(ConstBufferView region);
  size_t SerializedSize() const;
  bool SerializeInto(BufferSink* sink) const;

 private:
  std::vector<uint8_t> extra_data_;
};

// Sink for raw delta.
class RawDeltaSink {
 public:
  RawDeltaSink();
  RawDeltaSink(const std::vector<uint8_t>& raw_delta_skip,
               const std::vector<uint8_t>& raw_delta_diff);
  RawDeltaSink(RawDeltaSink&&);
  ~RawDeltaSink();

  // Core functions.
  // Deltas must be given by increasing |RawDeltaUnit::copy_offset|.
  void PutNext(const RawDeltaUnit& delta);
  size_t SerializedSize() const;
  bool SerializeInto(BufferSink* sink) const;

 private:
  std::vector<uint8_t> raw_delta_skip_;  // Copy offset stating from last delta.
  std::vector<uint8_t> raw_delta_diff_;  // Bytewise difference.

  // We keep track of the compensation needed for next copy offset, taking into
  // accound delta encoding and bias of -1. Stored delta are biased by -1, so a
  // sequence of single byte deltas is represented as a string of 0's.
  offset_t copy_offset_compensation_ = 0;
};

// Sink for reference delta.
class ReferenceDeltaSink {
 public:
  ReferenceDeltaSink();
  explicit ReferenceDeltaSink(const std::vector<uint8_t>& reference_delta);
  ReferenceDeltaSink(ReferenceDeltaSink&&);
  ~ReferenceDeltaSink();

  // Core functions.
  void PutNext(int32_t diff);
  size_t SerializedSize() const;
  bool SerializeInto(BufferSink* sink) const;

 private:
  std::vector<uint8_t> reference_delta_;
};

// Sink for additional targets.
class TargetSink {
 public:
  TargetSink();
  explicit TargetSink(const std::vector<uint8_t>& extra_targets);
  TargetSink(TargetSink&&);
  ~TargetSink();

  // Core functions.
  // Targets must be given by increasing order.
  void PutNext(uint32_t target);
  size_t SerializedSize() const;
  bool SerializeInto(BufferSink* sink) const;

 private:
  // Targets are delta-encoded and biaised by 1, stored as unsigned varint.
  std::vector<uint8_t> extra_targets_;

  // We keep track of the compensation needed for next target, taking into
  // accound delta encoding and bias of -1.
  offset_t target_compensation_ = 0;
};

// Following are utility classes to write structured data forming a patch.

// Utility to write a patch element. A patch element contains all the
// information necessary to patch a single element. This class
// provides an interface to individually set different building blocks of data
// in the patch element.
class PatchElementWriter {
 public:
  PatchElementWriter();
  explicit PatchElementWriter(ElementMatch element_match);
  PatchElementWriter(PatchElementWriter&&);
  ~PatchElementWriter();

  const ElementMatch& element_match() const { return element_match_; }
  const Element& old_element() const { return element_match_.old_element; }
  const Element& new_element() const { return element_match_.new_element; }

  // Following methods set individual blocks for this element. Previous
  // corresponding block is replaced. All streams must be set before call to
  // SerializedSize() of SerializeInto().

  void SetEquivalenceSink(EquivalenceSink&& equivalences) {
    equivalences_.emplace(std::move(equivalences));
  }
  void SetExtraDataSink(ExtraDataSink&& extra_data) {
    extra_data_.emplace(std::move(extra_data));
  }
  void SetRawDeltaSink(RawDeltaSink&& raw_delta) {
    raw_delta_.emplace(std::move(raw_delta));
  }
  void SetReferenceDeltaSink(ReferenceDeltaSink reference_delta) {
    reference_delta_.emplace(std::move(reference_delta));
  }
  // Set additional targets for pool identified with |pool_tag|.
  void SetTargetSink(PoolTag pool_tag, TargetSink&& extra_targets) {
    DCHECK(pool_tag != kNoPoolTag);
    extra_targets_.emplace(pool_tag, std::move(extra_targets));
  }

  // Returns the serialized size in bytes of the data this object is holding.
  size_t SerializedSize() const;

  // If sufficient space is available, serializes data into |sink|, which is at
  // least SerializedSize() bytes, and returns true. Otherwise returns false.
  bool SerializeInto(BufferSink* sink) const;

 private:
  ElementMatch element_match_;
  std::optional<EquivalenceSink> equivalences_;
  std::optional<ExtraDataSink> extra_data_;
  std::optional<RawDeltaSink> raw_delta_;
  std::optional<ReferenceDeltaSink> reference_delta_;
  std::map<PoolTag, TargetSink> extra_targets_;
};

// Utility to write a Zucchini ensemble patch. An ensemble patch is the
// concatenation of a patch header with a vector of patch elements.
class EnsemblePatchWriter {
 public:
  explicit EnsemblePatchWriter(const PatchHeader& header);
  EnsemblePatchWriter(ConstBufferView old_image, ConstBufferView new_image);
  EnsemblePatchWriter(const EnsemblePatchWriter&) = delete;
  const EnsemblePatchWriter& operator=(const EnsemblePatchWriter&) = delete;
  ~EnsemblePatchWriter();

  // Reserves space for |count| patch elements.
  void ReserveElements(size_t count) { elements_.reserve(count); }

  // Adds an patch element into the patch. Patch elements must be ordered by
  // their location in the new image file.
  void AddElement(PatchElementWriter&& patch_element);

  // Returns the serialized size in bytes of the data this object is holding.
  size_t SerializedSize() const;

  // If sufficient space is available, serializes data into |sink|, which is at
  // least SerializedSize() bytes, and returns true. Otherwise returns false.
  bool SerializeInto(BufferSink* sink) const;

  // If sufficient space is available, serializes data into |buffer|, which is
  // at least SerializedSize() bytes, and returns true. Otherwise returns false.
  bool SerializeInto(MutableBufferView buffer) const {
    BufferSink sink(buffer);
    return SerializeInto(&sink);
  }

 private:
  PatchHeader header_;
  std::vector<PatchElementWriter> elements_;
  offset_t current_dst_offset_ = 0;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_PATCH_WRITER_H_
