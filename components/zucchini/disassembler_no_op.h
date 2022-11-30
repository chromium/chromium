// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_DISASSEMBLER_NO_OP_H_
#define COMPONENTS_ZUCCHINI_DISASSEMBLER_NO_OP_H_

#include <memory>
#include <string>
#include <vector>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/disassembler.h"
#include "components/zucchini/image_utils.h"

namespace zucchini {

// This disassembler works on any file and does not look for reference.
class DisassemblerNoOp : public Disassembler {
 public:
  static constexpr uint16_t kVersion = 1;

  DisassemblerNoOp();
  DisassemblerNoOp(const DisassemblerNoOp&) = delete;
  const DisassemblerNoOp& operator=(const DisassemblerNoOp&) = delete;
  ~DisassemblerNoOp() override;

  // Disassembler:
  ExecutableType GetExeType() const override;
  std::string GetExeTypeString() const override;
  std::vector<ReferenceGroup> MakeReferenceGroups() const override;

 private:
  friend Disassembler;

  bool Parse(ConstBufferView image) override;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_DISASSEMBLER_NO_OP_H_
