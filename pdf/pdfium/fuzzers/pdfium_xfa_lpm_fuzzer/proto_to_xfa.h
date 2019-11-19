// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_FUZZERS_PDFIUM_XFA_LPM_FUZZER_PROTO_TO_XFA_H_
#define PDF_PDFIUM_FUZZERS_PDFIUM_XFA_LPM_FUZZER_PROTO_TO_XFA_H_

#include <string>

#include "pdf/pdfium/fuzzers/pdfium_xfa_lpm_fuzzer/xfa.pb.h"

namespace xfa_proto {

class ProtoToXfa {
 public:
  ProtoToXfa();
  ~ProtoToXfa();

  std::string Convert(const Xfa& xfa);
};

}  // namespace xfa_proto

#endif  // PDF_PDFIUM_FUZZERS_PDFIUM_XFA_LPM_FUZZER_PROTO_TO_XFA_H_
