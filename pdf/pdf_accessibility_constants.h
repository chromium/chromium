// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ACCESSIBILITY_CONSTANTS_H_
#define PDF_PDF_ACCESSIBILITY_CONSTANTS_H_

namespace chrome_pdf {

// PDF structure type constants from PDF 32000-1:2008.

// Table 333 in PDF 32000-1:2008 spec, section 14.8.4.2
inline constexpr char kPDFStructureTypeDocument[] = "Document";
inline constexpr char kPDFStructureTypePart[] = "Part";
inline constexpr char kPDFStructureTypeArticle[] = "Art";
inline constexpr char kPDFStructureTypeSect[] = "Sect";
inline constexpr char kPDFStructureTypeDiv[] = "Div";
inline constexpr char kPDFStructureTypeBlockQuote[] = "BlockQuote";
inline constexpr char kPDFStructureTypeCaption[] = "Caption";
inline constexpr char kPDFStructureTypeTOC[] = "TOC";
inline constexpr char kPDFStructureTypeTOCI[] = "TOCI";
inline constexpr char kPDFStructureTypeIndex[] = "Index";
inline constexpr char kPDFStructureTypeNonStruct[] = "NonStruct";

// Table 335 in PDF 32000-1:2008 spec, section 14.8.4.3.2
inline constexpr char kPDFStructureTypeHeading[] = "H";
inline constexpr char kPDFStructureTypeH1[] = "H1";
inline constexpr char kPDFStructureTypeH2[] = "H2";
inline constexpr char kPDFStructureTypeH3[] = "H3";
inline constexpr char kPDFStructureTypeH4[] = "H4";
inline constexpr char kPDFStructureTypeH5[] = "H5";
inline constexpr char kPDFStructureTypeH6[] = "H6";
inline constexpr char kPDFStructureTypeParagraph[] = "P";

// Table 336 in PDF 32000-1:2008 spec, section 14.8.4.3.3
inline constexpr char kPDFStructureTypeList[] = "L";
inline constexpr char kPDFStructureTypeListItemBody[] = "LI";
inline constexpr char kPDFStructureTypeListItemLabel[] = "Lbl";
inline constexpr char kPDFStructureTypeListBody[] = "LBody";

// Table 337 in PDF 32000-1:2008 spec, section 14.8.4.3.4
inline constexpr char kPDFStructureTypeTable[] = "Table";
inline constexpr char kPDFStructureTypeTableRow[] = "TR";
inline constexpr char kPDFStructureTypeTableHeader[] = "TH";
inline constexpr char kPDFStructureTypeTableCell[] = "TD";
inline constexpr char kPDFStructureTypeTableHead[] = "THead";
inline constexpr char kPDFStructureTypeTableBody[] = "TBody";
inline constexpr char kPDFStructureTypeTableFoot[] = "TFoot";

// Table 338 in PDF 32000-1:2008 spec, section 14.8.4.4.1
inline constexpr char kPDFStructureTypeSpan[] = "Span";
inline constexpr char kPDFStructureTypeLink[] = "Link";
inline constexpr char kPDFStructureTypeCode[] = "Code";

// Table 340 in PDF 32000-1:2008 spec, section 14.8.5
inline constexpr char kPDFStructureTypeFigure[] = "Figure";
inline constexpr char kPDFStructureTypeFormula[] = "Formula";
inline constexpr char kPDFStructureTypeForm[] = "Form";

// Standard attribute owners from Table 376 PDF 32000-2:2020 spec,
// section 14.8.5.2

// Standard attribute owners from PDF 32000-1:2008 spec, section 14.8.5.2
// (Attribute owners are kind of like "categories" for structure node
// attributes.)
inline constexpr char kPDFTableAttributeOwner[] = "Table";

// Table Attributes from PDF 32000-1:2008 spec, section 14.8.5.7
inline constexpr char kPDFTableCellColSpanAttribute[] = "ColSpan";
inline constexpr char kPDFTableCellHeadersAttribute[] = "Headers";
inline constexpr char kPDFTableCellRowSpanAttribute[] = "RowSpan";
inline constexpr char kPDFTableHeaderScopeAttribute[] = "Scope";
inline constexpr char kPDFTableHeaderScopeColumn[] = "Column";
inline constexpr char kPDFTableHeaderScopeRow[] = "Row";

// PDF structure type constants from PDF 32000-2:2020 spec.

// Table 365 in PDF 32000-2:2020 spec, section 14.8.4.4
inline constexpr char kPDFStructureTypeAside[] = "Aside";

// Table 368 in PDF 32000-2:2020 spec, section 14.8.4.7.2
inline constexpr char kPDFStructureTypeEmphasis[] = "Em";
inline constexpr char kPDFStructureTypeStrong[] = "Strong";

// Please keep the below enum as close as possible to the list defined in the
// PDF Specification, ISO 32000-1:2008, section 14.8.4.
enum class PdfTagType {
  kNone,  // Not present.
  kDocument,
  kPart,
  kArt,
  kSect,
  kDiv,
  kBlockQuote,
  kCaption,
  kTOC,   // Table of contents.
  kTOCI,  // Table of contents entry.
  kIndex,
  kP,  // Paragraph.
  kH,  // Heading.
  kH1,
  kH2,
  kH3,
  kH4,
  kH5,
  kH6,
  kL,    // List.
  kLI,   // List item.
  kLbl,  // List marker.
  kLBody,
  kTable,
  kTR,
  kTH,
  kTHead,  // Table row group header.
  kTBody,
  kTFoot,
  kTD,
  kSpan,
  kLink,
  kFigure,
  kFormula,
  kForm,
  kUnknown,  // Unrecognized.
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_ACCESSIBILITY_CONSTANTS_H_
