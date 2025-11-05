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
         {kPDFStructureTypeDocument, PdfTagType::kDocument},
         {kPDFStructureTypePart, PdfTagType::kPart},
         {kPDFStructureTypeArticle, PdfTagType::kArt},
         {kPDFStructureTypeSect, PdfTagType::kSect},
         {kPDFStructureTypeDiv, PdfTagType::kDiv},
         {kPDFStructureTypeBlockQuote, PdfTagType::kBlockQuote},
         {kPDFStructureTypeCaption, PdfTagType::kCaption},
         {kPDFStructureTypeTOC, PdfTagType::kTOC},
         {kPDFStructureTypeTOCI, PdfTagType::kTOCI},
         {kPDFStructureTypeIndex, PdfTagType::kIndex},
         {kPDFStructureTypeParagraph, PdfTagType::kP},
         {kPDFStructureTypeHeading, PdfTagType::kH},
         {kPDFStructureTypeH1, PdfTagType::kH1},
         {kPDFStructureTypeH2, PdfTagType::kH2},
         {kPDFStructureTypeH3, PdfTagType::kH3},
         {kPDFStructureTypeH4, PdfTagType::kH4},
         {kPDFStructureTypeH5, PdfTagType::kH5},
         {kPDFStructureTypeH6, PdfTagType::kH6},
         {kPDFStructureTypeList, PdfTagType::kL},
         {kPDFStructureTypeListItemBody, PdfTagType::kLI},
         {kPDFStructureTypeListItemLabel, PdfTagType::kLbl},
         {kPDFStructureTypeListBody, PdfTagType::kLBody},
         {kPDFStructureTypeTable, PdfTagType::kTable},
         {kPDFStructureTypeTableRow, PdfTagType::kTR},
         {kPDFStructureTypeTableHeader, PdfTagType::kTH},
         {kPDFStructureTypeTableHead, PdfTagType::kTHead},
         {kPDFStructureTypeTableBody, PdfTagType::kTBody},
         {kPDFStructureTypeTableFoot, PdfTagType::kTFoot},
         {kPDFStructureTypeTableCell, PdfTagType::kTD},
         {kPDFStructureTypeSpan, PdfTagType::kSpan},
         {kPDFStructureTypeLink, PdfTagType::kLink},
         {kPDFStructureTypeFigure, PdfTagType::kFigure},
         {kPDFStructureTypeFormula, PdfTagType::kFormula},
         {kPDFStructureTypeForm, PdfTagType::kForm}});

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
