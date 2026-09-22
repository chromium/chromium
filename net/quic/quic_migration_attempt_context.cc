// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_migration_attempt_context.h"

#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"

namespace net {

std::string_view QuicMigrationAttemptCauseToString(
    QuicMigrationAttemptCause cause) {
  switch (cause) {
    case QuicMigrationAttemptCause::kUnknown:
      return "Unknown";
    case QuicMigrationAttemptCause::kOnNetworkDisconnected:
      return "OnNetworkDisconnected";
    case QuicMigrationAttemptCause::kOnWriteError:
      return "OnWriteError";
    case QuicMigrationAttemptCause::kOnNetworkMadeDefault:
      return "OnNetworkMadeDefault";
    case QuicMigrationAttemptCause::kOnMigrateBackToDefaultNetwork:
      return "OnMigrateBackToDefaultNetwork";
    case QuicMigrationAttemptCause::kChangeNetworkOnPathDegrading:
      return "ChangeNetworkOnPathDegrading";
    case QuicMigrationAttemptCause::kChangePortOnPathDegrading:
      return "ChangePortOnPathDegrading";
    case QuicMigrationAttemptCause::kNewNetworkConnectedPostPathDegrading:
      return "NewNetworkConnectedPostPathDegrading";
    case QuicMigrationAttemptCause::kOnServerPreferredAddressAvailable:
      return "OnServerPreferredAddressAvailable";
    case QuicMigrationAttemptCause::kMultiPortPath:
      return "MultiPortPath";
  }
}

// static
void QuicMigrationAttemptContext::RecordIneligible(
    QuicMigrationAttemptCause cause,
    QuicMigrationAttemptIneligibleReason reason) {
  base::UmaHistogramEnumeration("Net.Quic.Migration.Attempt.Ineligible",
                                reason);
  base::UmaHistogramEnumeration(
      base::StrCat({"Net.Quic.Migration.Attempt.Ineligible.ByTrigger.",
                    QuicMigrationAttemptCauseToString(cause)}),
      reason);
}

QuicMigrationAttemptContext::QuicMigrationAttemptContext(
    QuicMigrationAttemptCause cause,
    handles::NetworkHandle from_network,
    handles::NetworkHandle target_network,
    const quic::QuicSocketAddress& target_peer_address,
    std::unique_ptr<QuicChromiumPacketReader> reader,
    std::unique_ptr<QuicChromiumPacketWriter> writer,
    base::RepeatingCallback<bool()> is_session_alive)
    : cause_(cause),
      from_network_(from_network),
      target_network_(target_network),
      target_peer_address_(target_peer_address),
      reader_(std::move(reader)),
      writer_(std::move(writer)),
      is_session_alive_(std::move(is_session_alive)) {
  CHECK(reader_);
  CHECK(writer_);
  CHECK(is_session_alive_);
}

QuicMigrationAttemptContext::~QuicMigrationAttemptContext() {
  // TODO(crbug.com/557126867): Handle logging of multi-port path migrations via
  // QuicMigrationAttemptContext, like other migration attempts causes.
  if (cause_ == QuicMigrationAttemptCause::kMultiPortPath) {
    return;
  }

  if (outcome_ == Outcome::kUnknown && !is_session_alive_.Run()) {
    // This happens when the session is destroyed, for whatever reason, before
    // an actual outcome for the attempt has been decided.
    outcome_ = Outcome::kIneligible;
    outcome_details_ = QuicMigrationAttemptIneligibleReason::kSessionDestroyed;
  }

  std::string_view trigger_str = QuicMigrationAttemptCauseToString(cause_);

  switch (outcome_) {
    case Outcome::kSuccess:
      base::UmaHistogramBoolean("Net.Quic.Migration.Attempt.Eligible", true);
      base::UmaHistogramBoolean(
          base::StrCat(
              {"Net.Quic.Migration.Attempt.Eligible.ByTrigger.", trigger_str}),
          true);
      break;
    case Outcome::kFailure: {
      base::UmaHistogramBoolean("Net.Quic.Migration.Attempt.Eligible", false);
      base::UmaHistogramBoolean(
          base::StrCat(
              {"Net.Quic.Migration.Attempt.Eligible.ByTrigger.", trigger_str}),
          false);
      auto failure_reason =
          std::get<QuicMigrationAttemptFailureReason>(outcome_details_);
      base::UmaHistogramEnumeration("Net.Quic.Migration.Attempt.FailureReason",
                                    failure_reason);
      base::UmaHistogramEnumeration(
          base::StrCat({"Net.Quic.Migration.Attempt.FailureReason.ByTrigger.",
                        trigger_str}),
          failure_reason);
      break;
    }
    case Outcome::kIneligible:
      RecordIneligible(cause_, std::get<QuicMigrationAttemptIneligibleReason>(
                                   outcome_details_));
      break;
    case Outcome::kSuperseded:
      base::UmaHistogramEnumeration(
          "Net.Quic.Migration.Attempt.Superseded",
          std::get<QuicMigrationAttemptCause>(outcome_details_));
      break;
    case Outcome::kUnknown:
      base::UmaHistogramBoolean(
          "Net.Quic.Migration.Attempt.UnclassifiedOutcome", true);
      break;
  }
}

void QuicMigrationAttemptContext::SetSuccess() {
  // Since the connection migration code predates this class, err on the safe
  // side and record spurious outcomes for now.
  // TODO(crbug.com/557126867): Replace this UMA with a CHECK once we have
  // confirmed that outcomes are never set more than once.
  if (outcome_ != Outcome::kUnknown) {
    base::UmaHistogramBoolean("Net.Quic.Migration.Attempt.SpuriousOutcome",
                              true);
    return;
  }
  outcome_ = Outcome::kSuccess;
}

void QuicMigrationAttemptContext::SetFailure(
    QuicMigrationAttemptFailureReason reason) {
  // Since the connection migration code predates this class, err on the safe
  // side and record spurious outcomes for now.
  // TODO(crbug.com/557126867): Replace this UMA with a CHECK once we have
  // confirmed that outcomes are never set more than once.
  if (outcome_ != Outcome::kUnknown) {
    base::UmaHistogramBoolean("Net.Quic.Migration.Attempt.SpuriousOutcome",
                              true);
    return;
  }
  outcome_ = Outcome::kFailure;
  outcome_details_ = reason;
}

void QuicMigrationAttemptContext::SetIneligible(
    QuicMigrationAttemptIneligibleReason reason) {
  // Since the connection migration code predates this class, err on the safe
  // side and record spurious outcomes for now.
  // TODO(crbug.com/557126867): Replace this UMA with a CHECK once we have
  // confirmed that outcomes are never set more than once.
  if (outcome_ != Outcome::kUnknown) {
    base::UmaHistogramBoolean("Net.Quic.Migration.Attempt.SpuriousOutcome",
                              true);
    return;
  }
  outcome_ = Outcome::kIneligible;
  outcome_details_ = reason;
}

void QuicMigrationAttemptContext::SetSuperseded(
    QuicMigrationAttemptCause cause) {
  // Since the connection migration code predates this class, err on the safe
  // side and record spurious outcomes for now.
  // TODO(crbug.com/557126867): Replace this UMA with a CHECK once we have
  // confirmed that outcomes are never set more than once.
  if (outcome_ != Outcome::kUnknown) {
    base::UmaHistogramBoolean("Net.Quic.Migration.Attempt.SpuriousOutcome",
                              true);
    return;
  }
  outcome_ = Outcome::kSuperseded;
  outcome_details_ = cause;
}

}  // namespace net
