// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_METRICS_HANDLER_H_
#define PDF_METRICS_HANDLER_H_

#include <vector>

#include "pdf/document_attachment_info.h"

namespace chrome_pdf {

struct DocumentMetadata;

// Handles various UMA metrics. Note that action metrics are handled separately.
class MetricsHandler {
 public:
  MetricsHandler();
  MetricsHandler(const MetricsHandler& other) = delete;
  MetricsHandler& operator=(const MetricsHandler& other) = delete;
  ~MetricsHandler();

  void RecordAttachmentTypes(
      const std::vector<DocumentAttachmentInfo>& attachments);

  void RecordDocumentMetrics(const DocumentMetadata& metadata);
};

}  // namespace chrome_pdf

#endif  // PDF_METRICS_HANDLER_H_
