// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/metrics_handler.h"

#include <vector>

#include "base/metrics/histogram_functions.h"
#include "pdf/document_metadata.h"

namespace chrome_pdf {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PdfHasAttachment {
  kNo = 0,
  kYes = 1,
  kMaxValue = kYes,
};

}  // namespace

MetricsHandler::MetricsHandler() = default;

MetricsHandler::~MetricsHandler() = default;

void MetricsHandler::RecordDocumentMetrics(const DocumentMetadata& metadata) {
  base::UmaHistogramEnumeration("PDF.Version", metadata.version);
  base::UmaHistogramCustomCounts("PDF.PageCount", metadata.page_count, 1,
                                 1000000, 50);
  base::UmaHistogramEnumeration(
      "PDF.HasAttachment", metadata.has_attachments ? PdfHasAttachment::kYes
                                                    : PdfHasAttachment::kNo);
  base::UmaHistogramEnumeration("PDF.FormType", metadata.form_type);
}

void MetricsHandler::RecordAccessibilityIsDocTagged(bool is_tagged) {
  base::UmaHistogramBoolean("Accessibility.PDF.IsPDFTagged", is_tagged);
}

}  // namespace chrome_pdf
