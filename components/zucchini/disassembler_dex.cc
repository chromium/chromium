// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/disassembler_dex.h"

#include <stddef.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/zucchini/buffer_source.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/io_utils.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace zucchini {

namespace {

// A DEX item specified by an offset, if absent, has a sentinel value of 0 since
// 0 is never a valid item offset (it points to magic at start of DEX).
constexpr offset_t kDexSentinelOffset = 0U;

// A DEX item specified by an index, if absent, has a sentinel value of
// NO_INDEX = 0xFFFFFFFF. This is represented as an offset_t for uniformity.
constexpr offset_t kDexSentinelIndexAsOffset = 0xFFFFFFFFU;

static_assert(kDexSentinelIndexAsOffset != kInvalidOffset,
              "Sentinel should not be confused with invalid offset.");

// Size of a Dalvik instruction unit. Need to cast to signed int because
// sizeof() gives size_t, which dominates when operated on ptrdiff_t, then
// wrecks havoc for base::checked_cast<int16_t>().
constexpr int kInstrUnitSize = static_cast<int>(sizeof(uint16_t));

// Checks if |offset| is byte aligned to 32 bits or 4 bytes.
bool Is32BitAligned(offset_t offset) {
  return offset % 4 == 0;
}

// Returns a lower bound for the size of an item of type |type_item_code|.
// - For fixed-length items (e.g., kTypeFieldIdItem) this is the exact size.
// - For variant-length items (e.g., kTypeCodeItem), returns a value that is
//   known to be less than the item length (e.g., header size).
// - For items not handled by this function, returns 1 for sanity check.
size_t GetItemBaseSize(uint16_t type_item_code) {
  switch (type_item_code) {
    case dex::kTypeStringIdItem:
      return sizeof(dex::StringIdItem);
    case dex::kTypeTypeIdItem:
      return sizeof(dex::TypeIdItem);
    case dex::kTypeProtoIdItem:
      return sizeof(dex::ProtoIdItem);
    case dex::kTypeFieldIdItem:
      return sizeof(dex::FieldIdItem);
    case dex::kTypeMethodIdItem:
      return sizeof(dex::MethodIdItem);
    case dex::kTypeClassDefItem:
      return sizeof(dex::ClassDefItem);
    case dex::kTypeCallSiteIdItem:
      return sizeof(dex::CallSiteIdItem);
    case dex::kTypeMethodHandleItem:
      return sizeof(dex::MethodHandleItem);
    // No need to handle dex::kTypeMapList.
    case dex::kTypeTypeList:
      return sizeof(uint32_t);  // Variable-length.
    case dex::kTypeAnnotationSetRefList:
      return sizeof(uint32_t);  // Variable-length.
    case dex::kTypeAnnotationSetItem:
      return sizeof(uint32_t);  // Variable-length.
    case dex::kTypeCodeItem:
      return sizeof(dex::CodeItem);  // Variable-length.
    case dex::kTypeAnnotationsDirectoryItem:
      return sizeof(dex::AnnotationsDirectoryItem);  // Variable-length.
    default:
      return 1U;  // Unhandled item. For sanity check assume size >= 1.
  }
}

/******** CodeItemParser ********/

// A parser to extract successive code items from a DEX image whose header has
// been parsed.
class CodeItemParser {
 public:
  using size_type = BufferSource::size_type;

  explicit CodeItemParser(ConstBufferView image) : image_(image) {}

  // Initializes the parser, returns true on success and false on error.
  bool Init(const dex::MapItem& code_map_item) {
    // Sanity check to quickly fail if |code_map_item.offset| or
    // |code_map_item.size| is too large. This is a heuristic because code item
    // sizes need to be parsed (sizeof(dex::CodeItem) is a lower bound).
    if (!image_.covers_array(code_map_item.offset, code_map_item.size,
                             sizeof(dex::CodeItem))) {
      return false;
    }
    source_ = BufferSource(image_, code_map_item.offset);
    return true;
  }

  // Extracts the header of the next code item, and skips the variable-length
  // data. Returns the offset of the code item if successful. Otherwise returns
  // kInvalidOffset, and thereafter the parser becomes valid. For reference,
  // here's a pseudo-struct of a complete code item:
  //
  // struct code_item {
  //   // 4-byte aligned here.
  //   // 16-byte header defined (dex::CodeItem).
  //   uint16_t registers_size;
  //   uint16_t ins_size;
  //   uint16_t outs_size;
  //   uint16_t tries_size;
  //   uint32_t debug_info_off;
  //   uint32_t insns_size;
  //
  //   // Variable-length data follow.
  //   uint16_t insns[insns_size];  // Instruction bytes.
  //   uint16_t padding[(tries_size > 0 && insns_size % 2 == 1) ? 1 : 0];
  //
  //   if (tries_size > 0) {
  //     // 4-byte aligned here.
  //     struct try_item {  // dex::TryItem.
  //       uint32_t start_addr;
  //       uint16_t insn_count;
  //       uint16_t handler_off;
  //     } tries[tries_size];
  //
  //     struct encoded_catch_handler_list {
  //       uleb128 handlers_size;
  //       struct encoded_catch_handler {
  //         sleb128 encoded_catch_handler_size;
  //         struct encoded_type_addr_pair {
  //           uleb128 type_idx;
  //           uleb128 addr;
  //         } handlers[abs(encoded_catch_handler_size)];
  //         if (encoded_catch_handler_size <= 0) {
  //           uleb128 catch_all_addr;
  //         }
  //       } handlers_list[handlers_size];
  //     } handlers_group;  // Confusingly called "handlers" in DEX doc.
  //   }
  //
  //   // Padding to 4-bytes align next code_item *only if more exist*.
  // }
  offset_t GetNext() {
    // Read header CodeItem.
    if (!source_.AlignOn(image_, 4U))
      return kInvalidOffset;
    const offset_t code_item_offset =
        base::checked_cast<offset_t>(source_.begin() - image_.begin());
    const auto* code_item = source_.GetPointer<const dex::CodeItem>();
    if (!code_item)
      return kInvalidOffset;
    DCHECK(Is32BitAligned(code_item_offset));

    // TODO(huangs): Fail if |code_item->insns_size == 0| (Constraint A1).
    // Skip instruction bytes.
    if (!source_.Skip(code_item->insns_size * sizeof(uint16_t))) {
      return kInvalidOffset;
    }
    // Skip padding if present.
    if (code_item->tries_size > 0 && !source_.AlignOn(image_, 4U))
      return kInvalidOffset;

    // Skip tries[] and handlers_group to arrive at the next code item. Parsing
    // is nontrivial due to use of uleb128 / sleb128.
    if (code_item->tries_size > 0) {
      // Skip (try_item) tries[].
      if (!source_.Skip(code_item->tries_size * sizeof(dex::TryItem))) {
        return kInvalidOffset;
      }

      // Skip handlers_group.
      uint32_t handlers_size = 0;
      if (!source_.GetUleb128(&handlers_size))
        return kInvalidOffset;
      // Sanity check to quickly reject excessively large |handlers_size|.
      if (source_.Remaining() < static_cast<size_type>(handlers_size))
        return kInvalidOffset;

      // Skip (encoded_catch_handler) handlers_list[].
      for (uint32_t k = 0; k < handlers_size; ++k) {
        int32_t encoded_catch_handler_size = 0;
        if (!source_.GetSleb128(&encoded_catch_handler_size))
          return kInvalidOffset;
        const size_type abs_size = std::abs(encoded_catch_handler_size);
        if (source_.Remaining() < abs_size)  // Sanity check.
          return kInvalidOffset;
        // Skip (encoded_type_addr_pair) handlers[].
        for (size_type j = 0; j < abs_size; ++j) {
          if (!source_.SkipLeb128() || !source_.SkipLeb128())
            return kInvalidOffset;
        }
        // Skip catch_all_addr.
        if (encoded_catch_handler_size <= 0) {
          if (!source_.SkipLeb128())
            return kInvalidOffset;
        }
      }
    }
    // Success! |code_item->insns_size| is validated, but its content is still
    // considered unsafe and requires validation.
    return code_item_offset;
  }

  // Given |code_item_offset| that points to the start of a valid code item in
  // |image|, returns |insns| bytes as ConstBufferView.
  static ConstBufferView GetCodeItemInsns(ConstBufferView image,
                                          offset_t code_item_offset) {
    BufferSource source(image, code_item_offset);
    const auto* code_item = source.GetPointer<const dex::CodeItem>();
    DCHECK(code_item);
    BufferRegion insns{0, code_item->insns_size * kInstrUnitSize};
    DCHECK(source.covers(insns));
    return source[insns];
  }

 private:
  ConstBufferView image_;
  BufferSource source_;
};

/******** InstructionParser ********/

// A class that successively reads |code_item| for Dalvik instructions, which
// are found at |insns|, spanning |insns_size| uint16_t "units". These units
// store instructions followed by optional non-instruction "payload". Finding
// payload boundary requires parsing: On finding an instruction that uses (and
// points to) payload, the boundary is updated.
class InstructionParser {
 public:
  struct Value {
    offset_t instr_offset;
    raw_ptr<const dex::Instruction> instr =
        nullptr;  // null for unknown instructions.
  };

  // Returns pointer to DEX Instruction data for |opcode|, or null if |opcode|
  // is unknown. An internal initialize-on-first-use table is used for fast
  // lookup.
  const dex::Instruction* FindDalvikInstruction(uint8_t opcode) {
    static bool is_init = false;
    static const dex::Instruction* instruction_table[256];
    if (!is_init) {
      is_init = true;
      std::fill(std::begin(instruction_table), std::end(instruction_table),
                nullptr);
      for (const dex::Instruction& instr : dex::kByteCode) {
        std::fill(instruction_table + instr.opcode,
                  instruction_table + instr.opcode + instr.variant, &instr);
      }
    }
    return instruction_table[opcode];
  }

