// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_METRICS_HANDLER_H_
#define PDF_METRICS_HANDLER_H_

namespace chrome_pdf {

struct DocumentMetadata;

// Handles various UMA metrics. Note that action metrics are handled separately.
class MetricsHandler {
 public:
  MetricsHandler();
  MetricsHandler(const MetricsHandler& other) = delete;
  MetricsHandler& operator=(const MetricsHandler& other) = delete;
  ~MetricsHandler();

  void RecordDocumentMetrics(const DocumentMetadata& metadata);
  void RecordAccessibilityIsDocTagged(bool is_tagged);
};

}  // namespace chrome_pdf

#endif  // PDF_METRICS_HANDLER_H_
