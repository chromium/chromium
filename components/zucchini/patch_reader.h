// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_PATCH_READER_H_
#define COMPONENTS_ZUCCHINI_PATCH_READER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <vector>

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "components/zucchini/buffer_source.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/image_utils.h"
#include "components/zucchini/patch_utils.h"

namespace zucchini {

namespace patch {

// The Parse*() functions below attempt to extract data of a specific type from
// the beginning of |source|. A parse function: On success, consumes the used
// portion of |source|, writes data into the output parameter, and returns
// true. Otherwise returns false and does not consume |source|.

// Parses |source| for the next ElementMatch.
bool ParseElementMatch(BufferSource* source, ElementMatch* element_match);

// Parses |source| for the next embedded BufferSource.
bool ParseBuffer(BufferSource* source, BufferSource* buffer);

// Parses |source| for the next VarUInt.
template <class T>
bool ParseVarUInt(BufferSource* source, T* value) {
  auto bytes_read = DecodeVarUInt(source->begin(), source->end(), value);
  if (!bytes_read) {
    LOG(ERROR) << "Impossible to read VarUInt from source.";
    LOG(ERROR) << base::debug::StackTrace().ToString();
    return false;
  }
  // Advance |source| beyond the VarUInt value.
  return source->Skip(bytes_read);
}

// Parses |source| for the next VarInt.
template <class T>
bool ParseVarInt(BufferSource* source, T* value) {
  auto bytes_read = DecodeVarInt(source->begin(), source->end(), value);
  if (!bytes_read) {
    LOG(ERROR) << "Impossible to read VarInt from source.";
    LOG(ERROR) << base::debug::StackTrace().ToString();
    return false;
  }
  // Advance |source| beyond the VarInt value.
  return source->Skip(bytes_read);
}

}  // namespace patch

// The *Source classes below are light-weight (i.e., allows copying) visitors to
// read patch data. Each of them has an associated "main type", and performs the
// following:
// - Consumes portions of a BufferSource (required to remain valid for the
//   lifetime of the object).
// - Decodes consumed data, which represent a list of items with "main type".
// - Dispenses "main type" elements (hence "Source" in the name).
//
// Common "core functions" implemented by *Source classes are:
// - bool Initialize(BufferSource* source): Consumes data from BufferSource and
//   initializes internal states. Returns true if successful, and false
//   otherwise (|source| may be partially consumed).
// - std::optional<MAIN_TYPE> GetNext(OPT_PARAMS): Decodes consumed data and
//   returns the next item as std::optional (returns std::nullopt on failure).
// - bool Done() const: Returns true if no more items remain; otherwise false.
//
// Usage of *Source instances don't mix, and GetNext() have dissimilar
// interfaces. Therefore we do not use inheritance to relate *Source  classes,
// and simply implement "core functions" with matching names.

// Source for Equivalences.
class EquivalenceSource {
 public:
  EquivalenceSource();
  EquivalenceSource(const EquivalenceSource&);
  ~EquivalenceSource();

  // Core functions.
  bool Initialize(BufferSource* source);
  std::optional<Equivalence> GetNext();
  bool Done() const {
    return src_skip_.empty() && dst_skip_.empty() && copy_count_.empty();
  }

  // Accessors for unittest.
  BufferSource src_skip() const { return src_skip_; }
  BufferSource dst_skip() const { return dst_skip_; }
  BufferSource copy_count() const { return copy_count_; }

 private:
  BufferSource src_skip_;
  BufferSource dst_skip_;
  BufferSource copy_count_;

  base::CheckedNumeric<offset_t> previous_src_offset_ = 0;
  base::CheckedNumeric<offset_t> previous_dst_offset_ = 0;
};

// Source for extra data.
class ExtraDataSource {
 public:
  ExtraDataSource();
  ExtraDataSource(const ExtraDataSource&);
  ~ExtraDataSource();

  // Core functions.
  bool Initialize(BufferSource* source);
  // |size| is the size in bytes of the buffer requested.
  std::optional<ConstBufferView> GetNext(offset_t size);
  bool Done() const { return extra_data_.empty(); }

  // Accessors for unittest.
  BufferSource extra_data() const { return extra_data_; }

 private:
  BufferSource extra_data_;
};

// Source for raw delta.
class RawDeltaSource {
 public:
  RawDeltaSource();
  RawDeltaSource(const RawDeltaSource&);
  ~RawDeltaSource();

  // Core functions.
  bool Initialize(BufferSource* source);
  std::optional<RawDeltaUnit> GetNext();
  bool Done() const {
    return raw_delta_skip_.empty() && raw_delta_diff_.empty();
  }