  InstructionParser() = default;

  InstructionParser(ConstBufferView image, offset_t base_offset)
      : image_begin_(image.begin()),
        insns_(CodeItemParser::GetCodeItemInsns(image, base_offset)),
        payload_boundary_(insns_.end()) {}

  // Reads the next instruction. On success, makes the data read available via
  // value() and returns true. Otherwise (done or found error) returns false.
  bool ReadNext() {
    // Do not scan past payload boundary.
    if (insns_.begin() >= payload_boundary_)
      return false;

    const offset_t instr_offset =
        base::checked_cast<offset_t>(insns_.begin() - image_begin_);
    const uint8_t op = insns_.read<uint8_t>(0);
    const dex::Instruction* instr = FindDalvikInstruction(op);

    // Stop on finding unknown instructions. ODEX files might trigger this.
    if (!instr) {
      LOG(WARNING) << "Unknown Dalvik instruction detected at "
                   << AsHex<8>(instr_offset) << ".";
      return false;
    }

    const int instr_length_units = instr->layout;
    const size_t instr_length_bytes = instr_length_units * kInstrUnitSize;
    if (insns_.size() < instr_length_bytes)
      return false;

    // Handle instructions with variable-length data payload (31t).
    if (instr->opcode == 0x26 ||  // fill-array-data
        instr->opcode == 0x2B ||  // packed-switch
        instr->opcode == 0x2C) {  // sparse-switch
      const int32_t unsafe_payload_rel_units = insns_.read<int32_t>(2);
      // Payload must be in current code item, after current instruction.
      if (unsafe_payload_rel_units < instr_length_units ||
          static_cast<uint32_t>(unsafe_payload_rel_units) >=
              insns_.size() / kInstrUnitSize) {
        LOG(WARNING) << "Invalid payload found.";
        return false;
      }
      // Update boundary between instructions and payload.
      const ConstBufferView::const_iterator payload_it =
          insns_.begin() + unsafe_payload_rel_units * kInstrUnitSize;
      payload_boundary_ = std::min(payload_boundary_, payload_it);
    }

    insns_.remove_prefix(instr_length_bytes);
    value_ = {instr_offset, instr};
    return true;
  }

  const Value& value() const { return value_; }

 private:
  ConstBufferView::const_iterator image_begin_;
  ConstBufferView insns_;
  ConstBufferView::const_iterator payload_boundary_;
  Value value_;
};

/******** InstructionReferenceReader ********/

// A class to visit |code_items|, parse instructions, and emit embedded
// References of a type determined by |filter_| and |mapper_|. Only References
// located in |[lo, hi)| are emitted. |lo| and |hi| are assumed to never
// straddle the body of a Reference.
class InstructionReferenceReader : public ReferenceReader {
 public:
  // A function that takes a parsed Dalvik instruction and decides whether it
  // contains a specific type of Reference. If true, then returns the Reference
  // location. Otherwise returns kInvalidOffset.
  using Filter =
      base::RepeatingCallback<offset_t(const InstructionParser::Value&)>;
  // A function that takes Reference location from |filter_| to extract the
  // stored target. If valid, returns it. Otherwise returns kInvalidOffset.
  using Mapper = base::RepeatingCallback<offset_t(offset_t)>;

  InstructionReferenceReader(ConstBufferView image,
                             offset_t lo,
                             offset_t hi,
                             const std::vector<offset_t>& code_item_offsets,
                             Filter&& filter,
                             Mapper&& mapper)
      : image_(image),
        lo_(lo),
        hi_(hi),
        end_it_(code_item_offsets.end()),
        filter_(std::move(filter)),
        mapper_(std::move(mapper)) {
    const auto begin_it = code_item_offsets.begin();
    // Use binary search to find the code item that contains |lo_|.
    auto comp = [](offset_t test_offset, offset_t code_item_offset) {
      return test_offset < code_item_offset;
    };
    cur_it_ = std::upper_bound(begin_it, end_it_, lo_, comp);
    if (cur_it_ != begin_it)
      --cur_it_;
    parser_ = InstructionParser(image_, *cur_it_);
  }

  // ReferenceReader:
  std::optional<Reference> GetNext() override {
    while (true) {
      while (parser_.ReadNext()) {
        const auto& v = parser_.value();
        DCHECK_NE(v.instr, nullptr);
        if (v.instr_offset >= hi_)
          return std::nullopt;
        const offset_t location = filter_.Run(v);
        if (location == kInvalidOffset || location < lo_)
          continue;
        // The general check is |location + reference_width > hi_|. However, by
        // assumption |hi_| and |lo_| do not straddle the body of a Reference.
        // So |reference_width| is unneeded.
        if (location >= hi_)
          return std::nullopt;
        offset_t target = mapper_.Run(location);
        if (target != kInvalidOffset)
          return Reference{location, target};
        else
          LOG(WARNING) << "Invalid target at " << AsHex<8>(location) << ".";
      }
      ++cur_it_;
      if (cur_it_ == end_it_)
        return std::nullopt;
      parser_ = InstructionParser(image_, *cur_it_);
    }
  }

 private:
  const ConstBufferView image_;
  const offset_t lo_;
  const offset_t hi_;
  const std::vector<offset_t>::const_iterator end_it_;
  const Filter filter_;
  const Mapper mapper_;
  std::vector<offset_t>::const_iterator cur_it_;
  InstructionParser parser_;
};

/******** ItemReferenceReader ********/

// A class to visit fixed-size item elements (determined by |item_size|) and
// emit a "member variable of interest" (MVI, determined by |rel_location| and
// |mapper|) as Reference. Only MVIs lying in |[lo, hi)| are emitted. |lo| and
// |hi| are assumed to never straddle the body of a Reference.
class ItemReferenceReader : public ReferenceReader {
 public:
  // A function that takes an MVI's location and emit its target offset.
  using Mapper = base::RepeatingCallback<offset_t(offset_t)>;

  // |item_size| is the size of a fixed-size item. |rel_location| is the
  // relative location of MVI from the start of the item containing it.
  // |rel_item_offset| is the offset to use relative to |item_offset| in cases
  // where a value other than |rel_location| is required. For an example of this
  // see ReadMethodHandleFieldOrMethodId.
  ItemReferenceReader(offset_t lo,
                      offset_t hi,
                      const dex::MapItem& map_item,
                      size_t item_size,
                      size_t rel_location,
                      Mapper&& mapper,
                      bool mapper_wants_item = false)
      : hi_(hi),
        item_base_offset_(base::checked_cast<offset_t>(map_item.offset)),
        num_items_(base::checked_cast<uint32_t>(map_item.size)),
        item_size_(base::checked_cast<uint32_t>(item_size)),
        rel_location_(base::checked_cast<uint32_t>(rel_location)),
        mapper_input_delta_(
            mapper_wants_item ? 0 : base::checked_cast<uint32_t>(rel_location)),
        mapper_(std::move(mapper)) {
    static_assert(sizeof(decltype(map_item.offset)) <= sizeof(offset_t),
                  "map_item.offset too large.");
    static_assert(sizeof(decltype(map_item.size)) <= sizeof(offset_t),
                  "map_item.size too large.");
    if (!item_base_offset_) {
      // Empty item: Assign |cur_idx| to |num_items_| to skip everything.
      cur_idx_ = num_items_;
    } else if (lo < item_base_offset_) {
      cur_idx_ = 0;
    } else if (lo < OffsetOfIndex(num_items_)) {
      cur_idx_ = (lo - item_base_offset_) / item_size_;
      // Fine-tune: Advance if |lo| lies beyond the MVI.
      if (lo > OffsetOfIndex(cur_idx_) + rel_location_)
        ++cur_idx_;
    } else {
      cur_idx_ = num_items_;
    }
  }

  // ReferenceReader:
  std::optional<Reference> GetNext() override {
    while (cur_idx_ < num_items_) {
      const offset_t item_offset = OffsetOfIndex(cur_idx_);
      const offset_t location = item_offset + rel_location_;
      // The general check is |location + reference_width > hi_|. However, by
      // assumption |hi_| and |lo_| do not straddle the body of a Reference. So
      // |reference_width| is unneeded.
      if (location >= hi_)
        break;

      // |location == item_offset + mapper_input_delta_| in the majority of
      // cases. The exception is when |mapper_| wants an item aligned location
      // instead e.g. ReadMethodHandleFieldOrMethodId.
      const offset_t target = mapper_.Run(item_offset + mapper_input_delta_);

      // kDexSentinelOffset (0) may appear for the following:
      // - ProtoIdItem: parameters_off.
      // - ClassDefItem: interfaces_off, annotations_off, class_data_off,
      //   static_values_off.
      // - AnnotationsDirectoryItem: class_annotations_off.
      // - AnnotationSetRefItem: annotations_off.
      // kDexSentinelIndexAsOffset (0xFFFFFFFF) may appear for the following:
      // - ClassDefItem: superclass_idx, source_file_idx.
      // - MethodHandleItem: |mapper_| uses ReadMethodHandleFieldOrMethodId and
      //   determines the item at |cur_idx_| is not of the required reference
      //   type.
      if (target == kDexSentinelOffset || target == kDexSentinelIndexAsOffset) {
        ++cur_idx_;
        continue;
      }

      if (target == kInvalidOffset) {
        LOG(WARNING) << "Invalid item target at " << AsHex<8>(location) << ".";
        break;
      }
      ++cur_idx_;
      return Reference{location, target};
    }
    return std::nullopt;
  }

 private:
  offset_t OffsetOfIndex(uint32_t idx) {
    return base::checked_cast<uint32_t>(item_base_offset_ + idx * item_size_);
  }

