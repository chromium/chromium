// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_TEST_DISASSEMBLER_H_
#define COMPONENTS_ZUCCHINI_TEST_DISASSEMBLER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

// A trivial Disassembler that reads injected references of 3 different types.
// This is only meant for testing and is not a full implementation of a
// disassembler. Reading reference ignores bounds, and writing references does
// nothing.
class TestDisassembler : public Disassembler {
 public:
  TestDisassembler(const ReferenceTypeTraits& traits1,
                   const std::vector<Reference>& refs1,
                   const ReferenceTypeTraits& traits2,
                   const std::vector<Reference>& refs2,
                   const ReferenceTypeTraits& traits3,
                   const std::vector<Reference>& refs3);
  TestDisassembler(const TestDisassembler&) = delete;
  const TestDisassembler& operator=(const TestDisassembler&) = delete;
  ~TestDisassembler() override;

  // Disassembler:
  ExecutableType GetExeType() const override;
  std::string GetExeTypeString() const override;
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

  // Disassembler::ReaderFactory:
  std::unique_ptr<ReferenceReader> MakeReadRefs1(offset_t /*lower*/,
                                                 offset_t /*upper*/) {
    return MakeReadRefs(0);
  }
  std::unique_ptr<ReferenceReader> MakeReadRefs2(offset_t /*lower*/,
                                                 offset_t /*upper*/) {
    return MakeReadRefs(1);
  }
  std::unique_ptr<ReferenceReader> MakeReadRefs3(offset_t /*lower*/,
                                                 offset_t /*upper*/) {
    return MakeReadRefs(2);
  }

  // Disassembler::WriterFactory:
  std::unique_ptr<ReferenceWriter> MakeWriteRefs1(MutableBufferView image) {
    return MakeWriteRefs(image);
  }
  std::unique_ptr<ReferenceWriter> MakeWriteRefs2(MutableBufferView image) {
    return MakeWriteRefs(image);
  }
  std::unique_ptr<ReferenceWriter> MakeWriteRefs3(MutableBufferView image) {
    return MakeWriteRefs(image);
  }

 private:
  // Disassembler:
  bool Parse(ConstBufferView image) override;

  std::unique_ptr<ReferenceReader> MakeReadRefs(int type);
  std::unique_ptr<ReferenceWriter> MakeWriteRefs(MutableBufferView image);

  ReferenceTypeTraits traits_[3];
  std::vector<Reference> refs_[3];
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_TEST_DISASSEMBLER_H_
