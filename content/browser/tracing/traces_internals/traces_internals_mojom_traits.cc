// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/traces_internals/traces_internals_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

ReportUploadState
EnumTraits<ReportUploadState, tracing::ReportUploadState>::ToMojom(
    tracing::ReportUploadState input) {
  switch (input) {
    case tracing::ReportUploadState::kNotUploaded:
      return ReportUploadState::kNotUploaded;
    case tracing::ReportUploadState::kPending:
      return ReportUploadState::kPending;
    case tracing::ReportUploadState::kPending_UserRequested:
      return ReportUploadState::kPending_UserRequested;
    case tracing::ReportUploadState::kUploaded:
      return ReportUploadState::kUploaded;
  }
  NOTREACHED();
}

tracing::ReportUploadState
EnumTraits<ReportUploadState, tracing::ReportUploadState>::FromMojom(
    ReportUploadState input) {
  switch (input) {
    case traces_internals::mojom::ReportUploadState::kNotUploaded:
      return tracing::ReportUploadState::kNotUploaded;
    case traces_internals::mojom::ReportUploadState::kPending:
      return tracing::ReportUploadState::kPending;
    case traces_internals::mojom::ReportUploadState::kPending_UserRequested:
      return tracing::ReportUploadState::kPending_UserRequested;
    case traces_internals::mojom::ReportUploadState::kUploaded:
      return tracing::ReportUploadState::kUploaded;
  }
}

SkipUploadReason
EnumTraits<SkipUploadReason, tracing::SkipUploadReason>::ToMojom(
    tracing::SkipUploadReason input) {
  switch (input) {
    case tracing::SkipUploadReason::kNoSkip:
      return SkipUploadReason::kNoSkip;
    case tracing::SkipUploadReason::kSizeLimitExceeded:
      return SkipUploadReason::kSizeLimitExceeded;
    case tracing::SkipUploadReason::kNotAnonymized:
      return SkipUploadReason::kNotAnonymized;
    case tracing::SkipUploadReason::kScenarioQuotaExceeded:
      return SkipUploadReason::kScenarioQuotaExceeded;
    case tracing::SkipUploadReason::kUploadTimedOut:
      return SkipUploadReason::kUploadTimedOut;
    case tracing::SkipUploadReason::kLocalScenario:
      return SkipUploadReason::kLocalScenario;
  }
  NOTREACHED();
}

tracing::SkipUploadReason
EnumTraits<SkipUploadReason, tracing::SkipUploadReason>::FromMojom(
    SkipUploadReason input) {
  switch (input) {
    case traces_internals::mojom::SkipUploadReason::kNoSkip:
      return tracing::SkipUploadReason::kNoSkip;
    case traces_internals::mojom::SkipUploadReason::kSizeLimitExceeded:
      return tracing::SkipUploadReason::kSizeLimitExceeded;
    case traces_internals::mojom::SkipUploadReason::kNotAnonymized:
      return tracing::SkipUploadReason::kNotAnonymized;
    case traces_internals::mojom::SkipUploadReason::kScenarioQuotaExceeded:
      return tracing::SkipUploadReason::kScenarioQuotaExceeded;
    case traces_internals::mojom::SkipUploadReason::kUploadTimedOut:
      return tracing::SkipUploadReason::kUploadTimedOut;
    case traces_internals::mojom::SkipUploadReason::kLocalScenario:
      return tracing::SkipUploadReason::kLocalScenario;
  }
  NOTREACHED();
}

TracingScenarioState
EnumTraits<TracingScenarioState, tracing::TracingScenario::State>::ToMojom(
    tracing::TracingScenario::State input) {
  switch (input) {
    case tracing::TracingScenario::State::kDisabled:
      return TracingScenarioState::kDisabled;
    case tracing::TracingScenario::State::kEnabled:
      return TracingScenarioState::kEnabled;
    case tracing::TracingScenario::State::kSetup:
      return TracingScenarioState::kSetup;
    case tracing::TracingScenario::State::kStarting:
      return TracingScenarioState::kStarting;
    case tracing::TracingScenario::State::kRecording:
      return TracingScenarioState::kRecording;
    case tracing::TracingScenario::State::kStopping:
      return TracingScenarioState::kStopping;
    case tracing::TracingScenario::State::kFinalizing:
      return TracingScenarioState::kFinalizing;
    case tracing::TracingScenario::State::kCloning:
      return TracingScenarioState::kCloning;
  }
  NOTREACHED();
}

tracing::TracingScenario::State
EnumTraits<TracingScenarioState, tracing::TracingScenario::State>::FromMojom(
    TracingScenarioState input) {
  switch (input) {
    case traces_internals::mojom::TracingScenarioState::kDisabled:
      return tracing::TracingScenario::State::kDisabled;
    case traces_internals::mojom::TracingScenarioState::kEnabled:
      return tracing::TracingScenario::State::kEnabled;
    case traces_internals::mojom::TracingScenarioState::kSetup:
      return tracing::TracingScenario::State::kSetup;
    case traces_internals::mojom::TracingScenarioState::kStarting:
      return tracing::TracingScenario::State::kStarting;
    case traces_internals::mojom::TracingScenarioState::kRecording:
      return tracing::TracingScenario::State::kRecording;
    case traces_internals::mojom::TracingScenarioState::kStopping:
      return tracing::TracingScenario::State::kStopping;
    case traces_internals::mojom::TracingScenarioState::kFinalizing:
      return tracing::TracingScenario::State::kFinalizing;
    case traces_internals::mojom::TracingScenarioState::kCloning:
      return tracing::TracingScenario::State::kCloning;
  }
  NOTREACHED();
}

}  // namespace mojo