  const offset_t hi_;
  const offset_t item_base_offset_;
  const uint32_t num_items_;
  const uint32_t item_size_;
  const uint32_t rel_location_;
  const uint32_t mapper_input_delta_;
  const Mapper mapper_;
  offset_t cur_idx_ = 0;
};

// Parses a flattened jagged list of lists of items that looks like:
//   NTTT|NTT|NTTTT|N|NTT...
// where |N| is an uint32_t representing the number of items in each sub-list,
// and "T" is a fixed-size item (|item_width|) of type "T". On success, stores
// the offset of each |T| into |item_offsets|, and returns true. Otherwise
// (e.g., on finding any structural problem) returns false.
bool ParseItemOffsets(ConstBufferView image,
                      const dex::MapItem& map_item,
                      size_t item_width,
                      std::vector<offset_t>* item_offsets) {
  // Sanity check: |image| should at least fit |map_item.size| copies of "N".
  if (!image.covers_array(map_item.offset, map_item.size, sizeof(uint32_t)))
    return false;
  BufferSource source(image, map_item.offset);
  item_offsets->clear();
  for (uint32_t i = 0; i < map_item.size; ++i) {
    if (!source.AlignOn(image, 4U))
      return false;
    uint32_t unsafe_size;
    if (!source.GetValue<uint32_t>(&unsafe_size))
      return false;
    DCHECK(Is32BitAligned(
        base::checked_cast<offset_t>(source.begin() - image.begin())));
    if (!source.covers_array(0, unsafe_size, item_width))
      return false;
    for (uint32_t j = 0; j < unsafe_size; ++j) {
      item_offsets->push_back(
          base::checked_cast<offset_t>(source.begin() - image.begin()));
      if (!source.Skip(item_width)) {
        return false;
      }
    }
  }
  return true;
}

// Parses AnnotationDirectoryItems of the format (using RegEx) "(AF*M*P*)*",
// where:
//   A = AnnotationsDirectoryItem (contains class annotation),
//   F = FieldAnnotation,
//   M = MethodAnnotation,
//   P = ParameterAnnotation.
// On success, stores the offsets of each class, field, method and parameter
// annotation for each item into |*_annotation_offsets|. Otherwise on finding
// structural issues returns false.
bool ParseAnnotationsDirectoryItems(
    ConstBufferView image,
    const dex::MapItem& annotations_directory_map_item,
    std::vector<offset_t>* annotations_directory_item_offsets,
    std::vector<offset_t>* field_annotation_offsets,
    std::vector<offset_t>* method_annotation_offsets,
    std::vector<offset_t>* parameter_annotation_offsets) {
  // Sanity check: |image| should at least fit
  // |annotations_directory_map_item.size| copies of "A".
  if (!image.covers_array(annotations_directory_map_item.offset,
                          annotations_directory_map_item.size,
                          sizeof(dex::AnnotationsDirectoryItem))) {
    return false;
  }
  BufferSource source(image, annotations_directory_map_item.offset);
  annotations_directory_item_offsets->clear();
  field_annotation_offsets->clear();
  method_annotation_offsets->clear();
  parameter_annotation_offsets->clear();

  // Helper to process sublists.
  auto parse_list = [&source, image](uint32_t unsafe_size, size_t item_width,
                                     std::vector<offset_t>* item_offsets) {
    DCHECK(Is32BitAligned(
        base::checked_cast<offset_t>(source.begin() - image.begin())));
    if (!source.covers_array(0, unsafe_size, item_width))
      return false;
    item_offsets->reserve(item_offsets->size() + unsafe_size);
    for (uint32_t i = 0; i < unsafe_size; ++i) {
      item_offsets->push_back(
          base::checked_cast<offset_t>(source.begin() - image.begin()));
      if (!source.Skip(item_width)) {
        return false;
      }
    }
    return true;
  };

  annotations_directory_item_offsets->reserve(
      annotations_directory_map_item.size);
  for (uint32_t i = 0; i < annotations_directory_map_item.size; ++i) {
    if (!source.AlignOn(image, 4U))
      return false;
    // Parse header.
    annotations_directory_item_offsets->push_back(
        base::checked_cast<offset_t>(source.begin() - image.begin()));
    dex::AnnotationsDirectoryItem unsafe_annotations_directory_item;
    if (!source.GetValue(&unsafe_annotations_directory_item))
      return false;
    // Parse sublists.
    if (!(parse_list(unsafe_annotations_directory_item.fields_size,
                     sizeof(dex::FieldAnnotation), field_annotation_offsets) &&
          parse_list(unsafe_annotations_directory_item.annotated_methods_size,
                     sizeof(dex::MethodAnnotation),
                     method_annotation_offsets) &&
          parse_list(
              unsafe_annotations_directory_item.annotated_parameters_size,
              sizeof(dex::ParameterAnnotation),
              parameter_annotation_offsets))) {
      return false;
    }
  }
  return true;
}

/******** CachedItemListReferenceReader ********/

// A class that takes sorted |item_offsets|, and emits all member variable of
// interest (MVIs) that fall inside |[lo, hi)|. The MVI of each item has
// location of |rel_location| from item offset, and has target extracted with
// |mapper| (which performs validation). By the "atomicity assumption",
// [|lo, hi)| never cut across an MVI.
class CachedItemListReferenceReader : public ReferenceReader {
 public:
  // A function that takes an MVI's location and emit its target offset.
  using Mapper = base::RepeatingCallback<offset_t(offset_t)>;

  CachedItemListReferenceReader(offset_t lo,
                                offset_t hi,
                                uint32_t rel_location,
                                const std::vector<offset_t>& item_offsets,
                                Mapper&& mapper)
      : hi_(hi),
        rel_location_(rel_location),
        end_it_(item_offsets.cend()),
        mapper_(mapper) {
    cur_it_ = std::upper_bound(item_offsets.cbegin(), item_offsets.cend(), lo);
    // Adding |rel_location_| is necessary as references can be offset from the
    // start of the item.
    if (cur_it_ != item_offsets.begin() && *(cur_it_ - 1) + rel_location_ >= lo)
      --cur_it_;
  }
  CachedItemListReferenceReader(const CachedItemListReferenceReader&) = delete;
  const CachedItemListReferenceReader& operator=(
      const CachedItemListReferenceReader&) = delete;

  // ReferenceReader:
  std::optional<Reference> GetNext() override {
    while (cur_it_ < end_it_) {
      const offset_t location = *cur_it_ + rel_location_;
      if (location >= hi_)  // Check is simplified by atomicity assumption.
        break;
      const offset_t target = mapper_.Run(location);
      if (target == kInvalidOffset) {
        LOG(WARNING) << "Invalid item target at " << AsHex<8>(location) << ".";
        break;
      }
      ++cur_it_;

      // kDexSentinelOffset is a sentinel for;
      // - AnnotationsDirectoryItem: class_annotations_off
      if (target == kDexSentinelOffset)
        continue;
      return Reference{location, target};
    }
    return std::nullopt;
  }

 private:
  const offset_t hi_;
  const uint32_t rel_location_;
  const std::vector<offset_t>::const_iterator end_it_;
  const Mapper mapper_;
  std::vector<offset_t>::const_iterator cur_it_;
};

// Reads an INT index at |location| in |image| and translates the index to the
// offset of a fixed-size item specified by |target_map_item| and
// |target_item_size|. Returns the target offset if valid, or kInvalidOffset
// otherwise. This is compatible with
// CachedReferenceListReferenceReader::Mapper,
// InstructionReferenceReader::Mapper, and ItemReferenceReader::Mapper.
template <typename INT>
static offset_t ReadTargetIndex(ConstBufferView image,
                                const dex::MapItem& target_map_item,
                                size_t target_item_size,
                                offset_t location) {
  static_assert(sizeof(INT) <= sizeof(offset_t),
                "INT may not fit into offset_t.");
  const offset_t unsafe_idx = image.read<INT>(location);
  // kDexSentinalIndexAsOffset (0xFFFFFFFF) is a sentinel for
  // - ClassDefItem: superclass_idx, source_file_idx.
  if (unsafe_idx == kDexSentinelIndexAsOffset)
    return unsafe_idx;
  if (unsafe_idx >= target_map_item.size)
    return kInvalidOffset;
  return target_map_item.offset +
         base::checked_cast<offset_t>(unsafe_idx * target_item_size);
}

// Reads a field or method index of the MethodHandleItem located at |location|
// in |image| and translates |method_handle_item.field_or_method_id| to the
// offset of a fixed-size item specified by |target_map_item| and
// |target_item_size|. The index is deemed to be of the correct target type if
// |method_handle_item.method_handle_type| falls within the range [|min_type|,
// |max_type|]. If the target type is correct ReadTargetIndex is called.
// Returns the target offset if valid, or kDexSentinelIndexAsOffset if
// |method_handle_item.method_handle_type| is of the wrong type, or
// kInvalidOffset otherwise.
//
// As of DEX version 39 MethodHandleType values for FieldId and MethodId each
// form one consecutive block of values. If this changes, then the interface to
// this function will need to be redesigned.
static offset_t ReadMethodHandleFieldOrMethodId(
    ConstBufferView image,
    const dex::MapItem& target_map_item,
    size_t target_item_size,
    dex::MethodHandleType min_type,
    dex::MethodHandleType max_type,
    offset_t location) {
  dex::MethodHandleItem method_handle_item =
      image.read<dex::MethodHandleItem>(location);

  // Cannot use base::checked_cast as dex::MethodHandleType is an enum class so
  // static_assert on the size instead.
  static_assert(sizeof(decltype(dex::MethodHandleItem::method_handle_type)) <=
                    sizeof(dex::MethodHandleType),
                "dex::MethodHandleItem::method_handle_type may not fit into "
                "dex::MethodHandleType.");
  dex::MethodHandleType method_handle_type =
      static_cast<dex::MethodHandleType>(method_handle_item.method_handle_type);

  if (method_handle_type >= dex::MethodHandleType::kMaxMethodHandleType) {
    return kInvalidOffset;
  }

  // Use DexSentinelIndexAsOffset to skip the item as it isn't of the
  // corresponding method handle type.
  if (method_handle_type < min_type || method_handle_type > max_type) {
    return kDexSentinelIndexAsOffset;
  }

  return ReadTargetIndex<decltype(dex::MethodHandleItem::field_or_method_id)>(
      image, target_map_item, target_item_size,
      location + offsetof(dex::MethodHandleItem, field_or_method_id));
}

// Reads uint32_t value in |image| at (valid) |location| and checks whether it
// is a safe offset of a fixed-size item. Returns the target offset (possibly a
// sentinel) if valid, or kInvalidOffset otherwise. This is compatible with
// CachedReferenceListReferenceReader::Mapper,
// InstructionReferenceReader::Mapper, and ItemReferenceReader::Mapper.
static offset_t ReadTargetOffset32(ConstBufferView image, offset_t location) {
  const offset_t unsafe_target =
      static_cast<offset_t>(image.read<uint32_t>(location));
  // Skip and don't validate kDexSentinelOffset as it is indicative of an
  // empty reference.
  if (unsafe_target == kDexSentinelOffset)
    return unsafe_target;

  // TODO(huangs): Check that |unsafe_target| is within the correct data
  // section.
  if (unsafe_target >= image.size())
    return kInvalidOffset;
  return unsafe_target;
}

/******** ReferenceWriterAdaptor ********/

// A ReferenceWriter that adapts a callback that performs type-specific
// Reference writes.
class ReferenceWriterAdaptor : public ReferenceWriter {
 public:
  using Writer = base::RepeatingCallback<void(Reference, MutableBufferView)>;

