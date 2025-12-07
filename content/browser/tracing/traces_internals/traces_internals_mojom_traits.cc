// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/traces_internals/traces_internals_mojom_traits.h"

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
  NOTREACHED();
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
    case content::SkipUploadReason::kLocalScenario:
      return SkipUploadReason::kLocalScenario;
  }
  NOTREACHED();
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
    case SkipUploadReason::kLocalScenario:
      *output = content::SkipUploadReason::kLocalScenario;
      return true;
  }
  return false;
}

TracingScenarioState
EnumTraits<TracingScenarioState, content::TracingScenario::State>::ToMojom(
    content::TracingScenario::State input) {
  switch (input) {
    case content::TracingScenario::State::kDisabled:
      return TracingScenarioState::kDisabled;
    case content::TracingScenario::State::kEnabled:
      return TracingScenarioState::kEnabled;
    case content::TracingScenario::State::kSetup:
      return TracingScenarioState::kSetup;
    case content::TracingScenario::State::kStarting:
      return TracingScenarioState::kStarting;
    case content::TracingScenario::State::kRecording:
      return TracingScenarioState::kRecording;
    case content::TracingScenario::State::kStopping:
      return TracingScenarioState::kStopping;
    case content::TracingScenario::State::kFinalizing:
      return TracingScenarioState::kFinalizing;
    case content::TracingScenario::State::kCloning:
      return TracingScenarioState::kCloning;
  }
  NOTREACHED();
}

bool EnumTraits<TracingScenarioState, content::TracingScenario::State>::
    FromMojom(TracingScenarioState input,
              content::TracingScenario::State* output) {
  switch (input) {
    case TracingScenarioState::kDisabled:
      *output = content::TracingScenario::State::kDisabled;
      return true;
    case TracingScenarioState::kEnabled:
      *output = content::TracingScenario::State::kEnabled;
      return true;
    case TracingScenarioState::kSetup:
      *output = content::TracingScenario::State::kSetup;
      return true;
    case TracingScenarioState::kStarting:
      *output = content::TracingScenario::State::kStarting;
      return true;
    case TracingScenarioState::kRecording:
      *output = content::TracingScenario::State::kRecording;
      return true;
    case TracingScenarioState::kStopping:
      *output = content::TracingScenario::State::kStopping;
      return true;
    case TracingScenarioState::kFinalizing:
      *output = content::TracingScenario::State::kFinalizing;
      return true;
    case TracingScenarioState::kCloning:
      *output = content::TracingScenario::State::kCloning;
      return true;
  }
  return false;
}

}  // namespace mojo
