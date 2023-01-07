// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/fuzzers/pdfium_xfa_lpm_fuzzer/proto_to_xfa.h"

namespace xfa_proto {

ProtoToXfa::ProtoToXfa() = default;
ProtoToXfa::~ProtoToXfa() = default;

std::string ProtoToXfa::Convert(const Xfa& xfa) {
  // TODO(metzman): Actually return an XFA form rather than an empty string.
  return "";
}

}  // namespace xfa_proto