  ReferenceWriterAdaptor(MutableBufferView image, Writer&& writer)
      : image_(image), writer_(std::move(writer)) {}

  // ReferenceWriter:
  void PutNext(Reference ref) override { writer_.Run(ref, image_); }

 private:
  MutableBufferView image_;
  Writer writer_;
};

// Helper that's compatible with ReferenceWriterAdaptor::Writer.
// Given that |ref.target| points to the start of a fixed size DEX item (e.g.,
// FieldIdItem), translates |ref.target| to item index, and writes the result to
// |ref.location| as |INT|.
template <typename INT>
static void WriteTargetIndex(const dex::MapItem& target_map_item,
                             size_t target_item_size,
                             Reference ref,
                             MutableBufferView image) {
  const size_t unsafe_idx =
      (ref.target - target_map_item.offset) / target_item_size;
  // Verify that index is within bound.
  if (unsafe_idx >= target_map_item.size) {
    LOG(ERROR) << "Target index out of bounds at: " << AsHex<8>(ref.location)
               << ".";
    return;
  }
  // Verify that |ref.target| points to start of item.
  DCHECK_EQ(ref.target, target_map_item.offset + unsafe_idx * target_item_size);
  image.write<INT>(ref.location, base::checked_cast<INT>(unsafe_idx));
}

// Buffer for ReadDexHeader() to optionally return results.
struct ReadDexHeaderResults {
  BufferSource source;
  raw_ptr<const dex::HeaderItem> header;
  int dex_version;
};

// Returns whether |image| points to a DEX file. If this is a possibility and
// |opt_results| is not null, then uses it to pass extracted data to enable
// further parsing.
bool ReadDexHeader(ConstBufferView image, ReadDexHeaderResults* opt_results) {
  // This part needs to be fairly efficient since it may be called many times.
  BufferSource source(image);
  const dex::HeaderItem* header = source.GetPointer<dex::HeaderItem>();
  if (!header)
    return false;
  if (header->magic[0] != 'd' || header->magic[1] != 'e' ||
      header->magic[2] != 'x' || header->magic[3] != '\n' ||
      header->magic[7] != '\0') {
    return false;
  }

  // Magic matches: More detailed tests can be conducted.
  int dex_version = 0;
  for (int i = 4; i < 7; ++i) {
    if (!absl::ascii_isdigit(header->magic[i])) {
      return false;
    }
    dex_version = dex_version * 10 + (header->magic[i] - '0');
  }

  // Only support DEX versions 35, 37, 38, and 39
  if (dex_version != 35 && dex_version != 37 && dex_version != 38 &&
      dex_version != 39) {
    return false;
  }

  if (header->file_size > image.size() ||
      header->file_size < sizeof(dex::HeaderItem) ||
      header->map_off < sizeof(dex::HeaderItem)) {
    return false;
  }

  if (opt_results)
    *opt_results = {source, header, dex_version};
  return true;
}

}  // namespace

/******** DisassemblerDex ********/

DisassemblerDex::DisassemblerDex() : Disassembler(4) {}

DisassemblerDex::~DisassemblerDex() = default;

// static.
bool DisassemblerDex::QuickDetect(ConstBufferView image) {
  return ReadDexHeader(image, nullptr);
}

ExecutableType DisassemblerDex::GetExeType() const {
  return kExeTypeDex;
}

std::string DisassemblerDex::GetExeTypeString() const {
  return base::StringPrintf("DEX (version %d)", dex_version_);
}

