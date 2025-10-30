// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_accessibility_constants_helper.h"

#include "base/containers/fixed_flat_map.h"

namespace chrome_pdf {

// Please keep the entries in the same order as the `PdfTagType` enum.
constexpr auto kStringToPdfTagTypeMap =
    base::MakeFixedFlatMap<std::string_view, PdfTagType>(
        {{"", PdfTagType::kNone},
         {"Document", PdfTagType::kDocument},
         {"Part", PdfTagType::kPart},
         {"Art", PdfTagType::kArt},
         {"Sect", PdfTagType::kSect},
         {"Div", PdfTagType::kDiv},
         {"BlockQuote", PdfTagType::kBlockQuote},
         {"Caption", PdfTagType::kCaption},
         {"TOC", PdfTagType::kTOC},
         {"TOCI", PdfTagType::kTOCI},
         {"Index", PdfTagType::kIndex},
         {"P", PdfTagType::kP},
         {"H", PdfTagType::kH},
         {"H1", PdfTagType::kH1},
         {"H2", PdfTagType::kH2},
         {"H3", PdfTagType::kH3},
         {"H4", PdfTagType::kH4},
         {"H5", PdfTagType::kH5},
         {"H6", PdfTagType::kH6},
         {"L", PdfTagType::kL},
         {"LI", PdfTagType::kLI},
         {"Lbl", PdfTagType::kLbl},
         {"LBody", PdfTagType::kLBody},
         {"Table", PdfTagType::kTable},
         {"TR", PdfTagType::kTR},
         {"TH", PdfTagType::kTH},
         {"THead", PdfTagType::kTHead},
         {"TBody", PdfTagType::kTBody},
         {"TFoot", PdfTagType::kTFoot},
         {"TD", PdfTagType::kTD},
         {"Span", PdfTagType::kSpan},
         {"Link", PdfTagType::kLink},
         {"Figure", PdfTagType::kFigure},
         {"Formula", PdfTagType::kFormula},
         {"Form", PdfTagType::kForm}});

static_assert(kStringToPdfTagTypeMap.size() ==
              static_cast<size_t>(PdfTagType::kUnknown));

PdfTagType PdfTagTypeFromString(const std::string& tag_type) {
  if (auto iter = kStringToPdfTagTypeMap.find(tag_type);
      iter != kStringToPdfTagTypeMap.end()) {
    return iter->second;
  }
  return PdfTagType::kUnknown;
}

const base::fixed_flat_map<std::string_view, PdfTagType, 35>&
GetPdfTagTypeMap() {
  return kStringToPdfTagTypeMap;
}

}  // namespace chrome_pdf
