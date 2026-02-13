// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_accessibility_constants_helper.h"

#include "base/containers/fixed_flat_map.h"

namespace chrome_pdf {

namespace {

// Please keep the entries in the same order as the `PdfTagType` enum.
constexpr auto kAXRoleFromPdfTagTypeMap =
    base::MakeFixedFlatMap<PdfTagType, ax::mojom::Role>({
        {PdfTagType::kDocument, ax::mojom::Role::kDocument},
        {PdfTagType::kPart, ax::mojom::Role::kDocPart},
        {PdfTagType::kArt, ax::mojom::Role::kArticle},
        {PdfTagType::kSect, ax::mojom::Role::kSection},
        {PdfTagType::kDiv, ax::mojom::Role::kGenericContainer},
        {PdfTagType::kBlockQuote, ax::mojom::Role::kBlockquote},
        {PdfTagType::kCaption, ax::mojom::Role::kCaption},
        {PdfTagType::kTOC, ax::mojom::Role::kDocToc},
        {PdfTagType::kTOCI, ax::mojom::Role::kListItem},
        {PdfTagType::kIndex, ax::mojom::Role::kDocIndex},
        {PdfTagType::kNonStruct, ax::mojom::Role::kGenericContainer},
        {PdfTagType::kP, ax::mojom::Role::kParagraph},
        // All heading types map to kHeading role.
        {PdfTagType::kH, ax::mojom::Role::kHeading},
        {PdfTagType::kH1, ax::mojom::Role::kHeading},
        {PdfTagType::kH2, ax::mojom::Role::kHeading},
        {PdfTagType::kH3, ax::mojom::Role::kHeading},
        {PdfTagType::kH4, ax::mojom::Role::kHeading},
        {PdfTagType::kH5, ax::mojom::Role::kHeading},
        {PdfTagType::kH6, ax::mojom::Role::kHeading},
        {PdfTagType::kL, ax::mojom::Role::kList},
        {PdfTagType::kLI, ax::mojom::Role::kListItem},
        {PdfTagType::kLbl, ax::mojom::Role::kListMarker},
        // LBody is presentational, maps to kNone.
        {PdfTagType::kLBody, ax::mojom::Role::kNone},
        {PdfTagType::kTable, ax::mojom::Role::kTable},
        {PdfTagType::kTR, ax::mojom::Role::kRow},
        {PdfTagType::kTH, ax::mojom::Role::kRowHeader},
        {PdfTagType::kTHead, ax::mojom::Role::kRowGroup},
        {PdfTagType::kTBody, ax::mojom::Role::kRowGroup},
        {PdfTagType::kTFoot, ax::mojom::Role::kRowGroup},
        {PdfTagType::kTD, ax::mojom::Role::kCell},
        {PdfTagType::kSpan, ax::mojom::Role::kStaticText},
        {PdfTagType::kLink, ax::mojom::Role::kLink},
        {PdfTagType::kRuby, ax::mojom::Role::kRuby},
        {PdfTagType::kRT, ax::mojom::Role::kRubyAnnotation},
        {PdfTagType::kFigure, ax::mojom::Role::kFigure},
        {PdfTagType::kFormula, ax::mojom::Role::kMath},
        {PdfTagType::kForm, ax::mojom::Role::kForm},
    });

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
         {kPDFStructureTypeNonStruct, PdfTagType::kNonStruct},
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
         {kPDFStructureTypeRuby, PdfTagType::kRuby},
         {kPDFStructureTypeRubyText, PdfTagType::kRT},
         {kPDFStructureTypeFigure, PdfTagType::kFigure},
         {kPDFStructureTypeFormula, PdfTagType::kFormula},
         {kPDFStructureTypeForm, PdfTagType::kForm}});

static_assert(kStringToPdfTagTypeMap.size() ==
              static_cast<size_t>(PdfTagType::kUnknown));

}  // namespace

ax::mojom::Role AXRoleFromPdfTagType(PdfTagType tag_type) {
  if (auto iter = kAXRoleFromPdfTagTypeMap.find(tag_type);
      iter != kAXRoleFromPdfTagTypeMap.end()) {
    return iter->second;
  }
  return ax::mojom::Role::kGenericContainer;
}

const base::fixed_flat_map<std::string_view, PdfTagType, 38>&
GetPdfTagTypeMap() {
  return kStringToPdfTagTypeMap;
}

PdfTagType PdfTagTypeFromString(const std::string& tag_type) {
  if (auto iter = kStringToPdfTagTypeMap.find(tag_type);
      iter != kStringToPdfTagTypeMap.end()) {
    return iter->second;
  }
  return PdfTagType::kUnknown;
}

}  // namespace chrome_pdf
