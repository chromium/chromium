// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/disassembler.h"

#include "base/check_op.h"

namespace zucchini {

/******** EmptyReferenceReader ********/

std::optional<Reference> EmptyReferenceReader::GetNext() {
  return std::nullopt;
}

/******** EmptyReferenceWriter ********/

void EmptyReferenceWriter::PutNext(Reference /* reference */) {}

/******** ReferenceGroup ********/

std::unique_ptr<ReferenceReader> ReferenceGroup::GetReader(
    offset_t lower,
    offset_t upper,
    Disassembler* disasm) const {
  DCHECK_LE(lower, upper);
  DCHECK_LE(upper, disasm->size());
  return (disasm->*reader_factory_)(lower, upper);
}

std::unique_ptr<ReferenceReader> ReferenceGroup::GetReader(
    Disassembler* disasm) const {
  return (disasm->*reader_factory_)(0, static_cast<offset_t>(disasm->size()));
}

std::unique_ptr<ReferenceWriter> ReferenceGroup::GetWriter(
    MutableBufferView image,
    Disassembler* disasm) const {
  DCHECK_EQ(image.begin(), disasm->image().begin());
  DCHECK_EQ(image.size(), disasm->size());
  return (disasm->*writer_factory_)(image);
}

std::unique_ptr<ReferenceMixer> ReferenceGroup::GetMixer(
    ConstBufferView old_image,
    ConstBufferView new_image,
    Disassembler* disasm) const {
  if (mixer_factory_)
    return (disasm->*mixer_factory_)(old_image, new_image);
  return nullptr;
}

/******** Disassembler ********/

Disassembler::Disassembler(int num_equivalence_iterations)
    : num_equivalence_iterations_(num_equivalence_iterations) {}

Disassembler::~Disassembler() = default;

}  // namespace zucchini
