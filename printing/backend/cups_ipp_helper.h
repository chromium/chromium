// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Methods for parsing IPP Printer attributes.

#ifndef PRINTING_BACKEND_CUPS_IPP_HELPER_H_
#define PRINTING_BACKEND_CUPS_IPP_HELPER_H_

#include <cups/cups.h>

#include <memory>

#include "base/component_export.h"
#include "printing/backend/print_backend.h"

namespace printing {

class CupsPrinter;

struct COMPONENT_EXPORT(PRINT_BACKEND) IppDeleter {
  void operator()(ipp_t* ipp) const;
};
// Smart ptr wrapper for CUPS ipp_t
using ScopedIppPtr = std::unique_ptr<ipp_t, IppDeleter>;

// MediaColData is used to store info needed to create a media-col-database
// entry.  Variable width/height entries are optional, in which case the max
// width/height will be the same as the min width/height.
struct COMPONENT_EXPORT(PRINT_BACKEND) MediaColData {
  bool HasVariableWidth() const { return min_width != max_width; }
  bool HasVariableHeight() const { return min_height != max_height; }

  int min_width;
  int min_height;
  int max_width;
  int max_height;
  int bottom_margin;
  int left_margin;
  int right_margin;
  int top_margin;
};

// Returns the default paper setting for `printer`.
COMPONENT_EXPORT(PRINT_BACKEND)
PrinterSemanticCapsAndDefaults::Paper DefaultPaper(const CupsPrinter& printer);

// Populates the `printer_info` object with attributes retrieved using IPP from
// `printer`.
COMPONENT_EXPORT(PRINT_BACKEND)
void CapsAndDefaultsFromPrinter(const CupsPrinter& printer,
                                PrinterSemanticCapsAndDefaults* printer_info);

// Gets the printer margins for the provided paper size.
COMPONENT_EXPORT(PRINT_BACKEND)
gfx::Rect GetPrintableAreaForSize(const CupsPrinter& printer,
                                  const gfx::Size& size_um);

// Wraps `ipp` in unique_ptr with appropriate deleter
COMPONENT_EXPORT(PRINT_BACKEND) ScopedIppPtr WrapIpp(ipp_t* ipp);

// Returns a MediaColData object by extracting necessary fields from `db_entry`.
COMPONENT_EXPORT(PRINT_BACKEND)
std::optional<MediaColData> ExtractMediaColData(ipp_t* db_entry);

// Creates a new media-col-database entry from `data`.  The caller is
// responsible for the returned memory.
COMPONENT_EXPORT(PRINT_BACKEND)
ScopedIppPtr NewMediaCollection(const MediaColData& data);

// Variable height entries are only allowed with a fixed width.  If an entry has
// a variable height and variable width, that entry is filtered out.  However, a
// new entry is created with that variable height for EACH fixed width (among
// all the entries).
COMPONENT_EXPORT(PRINT_BACKEND)
void FilterMediaColSizes(ScopedIppPtr& attributes);

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_IPP_HELPER_H_
