// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_MOJOM_TRAITS_H_
#define CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_MOJOM_TRAITS_H_

#include "content/browser/tracing/trace_report/trace_report.mojom.h"
#include "content/browser/tracing/trace_report/trace_report_database.h"

namespace mojo {

namespace {

using ReportUploadState = trace_report::mojom::ReportUploadState;
using SkipUploadReason = trace_report::mojom::SkipUploadReason;

}  // namespace

template <>
struct EnumTraits<ReportUploadState, content::ReportUploadState> {
  static ReportUploadState ToMojom(content::ReportUploadState input);
  static bool FromMojom(ReportUploadState input,
                        content::ReportUploadState* output);
};

template <>
struct EnumTraits<SkipUploadReason, content::SkipUploadReason> {
  static SkipUploadReason ToMojom(content::SkipUploadReason input);
  static bool FromMojom(SkipUploadReason input,
                        content::SkipUploadReason* output);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_MOJOM_TRAITS_H_
