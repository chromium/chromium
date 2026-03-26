// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/traces_internals/traces_internals_mojom_traits.h"

#include "base/notreached.h"

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

content::ReportUploadState
EnumTraits<ReportUploadState, content::ReportUploadState>::FromMojom(
    ReportUploadState input) {
  switch (input) {
    case ReportUploadState::kNotUploaded:
      return content::ReportUploadState::kNotUploaded;
    case ReportUploadState::kPending:
      return content::ReportUploadState::kPending;
    case ReportUploadState::kPending_UserRequested:
      return content::ReportUploadState::kPending_UserRequested;
    case ReportUploadState::kUploaded:
      return content::ReportUploadState::kUploaded;
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

content::SkipUploadReason
EnumTraits<SkipUploadReason, content::SkipUploadReason>::FromMojom(
    SkipUploadReason input) {
  switch (input) {
    case SkipUploadReason::kNoSkip:
      return content::SkipUploadReason::kNoSkip;
    case SkipUploadReason::kSizeLimitExceeded:
      return content::SkipUploadReason::kSizeLimitExceeded;
    case SkipUploadReason::kNotAnonymized:
      return content::SkipUploadReason::kNotAnonymized;
    case SkipUploadReason::kScenarioQuotaExceeded:
      return content::SkipUploadReason::kScenarioQuotaExceeded;
    case SkipUploadReason::kUploadTimedOut:
      return content::SkipUploadReason::kUploadTimedOut;
    case SkipUploadReason::kLocalScenario:
      return content::SkipUploadReason::kLocalScenario;
  }
  NOTREACHED();
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

content::TracingScenario::State
EnumTraits<TracingScenarioState, content::TracingScenario::State>::FromMojom(
    TracingScenarioState input) {
  switch (input) {
    case TracingScenarioState::kDisabled:
      return content::TracingScenario::State::kDisabled;
    case TracingScenarioState::kEnabled:
      return content::TracingScenario::State::kEnabled;
    case TracingScenarioState::kSetup:
      return content::TracingScenario::State::kSetup;
    case TracingScenarioState::kStarting:
      return content::TracingScenario::State::kStarting;
    case TracingScenarioState::kRecording:
      return content::TracingScenario::State::kRecording;
    case TracingScenarioState::kStopping:
      return content::TracingScenario::State::kStopping;
    case TracingScenarioState::kFinalizing:
      return content::TracingScenario::State::kFinalizing;
    case TracingScenarioState::kCloning:
      return content::TracingScenario::State::kCloning;
  }
  NOTREACHED();
}

}  // namespace mojo
