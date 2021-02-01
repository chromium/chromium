// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/encoded_program.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "courgette/label_manager.h"
#include "courgette/streams.h"

namespace courgette {

namespace {

// Serializes a vector of integral values using Varint32 coding.
template<typename V>
CheckBool WriteVector(const V& items, SinkStream* buffer) {
  size_t count = items.size();
  bool ok = buffer->WriteSizeVarint32(count);
  for (size_t i = 0; ok && i < count;  ++i) {
    ok = buffer->WriteSizeVarint32(items[i]);
  }
  return ok;
}

template<typename V>
bool ReadVector(V* items, SourceStream* buffer) {
  uint32_t count;
  if (!buffer->ReadVarint32(&count))
    return false;

  items->clear();

  bool ok = items->reserve(count);
  for (size_t i = 0;  ok && i < count;  ++i) {
    uint32_t item;
    ok = buffer->ReadVarint32(&item);
    if (ok)
      ok = items->push_back(static_cast<typename V::value_type>(item));
  }

  return ok;
}

// Serializes a vector, using delta coding followed by Varint32Signed coding.
template<typename V>
CheckBool WriteSigned32Delta(const V& set, SinkStream* buffer) {
  size_t count = set.size();
  bool ok = buffer->WriteSizeVarint32(count);
  uint32_t prev = 0;
  for (size_t i = 0; ok && i < count; ++i) {
    uint32_t current = set[i];
    int32_t delta = current - prev;
    ok = buffer->WriteVarint32Signed(delta);
    prev = current;
  }
  return ok;
}

template <typename V>
static CheckBool ReadSigned32Delta(V* set, SourceStream* buffer) {
  uint32_t count;

  if (!buffer->ReadVarint32(&count))
    return false;

  set->clear();
  bool ok = set->reserve(count);
  uint32_t prev = 0;
  for (size_t i = 0; ok && i < count; ++i) {
    int32_t delta;
    ok = buffer->ReadVarint32Signed(&delta);
    if (ok) {
      uint32_t current = static_cast<uint32_t>(prev + delta);
      ok = set->push_back(current);
      prev = current;
    }
  }
  return ok;
}

// Write a vector as the byte representation of the contents.
//
// (This only really makes sense for a type T that has sizeof(T)==1, otherwise
// serialized representation is not endian-agnostic.  But it is useful to keep
// the possibility of a greater size for experiments comparing Varint32 encoding
// of a vector of larger integrals vs a plain form.)
//
template<typename V>
CheckBool WriteVectorU8(const V& items, SinkStream* buffer) {
  size_t count = items.size();
  bool ok = buffer->WriteSizeVarint32(count);
  if (count != 0 && ok) {
    size_t byte_count = count * sizeof(typename V::value_type);
    ok = buffer->Write(static_cast<const void*>(&items[0]), byte_count);
  }
  return ok;
}

template<typename V>
bool ReadVectorU8(V* items, SourceStream* buffer) {
  uint32_t count;
  if (!buffer->ReadVarint32(&count))
    return false;

  items->clear();
  bool ok = items->resize(count, 0);
  if (ok && count != 0) {
    size_t byte_count = count * sizeof(typename V::value_type);
    return buffer->Read(static_cast<void*>(&((*items)[0])), byte_count);
  }
  return ok;
}

/******** InstructionStoreReceptor ********/

// An InstructionReceptor that stores emitted instructions.
class InstructionStoreReceptor : public InstructionReceptor {
 public:
  explicit InstructionStoreReceptor(ExecutableType exe_type,
                                    EncodedProgram* encoded)
      : exe_type_(exe_type), encoded_(encoded) {
    CHECK(encoded_);
  }

