// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_report/trace_report_mojom_traits.h"

namespace mojo {

ReportUploadState
EnumTraits<ReportUploadState, content::ReportUploadState>::ToMojom(
    content::ReportUploadState input) {
  switch (input) {
    case content::ReportUploadState::kNotUploaded:
      return ReportUploadState::kNotUploaded;
    case content::ReportUploadState::kPending:
      return ReportUploadState::kPending;
    case content::ReportUploadState::kPending_UserRequested:
      return ReportUploadState::kPending_UserRequested;
    case content::ReportUploadState::kUploaded:
      return ReportUploadState::kUploaded;
  }
}

bool EnumTraits<ReportUploadState, content::ReportUploadState>::FromMojom(
    ReportUploadState input,
    content::ReportUploadState* output) {
  switch (input) {
    case ReportUploadState::kNotUploaded:
      *output = content::ReportUploadState::kNotUploaded;
      return true;
    case ReportUploadState::kPending:
      *output = content::ReportUploadState::kPending;
      return true;
    case ReportUploadState::kPending_UserRequested:
      *output = content::ReportUploadState::kPending_UserRequested;
      return true;
    case ReportUploadState::kUploaded:
      *output = content::ReportUploadState::kUploaded;
      return true;
  }
}

SkipUploadReason
EnumTraits<SkipUploadReason, content::SkipUploadReason>::ToMojom(
    content::SkipUploadReason input) {
  switch (input) {
    case content::SkipUploadReason::kNoSkip:
      return SkipUploadReason::kNoSkip;
    case content::SkipUploadReason::kSizeLimitExceeded:
      return SkipUploadReason::kSizeLimitExceeded;
    case content::SkipUploadReason::kNotAnonymized:
      return SkipUploadReason::kNotAnonymized;
    case content::SkipUploadReason::kScenarioQuotaExceeded:
      return SkipUploadReason::kScenarioQuotaExceeded;
    case content::SkipUploadReason::kUploadTimedOut:
      return SkipUploadReason::kUploadTimedOut;
  }
}

bool EnumTraits<SkipUploadReason, content::SkipUploadReason>::FromMojom(
    SkipUploadReason input,
    content::SkipUploadReason* output) {
  switch (input) {
    case SkipUploadReason::kNoSkip:
      *output = content::SkipUploadReason::kNoSkip;
      return true;
    case SkipUploadReason::kSizeLimitExceeded:
      *output = content::SkipUploadReason::kSizeLimitExceeded;
      return true;
    case SkipUploadReason::kNotAnonymized:
      *output = content::SkipUploadReason::kNotAnonymized;
      return true;
    case SkipUploadReason::kScenarioQuotaExceeded:
      *output = content::SkipUploadReason::kScenarioQuotaExceeded;
      return true;
    case SkipUploadReason::kUploadTimedOut:
      *output = content::SkipUploadReason::kUploadTimedOut;
      return true;
  }
  return false;
}

}  // namespace mojo
