// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "pdf/pdfium/fuzzers/pdfium_xfa_lpm_fuzzer/proto_to_xfa.h"
#include "pdf/pdfium/fuzzers/pdfium_xfa_lpm_fuzzer/simple_xfa_pdf.h"
#include "pdf/pdfium/fuzzers/pdfium_xfa_lpm_fuzzer/xfa.pb.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "third_party/pdfium/testing/fuzzers/pdfium_xfa_lpm_fuzz_stub.h"

namespace pdfium_xfa_lpm_fuzzer {

DEFINE_PROTO_FUZZER(const xfa_proto::Xfa& xfa) {
  xfa_proto::ProtoToXfa proto_to_xfa;
  std::string xfa_string = proto_to_xfa.Convert(xfa);
  std::string pdf_string = CreateSimpleXfaPdf(xfa_string);
  if (getenv("LPM_DUMP_NATIVE_INPUT")) {
    for (auto c : pdf_string)
      putc(c, stdout);
  }
  PdfiumXFALPMFuzzStub(pdf_string.c_str(), pdf_string.size());
}

}  // namespace pdfium_xfa_lpm_fuzzer
