// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/fuzzers/fuzz_utils.h"

#include <map>
#include <memory>
#include <vector>

#include "components/zucchini/disassembler.h"

namespace zucchini {

void ReadAndWriteReferences(
    std::unique_ptr<zucchini::Disassembler> disassembler,
    std::vector<uint8_t>* mutable_data) {
  zucchini::MutableBufferView mutable_image(mutable_data->data(),
                                            disassembler->size());
  std::vector<zucchini::Reference> references;
  auto groups = disassembler->MakeReferenceGroups();
  std::map<zucchini::PoolTag, std::vector<zucchini::Reference>>
      references_of_pool;
  for (const auto& group : groups) {
    auto reader = group.GetReader(disassembler.get());
    std::vector<zucchini::Reference>* refs =
        &references_of_pool[group.pool_tag()];
    for (auto ref = reader->GetNext(); ref.has_value();
         ref = reader->GetNext()) {
      refs->push_back(ref.value());
    }
  }
  for (const auto& group : groups) {
    auto writer = group.GetWriter(mutable_image, disassembler.get());
    for (const auto& ref : references_of_pool[group.pool_tag()])
      writer->PutNext(ref);
  }
}

}  // namespace zucchini