std::vector<ReferenceGroup> DisassemblerDex::MakeReferenceGroups() const {
  // Must follow DisassemblerDex::ReferenceType order. Initialized on first use.
  return {
      {{4, TypeTag(kTypeIdToDescriptorStringId), PoolTag(kStringId)},
       &DisassemblerDex::MakeReadTypeIdToDescriptorStringId32,
       &DisassemblerDex::MakeWriteStringId32},
      {{4, TypeTag(kProtoIdToShortyStringId), PoolTag(kStringId)},
       &DisassemblerDex::MakeReadProtoIdToShortyStringId32,
       &DisassemblerDex::MakeWriteStringId32},
      {{4, TypeTag(kFieldIdToNameStringId), PoolTag(kStringId)},
       &DisassemblerDex::MakeReadFieldToNameStringId32,
       &DisassemblerDex::MakeWriteStringId32},
      {{4, TypeTag(kMethodIdToNameStringId), PoolTag(kStringId)},
       &DisassemblerDex::MakeReadMethodIdToNameStringId32,
       &DisassemblerDex::MakeWriteStringId32},
      {{4, TypeTag(kClassDefToSourceFileStringId), PoolTag(kStringId)},
       &DisassemblerDex::MakeReadClassDefToSourceFileStringId32,
       &DisassemblerDex::MakeWriteStringId32},
      {{2, TypeTag(kCodeToStringId16), PoolTag(kStringId)},
       &DisassemblerDex::MakeReadCodeToStringId16,
       &DisassemblerDex::MakeWriteStringId16},
      {{4, TypeTag(kCodeToStringId32), PoolTag(kStringId)},
       &DisassemblerDex::MakeReadCodeToStringId32,
       &DisassemblerDex::MakeWriteStringId32},
      {{4, TypeTag(kProtoIdToReturnTypeId), PoolTag(kTypeId)},
       &DisassemblerDex::MakeReadProtoIdToReturnTypeId32,
       &DisassemblerDex::MakeWriteTypeId32},
      {{2, TypeTag(kFieldIdToClassTypeId), PoolTag(kTypeId)},
       &DisassemblerDex::MakeReadFieldToClassTypeId16,
       &DisassemblerDex::MakeWriteTypeId16},
      {{2, TypeTag(kFieldIdToTypeId), PoolTag(kTypeId)},
       &DisassemblerDex::MakeReadFieldToTypeId16,
       &DisassemblerDex::MakeWriteTypeId16},
      {{2, TypeTag(kMethodIdToClassTypeId), PoolTag(kTypeId)},
       &DisassemblerDex::MakeReadMethodIdToClassTypeId16,
       &DisassemblerDex::MakeWriteTypeId16},
      {{4, TypeTag(kClassDefToClassTypeId), PoolTag(kTypeId)},
       &DisassemblerDex::MakeReadClassDefToClassTypeId32,
       &DisassemblerDex::MakeWriteTypeId32},
      {{4, TypeTag(kClassDefToSuperClassTypeId), PoolTag(kTypeId)},
       &DisassemblerDex::MakeReadClassDefToSuperClassTypeId32,
       &DisassemblerDex::MakeWriteTypeId32},
      {{2, TypeTag(kTypeListToTypeId), PoolTag(kTypeId)},
       &DisassemblerDex::MakeReadTypeListToTypeId16,
       &DisassemblerDex::MakeWriteTypeId16},
      {{2, TypeTag(kCodeToTypeId), PoolTag(kTypeId)},
       &DisassemblerDex::MakeReadCodeToTypeId16,
       &DisassemblerDex::MakeWriteTypeId16},
      {{2, TypeTag(kCodeToProtoId), PoolTag(kProtoId)},
       &DisassemblerDex::MakeReadCodeToProtoId16,
       &DisassemblerDex::MakeWriteProtoId16},
      {{2, TypeTag(kMethodIdToProtoId), PoolTag(kProtoId)},
       &DisassemblerDex::MakeReadMethodIdToProtoId16,
       &DisassemblerDex::MakeWriteProtoId16},
      {{2, TypeTag(kCodeToFieldId), PoolTag(kFieldId)},
       &DisassemblerDex::MakeReadCodeToFieldId16,
       &DisassemblerDex::MakeWriteFieldId16},
      {{2, TypeTag(kMethodHandleToFieldId), PoolTag(kFieldId)},
       &DisassemblerDex::MakeReadMethodHandleToFieldId16,
       &DisassemblerDex::MakeWriteFieldId16},
      {{4, TypeTag(kAnnotationsDirectoryToFieldId), PoolTag(kFieldId)},
       &DisassemblerDex::MakeReadAnnotationsDirectoryToFieldId32,
       &DisassemblerDex::MakeWriteFieldId32},
      {{2, TypeTag(kCodeToMethodId), PoolTag(kMethodId)},
       &DisassemblerDex::MakeReadCodeToMethodId16,
       &DisassemblerDex::MakeWriteMethodId16},
      {{2, TypeTag(kMethodHandleToMethodId), PoolTag(kMethodId)},
       &DisassemblerDex::MakeReadMethodHandleToMethodId16,
       &DisassemblerDex::MakeWriteMethodId16},
      {{4, TypeTag(kAnnotationsDirectoryToMethodId), PoolTag(kMethodId)},
       &DisassemblerDex::MakeReadAnnotationsDirectoryToMethodId32,
       &DisassemblerDex::MakeWriteMethodId32},
      {{4, TypeTag(kAnnotationsDirectoryToParameterMethodId),
        PoolTag(kMethodId)},
       &DisassemblerDex::MakeReadAnnotationsDirectoryToParameterMethodId32,
       &DisassemblerDex::MakeWriteMethodId32},
      {{2, TypeTag(kCodeToCallSiteId), PoolTag(kCallSiteId)},
       &DisassemblerDex::MakeReadCodeToCallSiteId16,
       &DisassemblerDex::MakeWriteCallSiteId16},
      {{2, TypeTag(kCodeToMethodHandle), PoolTag(kMethodHandle)},
       &DisassemblerDex::MakeReadCodeToMethodHandle16,
       &DisassemblerDex::MakeWriteMethodHandle16},
      {{4, TypeTag(kProtoIdToParametersTypeList), PoolTag(kTypeList)},
       &DisassemblerDex::MakeReadProtoIdToParametersTypeList,
       &DisassemblerDex::MakeWriteAbs32},
      {{4, TypeTag(kClassDefToInterfacesTypeList), PoolTag(kTypeList)},
       &DisassemblerDex::MakeReadClassDefToInterfacesTypeList,
       &DisassemblerDex::MakeWriteAbs32},
      {{4, TypeTag(kAnnotationsDirectoryToParameterAnnotationSetRef),
        PoolTag(kAnnotationSetRefList)},
       &DisassemblerDex::
           MakeReadAnnotationsDirectoryToParameterAnnotationSetRef,
       &DisassemblerDex::MakeWriteAbs32},
      {{4, TypeTag(kAnnotationSetRefListToAnnotationSet),
        PoolTag(kAnnotionSet)},
       &DisassemblerDex::MakeReadAnnotationSetRefListToAnnotationSet,
       &DisassemblerDex::MakeWriteAbs32},
      {{4, TypeTag(kAnnotationsDirectoryToClassAnnotationSet),
        PoolTag(kAnnotionSet)},
       &DisassemblerDex::MakeReadAnnotationsDirectoryToClassAnnotationSet,
       &DisassemblerDex::MakeWriteAbs32},
      {{4, TypeTag(kAnnotationsDirectoryToFieldAnnotationSet),
        PoolTag(kAnnotionSet)},
       &DisassemblerDex::MakeReadAnnotationsDirectoryToFieldAnnotationSet,
       &DisassemblerDex::MakeWriteAbs32},
      {{4, TypeTag(kAnnotationsDirectoryToMethodAnnotationSet),
        PoolTag(kAnnotionSet)},
       &DisassemblerDex::MakeReadAnnotationsDirectoryToMethodAnnotationSet,
       &DisassemblerDex::MakeWriteAbs32},
      {{4, TypeTag(kClassDefToClassData), PoolTag(kClassData)},
       &DisassemblerDex::MakeReadClassDefToClassData,
       &DisassemblerDex::MakeWriteAbs32},
      {{1, TypeTag(kCodeToRelCode8), PoolTag(kCode)},
       &DisassemblerDex::MakeReadCodeToRelCode8,
       &DisassemblerDex::MakeWriteRelCode8},
      {{2, TypeTag(kCodeToRelCode16), PoolTag(kCode)},
       &DisassemblerDex::MakeReadCodeToRelCode16,
       &DisassemblerDex::MakeWriteRelCode16},
      {{4, TypeTag(kCodeToRelCode32), PoolTag(kCode)},
       &DisassemblerDex::MakeReadCodeToRelCode32,
       &DisassemblerDex::MakeWriteRelCode32},
      {{4, TypeTag(kStringIdToStringData), PoolTag(kStringData)},
       &DisassemblerDex::MakeReadStringIdToStringData,
       &DisassemblerDex::MakeWriteAbs32},
      {{4, TypeTag(kAnnotationSetToAnnotation), PoolTag(kAnnotation)},
       &DisassemblerDex::MakeReadAnnotationSetToAnnotation,
       &DisassemblerDex::MakeWriteAbs32},
      {{4, TypeTag(kClassDefToStaticValuesEncodedArray),
        PoolTag(kEncodedArray)},
       &DisassemblerDex::MakeReadClassDefToStaticValuesEncodedArray,
       &DisassemblerDex::MakeWriteAbs32},
      {{4, TypeTag(kClassDefToAnnotationDirectory),
        PoolTag(kAnnotationsDirectory)},
       &DisassemblerDex::MakeReadClassDefToAnnotationDirectory,
       &DisassemblerDex::MakeWriteAbs32},
      {{4, TypeTag(kCallSiteIdToCallSite), PoolTag(kCallSite)},
       &DisassemblerDex::MakeReadCallSiteIdToCallSite32,
       &DisassemblerDex::MakeWriteAbs32},
  };
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadStringIdToStringData(
    offset_t lo,
    offset_t hi) {
  // dex::StringIdItem::string_data_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<ItemReferenceReader>(
      lo, hi, string_map_item_, sizeof(dex::StringIdItem),
      offsetof(dex::StringIdItem, string_data_off), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadTypeIdToDescriptorStringId32(offset_t lo,
                                                      offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::TypeIdItem::descriptor_idx)>, image_,
      string_map_item_, sizeof(dex::StringIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, type_map_item_, sizeof(dex::TypeIdItem),
      offsetof(dex::TypeIdItem, descriptor_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadProtoIdToShortyStringId32(offset_t lo, offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::ProtoIdItem::shorty_idx)>, image_,
      string_map_item_, sizeof(dex::StringIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, proto_map_item_, sizeof(dex::ProtoIdItem),
      offsetof(dex::ProtoIdItem, shorty_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadProtoIdToReturnTypeId32(offset_t lo, offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::ProtoIdItem::return_type_idx)>, image_,
      type_map_item_, sizeof(dex::TypeIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, proto_map_item_, sizeof(dex::ProtoIdItem),
      offsetof(dex::ProtoIdItem, return_type_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadProtoIdToParametersTypeList(offset_t lo, offset_t hi) {
  // dex::ProtoIdItem::parameters_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<ItemReferenceReader>(
      lo, hi, proto_map_item_, sizeof(dex::ProtoIdItem),
      offsetof(dex::ProtoIdItem, parameters_off), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadFieldToClassTypeId16(
    offset_t lo,
    offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::FieldIdItem::class_idx)>, image_,
      type_map_item_, sizeof(dex::TypeIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, field_map_item_, sizeof(dex::FieldIdItem),
      offsetof(dex::FieldIdItem, class_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadFieldToTypeId16(
    offset_t lo,
    offset_t hi) {
  auto mapper =
      base::BindRepeating(ReadTargetIndex<decltype(dex::FieldIdItem::type_idx)>,
                          image_, type_map_item_, sizeof(dex::TypeIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, field_map_item_, sizeof(dex::FieldIdItem),
      offsetof(dex::FieldIdItem, type_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadFieldToNameStringId32(
    offset_t lo,
    offset_t hi) {
  auto mapper =
      base::BindRepeating(ReadTargetIndex<decltype(dex::FieldIdItem::name_idx)>,
                          image_, string_map_item_, sizeof(dex::StringIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, field_map_item_, sizeof(dex::FieldIdItem),
      offsetof(dex::FieldIdItem, name_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadMethodIdToClassTypeId16(offset_t lo, offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::MethodIdItem::class_idx)>, image_,
      type_map_item_, sizeof(dex::TypeIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, method_map_item_, sizeof(dex::MethodIdItem),
      offsetof(dex::MethodIdItem, class_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadMethodIdToProtoId16(
    offset_t lo,
    offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::MethodIdItem::proto_idx)>, image_,
      proto_map_item_, sizeof(dex::ProtoIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, method_map_item_, sizeof(dex::MethodIdItem),
      offsetof(dex::MethodIdItem, proto_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadMethodIdToNameStringId32(offset_t lo, offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::MethodIdItem::name_idx)>, image_,
      string_map_item_, sizeof(dex::StringIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, method_map_item_, sizeof(dex::MethodIdItem),
      offsetof(dex::MethodIdItem, name_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadClassDefToClassTypeId32(offset_t lo, offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::ClassDefItem::superclass_idx)>, image_,
      type_map_item_, sizeof(dex::TypeIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, class_def_map_item_, sizeof(dex::ClassDefItem),
      offsetof(dex::ClassDefItem, class_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadClassDefToSuperClassTypeId32(offset_t lo,
                                                      offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::ClassDefItem::superclass_idx)>, image_,
      type_map_item_, sizeof(dex::TypeIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, class_def_map_item_, sizeof(dex::ClassDefItem),
      offsetof(dex::ClassDefItem, superclass_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadClassDefToInterfacesTypeList(offset_t lo,
                                                      offset_t hi) {
  // dex::ClassDefItem::interfaces_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<ItemReferenceReader>(
      lo, hi, class_def_map_item_, sizeof(dex::ClassDefItem),
      offsetof(dex::ClassDefItem, interfaces_off), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadClassDefToSourceFileStringId32(offset_t lo,
                                                        offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::ClassDefItem::source_file_idx)>, image_,
      string_map_item_, sizeof(dex::StringIdItem));
  return std::make_unique<ItemReferenceReader>(
      lo, hi, class_def_map_item_, sizeof(dex::ClassDefItem),
      offsetof(dex::ClassDefItem, source_file_idx), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadClassDefToAnnotationDirectory(offset_t lo,
                                                       offset_t hi) {
  // dex::ClassDefItem::annotations_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<ItemReferenceReader>(
      lo, hi, class_def_map_item_, sizeof(dex::ClassDefItem),
      offsetof(dex::ClassDefItem, annotations_off), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadClassDefToClassData(
    offset_t lo,
    offset_t hi) {
  // dex::ClassDefItem::class_data_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<ItemReferenceReader>(
      lo, hi, class_def_map_item_, sizeof(dex::ClassDefItem),
      offsetof(dex::ClassDefItem, class_data_off), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadClassDefToStaticValuesEncodedArray(offset_t lo,
                                                            offset_t hi) {
  // dex::ClassDefItem::static_values_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<ItemReferenceReader>(
      lo, hi, class_def_map_item_, sizeof(dex::ClassDefItem),
      offsetof(dex::ClassDefItem, static_values_off), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadCallSiteIdToCallSite32(offset_t lo, offset_t hi) {
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<ItemReferenceReader>(
      lo, hi, call_site_map_item_, sizeof(dex::CallSiteIdItem),
      offsetof(dex::CallSiteIdItem, call_site_off), std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadMethodHandleToFieldId16(offset_t lo, offset_t hi) {
  auto mapper = base::BindRepeating(ReadMethodHandleFieldOrMethodId, image_,
                                    field_map_item_, sizeof(dex::FieldIdItem),
                                    dex::MethodHandleType::kStaticPut,
                                    dex::MethodHandleType::kInstanceGet);
  // Use |mapper_wants_item == true| for ItemReferenceReader such that
  // |location| is aligned with MethodHandleItem when passed to |mapper|. This
  // allows ReadMethodHandleFieldOrMethodId to safely determine whether the
  // reference in the MethodHandleItem is of the correct type to be emitted.
  return std::make_unique<ItemReferenceReader>(
      lo, hi, method_handle_map_item_, sizeof(dex::MethodHandleItem),
      offsetof(dex::MethodHandleItem, field_or_method_id), std::move(mapper),
      /*mapper_wants_item=*/true);
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadMethodHandleToMethodId16(offset_t lo, offset_t hi) {
  auto mapper = base::BindRepeating(ReadMethodHandleFieldOrMethodId, image_,
                                    method_map_item_, sizeof(dex::MethodIdItem),
                                    dex::MethodHandleType::kInvokeStatic,
                                    dex::MethodHandleType::kInvokeInterface);
  // Use |mapper_wants_item == true| for ItemReferenceReader such that
  // |location| is aligned with MethodHandleItem when passed to |mapper|. This
  // allows ReadMethodHandleFieldOrMethodId to safely determine whether the
  // reference in the MethodHandleItem is of the correct type to be emitted.
  return std::make_unique<ItemReferenceReader>(
      lo, hi, method_handle_map_item_, sizeof(dex::MethodHandleItem),
      offsetof(dex::MethodHandleItem, field_or_method_id), std::move(mapper),
      /*mapper_wants_item=*/true);
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadTypeListToTypeId16(
    offset_t lo,
    offset_t hi) {
  auto mapper =
      base::BindRepeating(ReadTargetIndex<decltype(dex::TypeItem::type_idx)>,
                          image_, type_map_item_, sizeof(dex::TypeIdItem));
  return std::make_unique<CachedItemListReferenceReader>(
      lo, hi, offsetof(dex::TypeItem, type_idx), type_list_offsets_,
      std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadAnnotationSetToAnnotation(offset_t lo, offset_t hi) {
  // dex::AnnotationOffItem::annotation_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<CachedItemListReferenceReader>(
      lo, hi, offsetof(dex::AnnotationOffItem, annotation_off),
      annotation_set_offsets_, std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadAnnotationSetRefListToAnnotationSet(offset_t lo,
                                                             offset_t hi) {
  // dex::AnnotationSetRefItem::annotations_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<CachedItemListReferenceReader>(
      lo, hi, offsetof(dex::AnnotationSetRefItem, annotations_off),
      annotation_set_ref_list_offsets_, std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadAnnotationsDirectoryToClassAnnotationSet(offset_t lo,
                                                                  offset_t hi) {
  // dex::AnnotationsDirectoryItem::class_annotations_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<CachedItemListReferenceReader>(
      lo, hi, offsetof(dex::AnnotationsDirectoryItem, class_annotations_off),
      annotations_directory_item_offsets_, std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadAnnotationsDirectoryToFieldId32(offset_t lo,
                                                         offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::FieldAnnotation::field_idx)>, image_,
      field_map_item_, sizeof(dex::FieldIdItem));
  return std::make_unique<CachedItemListReferenceReader>(
      lo, hi, offsetof(dex::FieldAnnotation, field_idx),
      annotations_directory_item_field_annotation_offsets_, std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadAnnotationsDirectoryToFieldAnnotationSet(offset_t lo,
                                                                  offset_t hi) {
  // dex::FieldAnnotation::annotations_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<CachedItemListReferenceReader>(
      lo, hi, offsetof(dex::FieldAnnotation, annotations_off),
      annotations_directory_item_field_annotation_offsets_, std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadAnnotationsDirectoryToMethodId32(offset_t lo,
                                                          offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::MethodAnnotation::method_idx)>, image_,
      method_map_item_, sizeof(dex::MethodIdItem));
  return std::make_unique<CachedItemListReferenceReader>(
      lo, hi, offsetof(dex::MethodAnnotation, method_idx),
      annotations_directory_item_method_annotation_offsets_, std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadAnnotationsDirectoryToMethodAnnotationSet(
    offset_t lo,
    offset_t hi) {
  // dex::MethodAnnotation::annotations_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<CachedItemListReferenceReader>(
      lo, hi, offsetof(dex::MethodAnnotation, annotations_off),
      annotations_directory_item_method_annotation_offsets_, std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadAnnotationsDirectoryToParameterMethodId32(
    offset_t lo,
    offset_t hi) {
  auto mapper = base::BindRepeating(
      ReadTargetIndex<decltype(dex::ParameterAnnotation::method_idx)>, image_,
      method_map_item_, sizeof(dex::MethodIdItem));
  return std::make_unique<CachedItemListReferenceReader>(
      lo, hi, offsetof(dex::ParameterAnnotation, method_idx),
      annotations_directory_item_parameter_annotation_offsets_,
      std::move(mapper));
}

std::unique_ptr<ReferenceReader>
DisassemblerDex::MakeReadAnnotationsDirectoryToParameterAnnotationSetRef(
    offset_t lo,
    offset_t hi) {
  // dex::ParameterAnnotation::annotations_off mapper.
  auto mapper = base::BindRepeating(ReadTargetOffset32, image_);
  return std::make_unique<CachedItemListReferenceReader>(
      lo, hi, offsetof(dex::ParameterAnnotation, annotations_off),
      annotations_directory_item_parameter_annotation_offsets_,
      std::move(mapper));
}

// MakeReadCode* readers use offset relative to the instruction beginning based
// on the instruction format ID.
// See https://source.android.com/devices/tech/dalvik/instruction-formats

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadCodeToStringId16(
    offset_t lo,
    offset_t hi) {
  auto filter = base::BindRepeating(
      [](const InstructionParser::Value& value) -> offset_t {
        if (value.instr->format == dex::FormatId::c &&
            (value.instr->opcode == 0x1A)) {  // const-string
          // BBBB from e.g., const-string vAA, string@BBBB.
          return value.instr_offset + 2;
        }
        return kInvalidOffset;
      });
  auto mapper =
      base::BindRepeating(ReadTargetIndex<uint16_t>, image_, string_map_item_,
                          sizeof(dex::StringIdItem));
  return std::make_unique<InstructionReferenceReader>(
      image_, lo, hi, code_item_offsets_, std::move(filter), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadCodeToStringId32(
    offset_t lo,
    offset_t hi) {
  auto filter = base::BindRepeating(
      [](const InstructionParser::Value& value) -> offset_t {
        if (value.instr->format == dex::FormatId::c &&
            (value.instr->opcode == 0x1B)) {  // const-string/jumbo
          // BBBBBBBB from e.g., const-string/jumbo vAA, string@BBBBBBBB.
          return value.instr_offset + 2;
        }
        return kInvalidOffset;
      });
  auto mapper =
      base::BindRepeating(ReadTargetIndex<uint32_t>, image_, string_map_item_,
                          sizeof(dex::StringIdItem));
  return std::make_unique<InstructionReferenceReader>(
      image_, lo, hi, code_item_offsets_, std::move(filter), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadCodeToTypeId16(
    offset_t lo,
    offset_t hi) {
  auto filter = base::BindRepeating(
      [](const InstructionParser::Value& value) -> offset_t {
        if (value.instr->format == dex::FormatId::c &&
            (value.instr->opcode == 0x1C ||   // const-class
             value.instr->opcode == 0x1F ||   // check-cast
             value.instr->opcode == 0x20 ||   // instance-of
             value.instr->opcode == 0x22 ||   // new-instance
             value.instr->opcode == 0x23 ||   // new-array
             value.instr->opcode == 0x24 ||   // filled-new-array
             value.instr->opcode == 0x25)) {  // filled-new-array/range
          // BBBB from e.g., const-class vAA, type@BBBB.
          return value.instr_offset + 2;
        }
        return kInvalidOffset;
      });
  auto mapper = base::BindRepeating(ReadTargetIndex<uint16_t>, image_,
                                    type_map_item_, sizeof(dex::TypeIdItem));
  return std::make_unique<InstructionReferenceReader>(
      image_, lo, hi, code_item_offsets_, std::move(filter), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadCodeToProtoId16(
    offset_t lo,
    offset_t hi) {
  auto filter = base::BindRepeating(
      [](const InstructionParser::Value& value) -> offset_t {
        if (value.instr->format == dex::FormatId::c) {
          if (value.instr->opcode == 0xFA ||  // invoke-polymorphic
              value.instr->opcode == 0xFB) {  // invoke-polymorphic/range
            // HHHH from e.g, invoke-polymorphic {vC, vD, vE, vF, vG},
            //   meth@BBBB, proto@HHHH
            return value.instr_offset + 6;
          }
          if (value.instr->opcode == 0xFF) {  // const-method-type
            // BBBB from e.g., const-method-type vAA, proto@BBBB
            return value.instr_offset + 2;
          }
        }
        return kInvalidOffset;
      });
  auto mapper = base::BindRepeating(ReadTargetIndex<uint16_t>, image_,
                                    proto_map_item_, sizeof(dex::ProtoIdItem));
  return std::make_unique<InstructionReferenceReader>(
      image_, lo, hi, code_item_offsets_, std::move(filter), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadCodeToCallSiteId16(
    offset_t lo,
    offset_t hi) {
  auto filter = base::BindRepeating(
      [](const InstructionParser::Value& value) -> offset_t {
        if (value.instr->format == dex::FormatId::c &&
            (value.instr->opcode == 0xFC ||   // invoke-custom
             value.instr->opcode == 0xFD)) {  // invoke-custom/range
          // BBBB from e.g, invoke-custom {vC, vD, vE, vF, vG},
          //   call_site@BBBB
          return value.instr_offset + 2;
        }
        return kInvalidOffset;
      });
  auto mapper =
      base::BindRepeating(ReadTargetIndex<uint16_t>, image_,
                          call_site_map_item_, sizeof(dex::CallSiteIdItem));
  return std::make_unique<InstructionReferenceReader>(
      image_, lo, hi, code_item_offsets_, std::move(filter), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadCodeToMethodHandle16(
    offset_t lo,
    offset_t hi) {
  auto filter = base::BindRepeating(
      [](const InstructionParser::Value& value) -> offset_t {
        if (value.instr->format == dex::FormatId::c &&
            value.instr->opcode == 0xFE) {  // const-method-handle
          // BBBB from e.g, const-method-handle vAA, method_handle@BBBB
          return value.instr_offset + 2;
        }
        return kInvalidOffset;
      });
  auto mapper = base::BindRepeating(ReadTargetIndex<uint16_t>, image_,
                                    method_handle_map_item_,
                                    sizeof(dex::MethodHandleItem));
  return std::make_unique<InstructionReferenceReader>(
      image_, lo, hi, code_item_offsets_, std::move(filter), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadCodeToFieldId16(
    offset_t lo,
    offset_t hi) {
  auto filter = base::BindRepeating(
      [](const InstructionParser::Value& value) -> offset_t {
        if (value.instr->format == dex::FormatId::c &&
            (value.instr->opcode == 0x52 ||   // iinstanceop (iget-*, iput-*)
             value.instr->opcode == 0x60)) {  // sstaticop (sget-*, sput-*)
          // CCCC from e.g., iget vA, vB, field@CCCC.
          return value.instr_offset + 2;
        }
        return kInvalidOffset;
      });
  auto mapper = base::BindRepeating(ReadTargetIndex<uint16_t>, image_,
                                    field_map_item_, sizeof(dex::FieldIdItem));
  return std::make_unique<InstructionReferenceReader>(
      image_, lo, hi, code_item_offsets_, std::move(filter), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadCodeToMethodId16(
    offset_t lo,
    offset_t hi) {
  auto filter = base::BindRepeating(
      [](const InstructionParser::Value& value) -> offset_t {
        if (value.instr->format == dex::FormatId::c &&
            (value.instr->opcode == 0x6E ||   // invoke-kind
             value.instr->opcode == 0x74 ||   // invoke-kind/range
             value.instr->opcode == 0xFA ||   // invoke-polymorphic
             value.instr->opcode == 0xFB)) {  // invoke-polymorphic/range
          // BBBB from e.g., invoke-virtual {vC, vD, vE, vF, vG}, meth@BBBB.
          return value.instr_offset + 2;
        }
        return kInvalidOffset;
      });
  auto mapper =
      base::BindRepeating(ReadTargetIndex<uint16_t>, image_, method_map_item_,
                          sizeof(dex::MethodIdItem));
  return std::make_unique<InstructionReferenceReader>(
      image_, lo, hi, code_item_offsets_, std::move(filter), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadCodeToRelCode8(
    offset_t lo,
    offset_t hi) {
  auto filter = base::BindRepeating(
      [](const InstructionParser::Value& value) -> offset_t {
        if (value.instr->format == dex::FormatId::t &&
            value.instr->opcode == 0x28) {  // goto
          // +AA from e.g., goto +AA.
          return value.instr_offset + 1;
        }
        return kInvalidOffset;
      });
  auto mapper = base::BindRepeating(
      [](DisassemblerDex* dis, offset_t location) {
        // Address is relative to the current instruction, which begins 1 unit
        // before |location|. This needs to be subtracted out. Also, store as
        // int32_t so |unsafe_delta - 1| won't underflow!
        int32_t unsafe_delta = dis->image_.read<int8_t>(location);
        offset_t unsafe_target = static_cast<offset_t>(
            location + (unsafe_delta - 1) * kInstrUnitSize);
        // TODO(huangs): Check that |unsafe_target| stays within code item.
        return unsafe_target;
      },
      base::Unretained(this));
  return std::make_unique<InstructionReferenceReader>(
      image_, lo, hi, code_item_offsets_, std::move(filter), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadCodeToRelCode16(
    offset_t lo,
    offset_t hi) {
  auto filter = base::BindRepeating(
      [](const InstructionParser::Value& value) -> offset_t {
        if (value.instr->format == dex::FormatId::t &&
            (value.instr->opcode == 0x29 ||   // goto/16
             value.instr->opcode == 0x32 ||   // if-test
             value.instr->opcode == 0x38)) {  // if-testz
          // +AAAA from e.g., goto/16 +AAAA.
          return value.instr_offset + 2;
        }
        return kInvalidOffset;
      });
  auto mapper = base::BindRepeating(
      [](DisassemblerDex* dis, offset_t location) {
        // Address is relative to the current instruction, which begins 1 unit
        // before |location|. This needs to be subtracted out. Also, store as
        // int32_t so |unsafe_delta - 1| won't underflow!
        int32_t unsafe_delta = dis->image_.read<int16_t>(location);
        offset_t unsafe_target = static_cast<offset_t>(
            location + (unsafe_delta - 1) * kInstrUnitSize);
        // TODO(huangs): Check that |unsafe_target| stays within code item.
        return unsafe_target;
      },
      base::Unretained(this));
  return std::make_unique<InstructionReferenceReader>(
      image_, lo, hi, code_item_offsets_, std::move(filter), std::move(mapper));
}

std::unique_ptr<ReferenceReader> DisassemblerDex::MakeReadCodeToRelCode32(
    offset_t lo,
    offset_t hi) {
  auto filter = base::BindRepeating(
      [](const InstructionParser::Value& value) -> offset_t {
        if (value.instr->format == dex::FormatId::t &&
            (value.instr->opcode == 0x26 ||   // fill-array-data
             value.instr->opcode == 0x2A ||   // goto/32
             value.instr->opcode == 0x2B ||   // packed-switch
             value.instr->opcode == 0x2C)) {  // sparse-switch
          // +BBBBBBBB from e.g., fill-array-data vAA, +BBBBBBBB.
          // +AAAAAAAA from e.g., goto/32 +AAAAAAAA.
          return value.instr_offset + 2;
        }
        return kInvalidOffset;
      });
  auto mapper = base::BindRepeating(
      [](DisassemblerDex* dis, offset_t location) {
        // Address is relative to the current instruction, which begins 1 unit
        // before |location|. This needs to be subtracted out. Use int64_t to
        // avoid underflow and overflow.
        int64_t unsafe_delta = dis->image_.read<int32_t>(location);
        int64_t unsafe_target = location + (unsafe_delta - 1) * kInstrUnitSize;

        // TODO(huangs): Check that |unsafe_target| stays within code item.
        offset_t checked_unsafe_target =
            static_cast<offset_t>(base::CheckedNumeric<offset_t>(unsafe_target)
                                      .ValueOrDefault(kInvalidOffset));
        return checked_unsafe_target < kOffsetBound ? checked_unsafe_target
                                                    : kInvalidOffset;
      },
      base::Unretained(this));
  return std::make_unique<InstructionReferenceReader>(
      image_, lo, hi, code_item_offsets_, std::move(filter), std::move(mapper));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteStringId16(
    MutableBufferView image) {
  auto writer = base::BindRepeating(
      WriteTargetIndex<uint16_t>, string_map_item_, sizeof(dex::StringIdItem));
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteStringId32(
    MutableBufferView image) {
  auto writer = base::BindRepeating(
      WriteTargetIndex<uint32_t>, string_map_item_, sizeof(dex::StringIdItem));
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteTypeId16(
    MutableBufferView image) {
  auto writer = base::BindRepeating(WriteTargetIndex<uint16_t>, type_map_item_,
                                    sizeof(dex::TypeIdItem));
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteTypeId32(
    MutableBufferView image) {
  auto writer = base::BindRepeating(WriteTargetIndex<uint32_t>, type_map_item_,
                                    sizeof(dex::TypeIdItem));
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteProtoId16(
    MutableBufferView image) {
  auto writer = base::BindRepeating(WriteTargetIndex<uint16_t>, proto_map_item_,
                                    sizeof(dex::ProtoIdItem));
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteFieldId16(
    MutableBufferView image) {
  auto writer = base::BindRepeating(WriteTargetIndex<uint16_t>, field_map_item_,
                                    sizeof(dex::FieldIdItem));
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteFieldId32(
    MutableBufferView image) {
  auto writer = base::BindRepeating(WriteTargetIndex<uint32_t>, field_map_item_,
                                    sizeof(dex::FieldIdItem));
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteMethodId16(
    MutableBufferView image) {
  auto writer = base::BindRepeating(
      WriteTargetIndex<uint16_t>, method_map_item_, sizeof(dex::MethodIdItem));
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteMethodId32(
    MutableBufferView image) {
  auto writer = base::BindRepeating(
      WriteTargetIndex<uint32_t>, method_map_item_, sizeof(dex::MethodIdItem));
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteCallSiteId16(
    MutableBufferView image) {
  auto writer =
      base::BindRepeating(WriteTargetIndex<uint16_t>, call_site_map_item_,
                          sizeof(dex::CallSiteIdItem));
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteMethodHandle16(
    MutableBufferView image) {
  auto writer =
      base::BindRepeating(WriteTargetIndex<uint16_t>, method_handle_map_item_,
                          sizeof(dex::MethodHandleItem));
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteRelCode8(
    MutableBufferView image) {
  auto writer = base::BindRepeating([](Reference ref, MutableBufferView image) {
    ptrdiff_t unsafe_byte_diff =
        static_cast<ptrdiff_t>(ref.target) - ref.location;
    DCHECK_EQ(0, unsafe_byte_diff % kInstrUnitSize);
    // |delta| is relative to start of instruction, which is 1 unit before
    // |ref.location|. The subtraction above removed too much, so +1 to fix.
    base::CheckedNumeric<int8_t> delta((unsafe_byte_diff / kInstrUnitSize) + 1);
    if (!delta.IsValid()) {
      LOG(ERROR) << "Invalid reference at: " << AsHex<8>(ref.location) << ".";
      return;
    }
    image.write<int8_t>(ref.location, delta.ValueOrDie());
  });
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteRelCode16(
    MutableBufferView image) {
  auto writer = base::BindRepeating([](Reference ref, MutableBufferView image) {
    ptrdiff_t unsafe_byte_diff =
        static_cast<ptrdiff_t>(ref.target) - ref.location;
    DCHECK_EQ(0, unsafe_byte_diff % kInstrUnitSize);
    // |delta| is relative to start of instruction, which is 1 unit before
    // |ref.location|. The subtraction above removed too much, so +1 to fix.
    base::CheckedNumeric<int16_t> delta((unsafe_byte_diff / kInstrUnitSize) +
                                        1);
    if (!delta.IsValid()) {
      LOG(ERROR) << "Invalid reference at: " << AsHex<8>(ref.location) << ".";
      return;
    }
    image.write<int16_t>(ref.location, delta.ValueOrDie());
  });
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteRelCode32(
    MutableBufferView image) {
  auto writer = base::BindRepeating([](Reference ref, MutableBufferView image) {
    ptrdiff_t unsafe_byte_diff =
        static_cast<ptrdiff_t>(ref.target) - ref.location;
    DCHECK_EQ(0, unsafe_byte_diff % kInstrUnitSize);
    // |delta| is relative to start of instruction, which is 1 unit before
    // |ref.location|. The subtraction above removed too much, so +1 to fix.
    base::CheckedNumeric<int32_t> delta((unsafe_byte_diff / kInstrUnitSize) +
                                        1);
    if (!delta.IsValid()) {
      LOG(ERROR) << "Invalid reference at: " << AsHex<8>(ref.location) << ".";
      return;
    }
    image.write<int32_t>(ref.location, delta.ValueOrDie());
  });
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

std::unique_ptr<ReferenceWriter> DisassemblerDex::MakeWriteAbs32(
    MutableBufferView image) {
  auto writer = base::BindRepeating([](Reference ref, MutableBufferView image) {
    image.write<uint32_t>(ref.location, ref.target);
  });
  return std::make_unique<ReferenceWriterAdaptor>(image, std::move(writer));
}

bool DisassemblerDex::Parse(ConstBufferView image) {
  image_ = image;
  return ParseHeader();
}

bool DisassemblerDex::ParseHeader() {
  ReadDexHeaderResults results;
  if (!ReadDexHeader(image_, &results))
    return false;

  header_ = results.header;
  dex_version_ = results.dex_version;
  BufferSource source = results.source;

  // DEX header contains file size, so use it to resize |image_| right away.
  image_.shrink(header_->file_size);

  // Read map list. This is not a fixed-size array, so instead of reading
  // MapList directly, read |MapList::size| first, then visit elements in
  // |MapList::list|.
  static_assert(
      offsetof(dex::MapList, list) == sizeof(decltype(dex::MapList::size)),
      "MapList size error.");
  source = BufferSource(image_, header_->map_off);
  decltype(dex::MapList::size) list_size = 0;
  if (!source.GetValue(&list_size) || list_size > dex::kMaxItemListSize)
    return false;
  const auto* item_list = source.GetArray<const dex::MapItem>(list_size);
  if (!item_list)
    return false;

  // Read and validate map list, ensuring that required item types are present.
  // GetItemBaseSize() should have an entry for each item.
  for (offset_t i = 0; i < list_size; ++i) {
    const dex::MapItem* item = &item_list[i];
    // Reject unreasonably large |item->size|.
    size_t item_size = GetItemBaseSize(item->type);
    // Confusing name: |item->size| is actually the number of items.
    if (!image_.covers_array(item->offset, item->size, item_size))
      return false;
    if (!map_item_map_.insert(std::make_pair(item->type, item)).second)
      return false;  // A given type must appear at most once.
  }

  // Make local copies of main map items.
  if (map_item_map_.count(dex::kTypeStringIdItem)) {
    string_map_item_ = *map_item_map_[dex::kTypeStringIdItem];
  }
  if (map_item_map_.count(dex::kTypeTypeIdItem)) {
    type_map_item_ = *map_item_map_[dex::kTypeTypeIdItem];
  }
  if (map_item_map_.count(dex::kTypeProtoIdItem)) {
    proto_map_item_ = *map_item_map_[dex::kTypeProtoIdItem];
  }
  if (map_item_map_.count(dex::kTypeFieldIdItem)) {
    field_map_item_ = *map_item_map_[dex::kTypeFieldIdItem];
  }
  if (map_item_map_.count(dex::kTypeMethodIdItem)) {
    method_map_item_ = *map_item_map_[dex::kTypeMethodIdItem];
  }
  if (map_item_map_.count(dex::kTypeClassDefItem)) {
    class_def_map_item_ = *map_item_map_[dex::kTypeClassDefItem];
  }
  if (map_item_map_.count(dex::kTypeCallSiteIdItem)) {
    call_site_map_item_ = *map_item_map_[dex::kTypeCallSiteIdItem];
  }
  if (map_item_map_.count(dex::kTypeMethodHandleItem)) {
    method_handle_map_item_ = *map_item_map_[dex::kTypeMethodHandleItem];
  }
  if (map_item_map_.count(dex::kTypeTypeList)) {
    type_list_map_item_ = *map_item_map_[dex::kTypeTypeList];
  }
  if (map_item_map_.count(dex::kTypeAnnotationSetRefList)) {
    annotation_set_ref_list_map_item_ =
        *map_item_map_[dex::kTypeAnnotationSetRefList];
  }
  if (map_item_map_.count(dex::kTypeAnnotationSetItem)) {
    annotation_set_map_item_ = *map_item_map_[dex::kTypeAnnotationSetItem];
  }
  if (map_item_map_.count(dex::kTypeCodeItem)) {
    code_map_item_ = *map_item_map_[dex::kTypeCodeItem];
  }
  if (map_item_map_.count(dex::kTypeAnnotationsDirectoryItem)) {
    annotations_directory_map_item_ =
        *map_item_map_[dex::kTypeAnnotationsDirectoryItem];
  }

  // Iteratively parse variable length lists, annotations directory items, and
  // code items blocks. Any failure would indicate invalid DEX. Success
  // indicates that no structural problem is found. However, contained
  // references data read from parsed items still require validation.
  if (!(ParseItemOffsets(image_, type_list_map_item_, sizeof(dex::TypeItem),
                         &type_list_offsets_) &&
        ParseItemOffsets(image_, annotation_set_ref_list_map_item_,
                         sizeof(dex::AnnotationSetRefItem),
                         &annotation_set_ref_list_offsets_) &&
        ParseItemOffsets(image_, annotation_set_map_item_,
                         sizeof(dex::AnnotationOffItem),
                         &annotation_set_offsets_) &&
        ParseAnnotationsDirectoryItems(
            image_, annotations_directory_map_item_,
            &annotations_directory_item_offsets_,
            &annotations_directory_item_field_annotation_offsets_,
            &annotations_directory_item_method_annotation_offsets_,
            &annotations_directory_item_parameter_annotation_offsets_))) {
    return false;
  }
  CodeItemParser code_item_parser(image_);
  if (!code_item_parser.Init(code_map_item_))
    return false;
  code_item_offsets_.resize(code_map_item_.size);
  for (size_t i = 0; i < code_map_item_.size; ++i) {
    const offset_t code_item_offset = code_item_parser.GetNext();
    if (code_item_offset == kInvalidOffset)
      return false;
    code_item_offsets_[i] = code_item_offset;
  }
  // DEX files are required to have parsable code items.
  return !code_item_offsets_.empty();
}

}  // namespace zucchini