  CheckBool EmitPeRelocs() override {
    return encoded_->AddPeMakeRelocs(exe_type_);
  }
  CheckBool EmitElfRelocation() override {
    return encoded_->AddElfMakeRelocs();
  }
  CheckBool EmitOrigin(RVA rva) override { return encoded_->AddOrigin(rva); }
  CheckBool EmitSingleByte(uint8_t byte) override {
    return encoded_->AddCopy(1, &byte);
  }
  CheckBool EmitMultipleBytes(const uint8_t* bytes, size_t len) override {
    return encoded_->AddCopy(len, bytes);
  }
  CheckBool EmitRel32(Label* label) override {
    return encoded_->AddRel32(label->index_);
  }
  CheckBool EmitAbs32(Label* label) override {
    return encoded_->AddAbs32(label->index_);
  }
  CheckBool EmitAbs64(Label* label) override {
    return encoded_->AddAbs64(label->index_);
  }

 private:
  ExecutableType exe_type_;
  EncodedProgram* encoded_;

  DISALLOW_COPY_AND_ASSIGN(InstructionStoreReceptor);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////

// Constructor is here rather than in the header. Although the constructor
// appears to do nothing it is fact quite large because of the implicit calls to
// field constructors. Ditto for the destructor.
EncodedProgram::EncodedProgram() = default;
EncodedProgram::~EncodedProgram() = default;

CheckBool EncodedProgram::ImportLabels(
    const LabelManager& abs32_label_manager,
    const LabelManager& rel32_label_manager) {
  if (!WriteRvasToList(abs32_label_manager, &abs32_rva_) ||
      !WriteRvasToList(rel32_label_manager, &rel32_rva_)) {
    return false;
  }
  FillUnassignedRvaSlots(&abs32_rva_);
  FillUnassignedRvaSlots(&rel32_rva_);
  return true;
}

CheckBool EncodedProgram::AddOrigin(RVA origin) {
  return ops_.push_back(ORIGIN) && origins_.push_back(origin);
}

CheckBool EncodedProgram::AddCopy(size_t count, const void* bytes) {
  const uint8_t* source = static_cast<const uint8_t*>(bytes);

  bool ok = true;

  // Fold adjacent COPY instructions into one.  This nearly halves the size of
  // an EncodedProgram with only COPY1 instructions since there are approx plain
  // 16 bytes per reloc.  This has a working-set benefit during decompression.
  // For compression of files with large differences this makes a small (4%)
  // improvement in size.  For files with small differences this degrades the
  // compressed size by 1.3%
  if (!ops_.empty()) {
    if (ops_.back() == COPY1) {
      ops_.back() = COPY;
      ok = copy_counts_.push_back(1);
    }
    if (ok && ops_.back() == COPY) {
      copy_counts_.back() += count;
      for (size_t i = 0; ok && i < count; ++i) {
        ok = copy_bytes_.push_back(source[i]);
      }
      return ok;
    }
  }

  if (ok) {
    if (count == 1) {
      ok = ops_.push_back(COPY1) && copy_bytes_.push_back(source[0]);
    } else {
      ok = ops_.push_back(COPY) && copy_counts_.push_back(count);
      for (size_t i = 0; ok && i < count; ++i) {
        ok = copy_bytes_.push_back(source[i]);
      }
    }
  }

  return ok;
}

CheckBool EncodedProgram::AddAbs32(int label_index) {
  return ops_.push_back(ABS32) && abs32_ix_.push_back(label_index);
}

CheckBool EncodedProgram::AddAbs64(int label_index) {
  return ops_.push_back(ABS64) && abs32_ix_.push_back(label_index);
}

CheckBool EncodedProgram::AddRel32(int label_index) {
  return ops_.push_back(REL32) && rel32_ix_.push_back(label_index);
}

CheckBool EncodedProgram::AddPeMakeRelocs(ExecutableType kind) {
  if (kind == EXE_WIN_32_X86)
    return ops_.push_back(MAKE_PE_RELOCATION_TABLE);
  return ops_.push_back(MAKE_PE64_RELOCATION_TABLE);
}

CheckBool EncodedProgram::AddElfMakeRelocs() {
  return ops_.push_back(MAKE_ELF_RELOCATION_TABLE);
}

void EncodedProgram::DebuggingSummary() {
  VLOG(1) << "EncodedProgram Summary"
          << "\n  image base  " << image_base_
          << "\n  abs32 rvas  " << abs32_rva_.size()
          << "\n  rel32 rvas  " << rel32_rva_.size()
          << "\n  ops         " << ops_.size()
          << "\n  origins     " << origins_.size()
          << "\n  copy_counts " << copy_counts_.size()
          << "\n  copy_bytes  " << copy_bytes_.size()
          << "\n  abs32_ix    " << abs32_ix_.size()
          << "\n  rel32_ix    " << rel32_ix_.size();
}

////////////////////////////////////////////////////////////////////////////////

// For algorithm refinement purposes it is useful to write subsets of the file
// format.  This gives us the ability to estimate the entropy of the
// differential compression of the individual streams, which can provide
// invaluable insights.  The default, of course, is to include all the streams.
//
enum FieldSelect {
  INCLUDE_ABS32_ADDRESSES = 0x0001,
  INCLUDE_REL32_ADDRESSES = 0x0002,
  INCLUDE_ABS32_INDEXES   = 0x0010,
  INCLUDE_REL32_INDEXES   = 0x0020,
  INCLUDE_OPS             = 0x0100,
  INCLUDE_BYTES           = 0x0200,
  INCLUDE_COPY_COUNTS     = 0x0400,
  INCLUDE_MISC            = 0x1000
};

static FieldSelect GetFieldSelect() {
  // TODO(sra): Use better configuration.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string s;
  env->GetVar("A_FIELDS", &s);
  uint64_t fields;
  if (!base::StringToUint64(s, &fields))
    return static_cast<FieldSelect>(~0);
  return static_cast<FieldSelect>(fields);
}

CheckBool EncodedProgram::WriteTo(SinkStreamSet* streams) {
  FieldSelect select = GetFieldSelect();

  // The order of fields must be consistent in WriteTo and ReadFrom, regardless
  // of the streams used.  The code can be configured with all kStreamXXX
  // constants the same.
  //
  // If we change the code to pipeline reading with assembly (to avoid temporary
  // storage vectors by consuming operands directly from the stream) then we
  // need to read the base address and the random access address tables first,
  // the rest can be interleaved.

  if (select & INCLUDE_MISC) {
    uint32_t high = static_cast<uint32_t>(image_base_ >> 32);
    uint32_t low = static_cast<uint32_t>(image_base_ & 0xffffffffU);

    if (!streams->stream(kStreamMisc)->WriteVarint32(high) ||
        !streams->stream(kStreamMisc)->WriteVarint32(low)) {
      return false;
    }
  }

  bool success = true;

  if (select & INCLUDE_ABS32_ADDRESSES) {
    success &= WriteSigned32Delta(abs32_rva_,
                                  streams->stream(kStreamAbs32Addresses));
  }

  if (select & INCLUDE_REL32_ADDRESSES) {
    success &= WriteSigned32Delta(rel32_rva_,
                                  streams->stream(kStreamRel32Addresses));
  }

  if (select & INCLUDE_MISC)
    success &= WriteVector(origins_, streams->stream(kStreamOriginAddresses));

  if (select & INCLUDE_OPS) {
    // 5 for length.
    success &= streams->stream(kStreamOps)->Reserve(ops_.size() + 5);
    success &= WriteVector(ops_, streams->stream(kStreamOps));
  }

  if (select & INCLUDE_COPY_COUNTS)
    success &= WriteVector(copy_counts_, streams->stream(kStreamCopyCounts));

  if (select & INCLUDE_BYTES)
    success &= WriteVectorU8(copy_bytes_, streams->stream(kStreamBytes));

  if (select & INCLUDE_ABS32_INDEXES)
    success &= WriteVector(abs32_ix_, streams->stream(kStreamAbs32Indexes));

  if (select & INCLUDE_REL32_INDEXES)
    success &= WriteVector(rel32_ix_, streams->stream(kStreamRel32Indexes));

  return success;
}

bool EncodedProgram::ReadFrom(SourceStreamSet* streams) {
  uint32_t high;
  uint32_t low;

  if (!streams->stream(kStreamMisc)->ReadVarint32(&high) ||
      !streams->stream(kStreamMisc)->ReadVarint32(&low)) {
    return false;
  }
  image_base_ = (static_cast<uint64_t>(high) << 32) | low;

  if (!ReadSigned32Delta(&abs32_rva_, streams->stream(kStreamAbs32Addresses)))
    return false;
  if (!ReadSigned32Delta(&rel32_rva_, streams->stream(kStreamRel32Addresses)))
    return false;
  if (!ReadVector(&origins_, streams->stream(kStreamOriginAddresses)))
    return false;
  if (!ReadVector(&ops_, streams->stream(kStreamOps)))
    return false;
  if (!ReadVector(&copy_counts_, streams->stream(kStreamCopyCounts)))
    return false;
  if (!ReadVectorU8(&copy_bytes_, streams->stream(kStreamBytes)))
    return false;
  if (!ReadVector(&abs32_ix_, streams->stream(kStreamAbs32Indexes)))
    return false;
  if (!ReadVector(&rel32_ix_, streams->stream(kStreamRel32Indexes)))
    return false;

  // Check that streams have been completely consumed.
  for (int i = 0;  i < kStreamLimit;  ++i) {
    if (streams->stream(i)->Remaining() > 0)
      return false;
  }

  return true;
}

// Safe, non-throwing version of std::vector::at().  Returns 'true' for success,
// 'false' for out-of-bounds index error.
template<typename V, typename T>
bool VectorAt(const V& v, size_t index, T* output) {
  if (index >= v.size())
    return false;
  *output = v[index];
  return true;
}

CheckBool EncodedProgram::AssembleTo(SinkStream* final_buffer) {
  // For the most part, the assembly process walks the various tables.
  // ix_mumble is the index into the mumble table.
  size_t ix_origins = 0;
  size_t ix_copy_counts = 0;
  size_t ix_copy_bytes = 0;
  size_t ix_abs32_ix = 0;
  size_t ix_rel32_ix = 0;

  RVA current_rva = 0;

  bool pending_pe_relocation_table = false;
  uint8_t pending_pe_relocation_table_type = 0x03;  // IMAGE_REL_BASED_HIGHLOW
  Elf32_Word pending_elf_relocation_table_type = 0;
  SinkStream bytes_following_relocation_table;

  SinkStream* output = final_buffer;

  for (size_t ix_ops = 0;  ix_ops < ops_.size();  ++ix_ops) {
    OP op = ops_[ix_ops];

    switch (op) {
      default:
        return false;

      case ORIGIN: {
        RVA section_rva;
        if (!VectorAt(origins_, ix_origins, &section_rva))
          return false;
        ++ix_origins;
        current_rva = section_rva;
        break;
      }

      case COPY: {
        size_t count;
        if (!VectorAt(copy_counts_, ix_copy_counts, &count))
          return false;
        ++ix_copy_counts;
        for (size_t i = 0;  i < count;  ++i) {
          uint8_t b;
          if (!VectorAt(copy_bytes_, ix_copy_bytes, &b))
            return false;
          ++ix_copy_bytes;
          if (!output->Write(&b, 1))
            return false;
        }
        current_rva += static_cast<RVA>(count);
        break;
      }

      case COPY1: {
        uint8_t b;
        if (!VectorAt(copy_bytes_, ix_copy_bytes, &b))
          return false;
        ++ix_copy_bytes;
        if (!output->Write(&b, 1))
          return false;
        current_rva += 1;
        break;
      }

      case REL32: {
        uint32_t index;
        if (!VectorAt(rel32_ix_, ix_rel32_ix, &index))
          return false;
        ++ix_rel32_ix;
        RVA rva;
        if (!VectorAt(rel32_rva_, index, &rva))
          return false;
        uint32_t offset = (rva - (current_rva + 4));
        if (!output->Write(&offset, 4))
          return false;
        current_rva += 4;
        break;
      }

      case ABS32:
      case ABS64: {
        uint32_t index;
        if (!VectorAt(abs32_ix_, ix_abs32_ix, &index))
          return false;
        ++ix_abs32_ix;
        RVA rva;
        if (!VectorAt(abs32_rva_, index, &rva))
          return false;
        if (op == ABS32) {
          base::CheckedNumeric<uint32_t> abs32 = image_base_;
          abs32 += rva;
          uint32_t safe_abs32 = abs32.ValueOrDie();
          if (!abs32_relocs_.push_back(current_rva) ||
              !output->Write(&safe_abs32, 4)) {
            return false;
          }
          current_rva += 4;
        } else {
          base::CheckedNumeric<uint64_t> abs64 = image_base_;
          abs64 += rva;
          uint64_t safe_abs64 = abs64.ValueOrDie();
          if (!abs32_relocs_.push_back(current_rva) ||
              !output->Write(&safe_abs64, 8)) {
            return false;
          }
          current_rva += 8;
        }
        break;
      }

      case MAKE_PE_RELOCATION_TABLE: {
        // We can see the base relocation anywhere, but we only have the
        // information to generate it at the very end.  So we divert the bytes
        // we are generating to a temporary stream.
        if (pending_pe_relocation_table)
          return false;  // Can't have two base relocation tables.

        pending_pe_relocation_table = true;
        output = &bytes_following_relocation_table;
        break;
        // There is a potential problem *if* the instruction stream contains
        // some REL32 relocations following the base relocation and in the same
        // section.  We don't know the size of the table, so 'current_rva' will
        // be wrong, causing REL32 offsets to be miscalculated.  This never
        // happens; the base relocation table is usually in a section of its
        // own, a data-only section, and following everything else in the
        // executable except some padding zero bytes.  We could fix this by
        // emitting an ORIGIN after the MAKE_BASE_RELOCATION_TABLE.
      }

      case MAKE_PE64_RELOCATION_TABLE: {
        if (pending_pe_relocation_table)
          return false;  // Can't have two base relocation tables.

        pending_pe_relocation_table = true;
        pending_pe_relocation_table_type = 0x0A;  // IMAGE_REL_BASED_DIR64
        output = &bytes_following_relocation_table;
        break;
      }

      case MAKE_ELF_RELOCATION_TABLE: {
        // We can see the base relocation anywhere, but we only have the
        // information to generate it at the very end.  So we divert the bytes
        // we are generating to a temporary stream.
        if (pending_elf_relocation_table_type)
          return false;  // Can't have two base relocation tables.

        pending_elf_relocation_table_type = R_386_RELATIVE;
        output = &bytes_following_relocation_table;
        break;
      }
    }
  }

  if (pending_pe_relocation_table) {
    if (!GeneratePeRelocations(final_buffer,
                               pending_pe_relocation_table_type) ||
        !final_buffer->Append(&bytes_following_relocation_table))
      return false;
  }

  if (pending_elf_relocation_table_type) {
    if (!GenerateElfRelocations(pending_elf_relocation_table_type,
                                final_buffer) ||
        !final_buffer->Append(&bytes_following_relocation_table))
      return false;
  }

  // Final verification check: did we consume all lists?
  if (ix_copy_counts != copy_counts_.size())
    return false;
  if (ix_copy_bytes != copy_bytes_.size())
    return false;
  if (ix_abs32_ix != abs32_ix_.size())
    return false;
  if (ix_rel32_ix != rel32_ix_.size())
    return false;

  return true;
}

CheckBool EncodedProgram::GenerateInstructions(
    ExecutableType exe_type,
    const InstructionGenerator& gen) {
  InstructionStoreReceptor store_receptor(exe_type, this);
  return gen.Run(&store_receptor);
}

// RelocBlock has the layout of a block of relocations in the base relocation
// table file format.
struct RelocBlockPOD {
  uint32_t page_rva;
  uint32_t block_size;
  uint16_t relocs[4096];  // Allow up to one relocation per byte of a 4k page.
};

static_assert(offsetof(RelocBlockPOD, relocs) == 8, "reloc block header size");

class RelocBlock {
 public:
  RelocBlock() {
    pod.page_rva = 0xFFFFFFFF;
    pod.block_size = 8;
  }

