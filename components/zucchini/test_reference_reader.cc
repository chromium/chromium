// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/test_reference_reader.h"

namespace zucchini {

TestReferenceReader::TestReferenceReader(const std::vector<Reference>& refs)
    : references_(refs) {}

TestReferenceReader::~TestReferenceReader() = default;

std::optional<Reference> TestReferenceReader::GetNext() {
  if (index_ == references_.size())
    return std::nullopt;
  return references_[index_++];
}

}  // namespace zucchini