  // Accessors for unittest.
  BufferSource raw_delta_skip() const { return raw_delta_skip_; }
  BufferSource raw_delta_diff() const { return raw_delta_diff_; }

 private:
  BufferSource raw_delta_skip_;
  BufferSource raw_delta_diff_;

  base::CheckedNumeric<offset_t> copy_offset_compensation_ = 0;
};

// Source for reference delta.
class ReferenceDeltaSource {
 public:
  ReferenceDeltaSource();
  ReferenceDeltaSource(const ReferenceDeltaSource&);
  ~ReferenceDeltaSource();

  // Core functions.
  bool Initialize(BufferSource* source);
  std::optional<int32_t> GetNext();
  bool Done() const { return source_.empty(); }

  // Accessors for unittest.
  BufferSource reference_delta() const { return source_; }

 private:
  BufferSource source_;
};

// Source for additional targets.
class TargetSource {
 public:
  TargetSource();
  TargetSource(const TargetSource&);
  ~TargetSource();

  // Core functions.
  bool Initialize(BufferSource* source);
  std::optional<offset_t> GetNext();
  bool Done() const { return extra_targets_.empty(); }

  // Accessors for unittest.
  BufferSource extra_targets() const { return extra_targets_; }

 private:
  BufferSource extra_targets_;

  base::CheckedNumeric<offset_t> target_compensation_ = 0;
};

// Following are utility classes providing a structured view on data forming a
// patch.

// Utility to read a patch element. A patch element contains all the information
// necessary to patch a single element. This class provide access
// to the multiple streams of data forming the patch element.
class PatchElementReader {
 public:
  PatchElementReader();
  PatchElementReader(PatchElementReader&&);
  ~PatchElementReader();

  // If data read from |source| is well-formed, initialize cached sources to
  // read from it, and returns true. Otherwise returns false.
  bool Initialize(BufferSource* source);

  const ElementMatch& element_match() const { return element_match_; }
  const Element& old_element() const { return element_match_.old_element; }
  const Element& new_element() const { return element_match_.new_element; }

  // The Get*() functions below return copies of cached sources. Callers may
  // assume the following:
  // - Equivalences satisfy basic boundary constraints
  //   - "Old" / "new" blocks lie entirely in "old" / "new" images.
  //   - "New" blocks are sorted.
  EquivalenceSource GetEquivalenceSource() const { return equivalences_; }
  ExtraDataSource GetExtraDataSource() const { return extra_data_; }
  RawDeltaSource GetRawDeltaSource() const { return raw_delta_; }
  ReferenceDeltaSource GetReferenceDeltaSource() const {
    return reference_delta_;
  }
  TargetSource GetExtraTargetSource(PoolTag tag) const {
    auto pos = extra_targets_.find(tag);
    return pos != extra_targets_.end() ? pos->second : TargetSource();
  }

 private:
  // Checks that "old" and "new" blocks of each item in |equivalences_| satisfy
  // basic order and image bound constraints (using |element_match_| data). Also
  // validates that the amount of extra data is correct. Returns true if
  // successful.
  bool ValidateEquivalencesAndExtraData();

  ElementMatch element_match_;

  // Cached sources.
  EquivalenceSource equivalences_;
  ExtraDataSource extra_data_;
  RawDeltaSource raw_delta_;
  ReferenceDeltaSource reference_delta_;
  std::map<PoolTag, TargetSource> extra_targets_;
};

// Utility to read a Zucchini ensemble patch. An ensemble patch is the
// concatenation of a patch header with a vector of patch elements.
class EnsemblePatchReader {
 public:
  // If data read from |buffer| is well-formed, initializes and returns
  // an instance of EnsemblePatchReader. Otherwise returns std::nullopt.
  static std::optional<EnsemblePatchReader> Create(ConstBufferView buffer);

  EnsemblePatchReader();
  EnsemblePatchReader(EnsemblePatchReader&&);
  ~EnsemblePatchReader();

  // If data read from |source| is well-formed, initialize internal state to
  // read from it, and returns true. Otherwise returns false.
  bool Initialize(BufferSource* source);

  // Check old / new image file validity, comparing against expected size and
  // CRC32. Return true if file matches expectations, false otherwise.
  bool CheckOldFile(ConstBufferView old_image) const;
  bool CheckNewFile(ConstBufferView new_image) const;

  const PatchHeader& header() const { return header_; }
  const std::vector<PatchElementReader>& elements() const { return elements_; }

 private:
  PatchHeader header_;
  std::vector<PatchElementReader> elements_;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_PATCH_READER_H_