  void Add(uint16_t item) {
    pod.relocs[(pod.block_size-8)/2] = item;
    pod.block_size += 2;
  }

  CheckBool Flush(SinkStream* buffer) WARN_UNUSED_RESULT {
    bool ok = true;
    if (pod.block_size != 8) {
      if (pod.block_size % 4 != 0) {  // Pad to make size multiple of 4 bytes.
        Add(0);
      }
      ok = buffer->Write(&pod, pod.block_size);
      pod.block_size = 8;
    }
    return ok;
  }
  RelocBlockPOD pod;
};

// static
// Updates |rvas| so |rvas[label.index_] == label.rva_| for each |label| in
// |label_manager|, assuming |label.index_| is properly assigned. Takes care of
// |rvas| resizing. Unused slots in |rvas| are assigned |kUnassignedRVA|.
// Returns true on success, and false otherwise.
CheckBool EncodedProgram::WriteRvasToList(const LabelManager& label_manager,
                                          RvaVector* rvas) {
  rvas->clear();
  int index_bound = LabelManager::GetLabelIndexBound(label_manager.Labels());
  if (!rvas->resize(index_bound, kUnassignedRVA))
    return false;

  // For each Label, write its RVA to assigned index.
  for (const Label& label : label_manager.Labels()) {
    DCHECK_NE(label.index_, Label::kNoIndex);
    DCHECK_EQ((*rvas)[label.index_], kUnassignedRVA)
        << "ExportToList() double assigned " << label.index_;
    (*rvas)[label.index_] = label.rva_;
  }
  return true;
}

// static
// Replaces all unassigned slots in |rvas| with the value at the previous index
// so they delta-encode to zero. (There might be better values than zero. The
// way to get that is have the higher level assembly program assign the
// unassigned slots.)
void EncodedProgram::FillUnassignedRvaSlots(RvaVector* rvas) {
  RVA previous = 0;
  for (RVA& rva : *rvas) {
    if (rva == kUnassignedRVA)
      rva = previous;
    else
      previous = rva;
  }
}

CheckBool EncodedProgram::GeneratePeRelocations(SinkStream* buffer,
                                                uint8_t type) {
  std::sort(abs32_relocs_.begin(), abs32_relocs_.end());
  DCHECK(abs32_relocs_.empty() || abs32_relocs_.back() != kUnassignedRVA);

  RelocBlock block;

  bool ok = true;
  for (size_t i = 0;  ok && i < abs32_relocs_.size();  ++i) {
    uint32_t rva = abs32_relocs_[i];
    uint32_t page_rva = rva & ~0xFFF;
    if (page_rva != block.pod.page_rva) {
      ok &= block.Flush(buffer);
      block.pod.page_rva = page_rva;
    }
    if (ok)
      block.Add(((static_cast<uint16_t>(type)) << 12) | (rva & 0xFFF));
  }
  ok &= block.Flush(buffer);
  return ok;
}

CheckBool EncodedProgram::GenerateElfRelocations(Elf32_Word r_info,
                                                 SinkStream* buffer) {
  std::sort(abs32_relocs_.begin(), abs32_relocs_.end());
  DCHECK(abs32_relocs_.empty() || abs32_relocs_.back() != kUnassignedRVA);

  Elf32_Rel relocation_block;

  relocation_block.r_info = r_info;

  bool ok = true;
  for (size_t i = 0;  ok && i < abs32_relocs_.size();  ++i) {
    relocation_block.r_offset = abs32_relocs_[i];
    ok = buffer->Write(&relocation_block, sizeof(Elf32_Rel));
  }

  return ok;
}
////////////////////////////////////////////////////////////////////////////////

Status WriteEncodedProgram(EncodedProgram* encoded, SinkStreamSet* sink) {
  if (!encoded->WriteTo(sink))
    return C_STREAM_ERROR;
  return C_OK;
}

Status ReadEncodedProgram(SourceStreamSet* streams,
                          std::unique_ptr<EncodedProgram>* output) {
  output->reset();
  std::unique_ptr<EncodedProgram> encoded(new EncodedProgram());
  if (!encoded->ReadFrom(streams))
    return C_DESERIALIZATION_FAILED;

  *output = std::move(encoded);
  return C_OK;
}

Status Assemble(EncodedProgram* encoded, SinkStream* buffer) {
  bool assembled = encoded->AssembleTo(buffer);
  if (assembled)
    return C_OK;
  return C_ASSEMBLY_FAILED;
}

}  // namespace courgette
