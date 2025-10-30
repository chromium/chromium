// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ACCESSIBILITY_CONSTANTS_H_
#define PDF_PDF_ACCESSIBILITY_CONSTANTS_H_

namespace chrome_pdf {

// Please keep the below enum as close as possible to the list defined in the
// PDF Specification, ISO 32000-1:2008, table 333.
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
