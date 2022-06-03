// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/fuzzers/pdfium_xfa_lpm_fuzzer/simple_xfa_pdf.h"

#include <string>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kEmptyXfa[] = "";
const char kEmptyXfaPdf[] = R"(%PDF-1.7
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
/Length 1
>>
stream

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
376
%%EOF)";

}  // namespace

class SimpleXfaPdfTest
    : public ::testing::TestWithParam<std::pair<std::string, std::string>> {};

TEST_P(SimpleXfaPdfTest, CheckTranslation) {
  const std::pair<std::string, std::string>& param = GetParam();
  ASSERT_EQ(param.second,
            pdfium_xfa_lpm_fuzzer::CreateSimpleXfaPdf(param.first));
}

INSTANTIATE_TEST_SUITE_P(LpmFuzzer,
                         SimpleXfaPdfTest,
                         ::testing::Values(std::make_pair(kEmptyXfa,
                                                          kEmptyXfaPdf)));
