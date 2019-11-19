// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "pdf/pdfium/fuzzers/pdfium_xfa_lpm_fuzzer/simple_xfa_pdf.h"

namespace pdfium_xfa_lpm_fuzzer {

namespace {

const char kSimplePdfTemplate[] = R"(%PDF-1.7
%
  
1 0 obj <<
/XFA 5 0 R
>>
endobj
2 0 obj <<
/Type /Pages
/Count 1
/Kids [3 0 R]
>>
endobj
3 0 obj <<
/Type /Page
/MediaBox [0 0 600 800]
/Parent 2 0 R
>>
endobj
4 0 obj <<
/Extensions <<
/ADBE <<
/BaseVersion /1.0
/ExtensionLevel 8
>>
>>
/Pages 2 0 R
/Type /Catalog
/AcroForm 1 0 R
/NeedsRendering true
>>
endobj
5 0 obj <<
/Length $1
>>
stream
$2
endstream
endobj
xref
6 0
0000000000 65535 f 
0000000015 00000 n 
0000000047 00000 n 
0000000104 00000 n 
0000000175 00000 n 
0000000327 00000 n 
trailer <<
/Size 6
/Root 4 0 R
>>
startxref
$3
%%EOF)";

}  // namespace

std::string CreateSimpleXfaPdf(const std::string& xfa_string) {
  // Add 1 for newline before endstream.
  std::string xfa_stream_len = base::NumberToString(xfa_string.size() + 1);
  // Each placeholder is two bytes. Two of them precede xref.
  const size_t kPlaceholderSizes = 2 * 2;
  static const size_t kCurrentXrefPosition =
      std::string(kSimplePdfTemplate).find("xref");
  std::string startxref =
      base::NumberToString(kCurrentXrefPosition - kPlaceholderSizes +
                           xfa_string.size() + xfa_stream_len.size() + 1);
  std::vector<std::string> replacements(
      {xfa_stream_len, xfa_string, startxref});
  return base::ReplaceStringPlaceholders(kSimplePdfTemplate, replacements,
                                         nullptr);
}

}  // namespace pdfium_xfa_lpm_fuzzer
