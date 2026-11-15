// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_migration_attempt_context.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_socket_config_step.h"

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
    case QuicMigrationAttemptCause::kWaitForNewNetworkPostNetworkDisconnected:
      return "WaitForNewNetworkPostNetworkDisconnected";
    case QuicMigrationAttemptCause::kWaitForNewNetworkPostWriteError:
      return "WaitForNewNetworkPostWriteError";
  }
}

namespace {

void RecordIneligibleWithPrefix(std::string_view prefix,
                                QuicMigrationAttemptCause cause,
                                QuicMigrationAttemptIneligibleReason reason) {
  base::UmaHistogramEnumeration(base::StrCat({prefix, ".Ineligible"}), reason);
  base::UmaHistogramEnumeration(
      base::StrCat({prefix, ".Ineligible.ByTrigger.",
                    QuicMigrationAttemptCauseToString(cause)}),
      reason);
}

void RecordSupersededWithPrefix(std::string_view prefix,
                                QuicMigrationAttemptCause cause,
                                QuicMigrationAttemptCause superseded_by_cause) {
  base::UmaHistogramEnumeration(base::StrCat({prefix, ".Superseded"}),
                                superseded_by_cause);
  base::UmaHistogramEnumeration(
      base::StrCat({prefix, ".Superseded.ByTrigger.",
                    QuicMigrationAttemptCauseToString(cause)}),
      superseded_by_cause);
}

}  // namespace

// static
void QuicMigrationAttemptContext::RecordIneligible(
    QuicMigrationAttemptCause cause,
    QuicMigrationAttemptIneligibleReason reason) {
  RecordIneligibleWithPrefix("Net.Quic.Migration.Attempt", cause, reason);
}

// static
void QuicMigrationAttemptContext::RecordSuperseded(
    QuicMigrationAttemptCause cause,
    QuicMigrationAttemptCause superseded_by_cause) {
  RecordSupersededWithPrefix("Net.Quic.Migration.Attempt", cause,
                             superseded_by_cause);
}

// static
void QuicMigrationAttemptContext::RecordWaitForNewNetworkDuration(
    QuicMigrationAttemptCause cause,
    base::TimeDelta duration) {
  CHECK(cause == QuicMigrationAttemptCause::
                     kWaitForNewNetworkPostNetworkDisconnected ||
        cause == QuicMigrationAttemptCause::kWaitForNewNetworkPostWriteError);
  base::UmaHistogramCustomTimes(
      "Net.Quic.Migration.Attempt.WaitForNewNetwork.Duration", duration,
      base::Milliseconds(1), base::Minutes(1), 50);
  base::UmaHistogramCustomTimes(
      base::StrCat({"Net.Quic.Migration.Attempt.WaitForNewNetwork.Duration."
                    "ByTrigger.",
                    QuicMigrationAttemptCauseToString(cause)}),
      duration, base::Milliseconds(1), base::Minutes(1), 50);
}

// static
std::unique_ptr<QuicMigrationAttemptContext>
QuicMigrationAttemptContext::CreateV6ToV4Fallback(
    std::unique_ptr<QuicMigrationAttemptContext> ipv6_context,
    const quic::QuicSocketAddress& fallback_ipv4_peer_address,
    std::unique_ptr<QuicChromiumPacketReader> reader,
    std::unique_ptr<QuicChromiumPacketWriter> writer) {
  CHECK(ipv6_context);
  CHECK(!ipv6_context->is_v6_to_v4_fallback_);
  CHECK_EQ(ipv6_context->outcome_, Outcome::kUnknown);
  CHECK(ipv6_context->target_peer_address_.host().IsIPv6());
  CHECK(fallback_ipv4_peer_address.host().IsIPv4());

  ipv6_context->outcome_ = Outcome::kConsumedByV6ToV4Fallback;
  return base::WrapUnique(new QuicMigrationAttemptContext(
      ipv6_context->cause_, ipv6_context->from_network_,
      ipv6_context->target_network_, fallback_ipv4_peer_address,
      std::move(reader), std::move(writer),
      std::move(ipv6_context->is_session_alive_), ipv6_context->is_google_host_,
      /*is_v6_to_v4_fallback=*/true));
}

QuicMigrationAttemptContext::QuicMigrationAttemptContext(
    QuicMigrationAttemptCause cause,
    handles::NetworkHandle from_network,
    handles::NetworkHandle target_network,
    const quic::QuicSocketAddress& target_peer_address,
    std::unique_ptr<QuicChromiumPacketReader> reader,
    std::unique_ptr<QuicChromiumPacketWriter> writer,
    base::RepeatingCallback<bool()> is_session_alive,
    bool is_google_host)
    : QuicMigrationAttemptContext(cause,
                                  from_network,
                                  target_network,
                                  target_peer_address,
                                  std::move(reader),
                                  std::move(writer),
                                  std::move(is_session_alive),
                                  is_google_host,
                                  /*is_v6_to_v4_fallback=*/false) {}

QuicMigrationAttemptContext::QuicMigrationAttemptContext(
    QuicMigrationAttemptCause cause,
    handles::NetworkHandle from_network,
    handles::NetworkHandle target_network,
    const quic::QuicSocketAddress& target_peer_address,
    std::unique_ptr<QuicChromiumPacketReader> reader,
    std::unique_ptr<QuicChromiumPacketWriter> writer,
    base::RepeatingCallback<bool()> is_session_alive,
    bool is_google_host,
    bool is_v6_to_v4_fallback)
    : cause_(cause),
      from_network_(from_network),
      target_network_(target_network),
      target_peer_address_(target_peer_address),
      reader_(std::move(reader)),
      writer_(std::move(writer)),
      is_session_alive_(std::move(is_session_alive)),
      is_google_host_(is_google_host),
      is_v6_to_v4_fallback_(is_v6_to_v4_fallback) {
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

  // An IPv6-to-IPv4 fallback does not start a new migration attempt. It
  // carries the same attempt over to a different server address after the
  // IPv6 target turned out to be unreachable. The fallback context that
  // consumed this one will record the outcome of that single attempt, so this
  // context must not record anything. Otherwise every fallback would be
  // counted twice (an IPv6 failure plus an IPv4 outcome), and the overall
  // Net.Quic.Migration.Attempt.* success rate would no longer show the
  // improvement brought by V6-to-V4 fallback. The fallback context also
  // records its outcome under Net.Quic.Migration.Attempt.V6ToV4Fallback.*,
  // which measures the success rate of the fallbacks themselves.
  if (outcome_ == Outcome::kConsumedByV6ToV4Fallback) {
    return;
  }

  if (outcome_ == Outcome::kUnknown && !is_session_alive_.Run()) {
    // This happens when the session is destroyed, for whatever reason, before
    // an actual outcome for the attempt has been decided.
    outcome_ = Outcome::kIneligible;
    outcome_details_ = QuicMigrationAttemptIneligibleReason::kSessionDestroyed;
  }

  RecordOutcomeHistograms("Net.Quic.Migration.Attempt");
  if (is_v6_to_v4_fallback_) {
    RecordOutcomeHistograms("Net.Quic.Migration.Attempt.V6ToV4Fallback");
  }
}

void QuicMigrationAttemptContext::RecordOutcomeHistograms(
    std::string_view prefix) const {
  std::string_view trigger_str = QuicMigrationAttemptCauseToString(cause_);

  switch (outcome_) {
    case Outcome::kSuccess:
      base::UmaHistogramBoolean(base::StrCat({prefix, ".Eligible"}), true);
      base::UmaHistogramBoolean(
          base::StrCat({prefix, ".Eligible.ByTrigger.", trigger_str}), true);
      break;
    case Outcome::kFailure: {
      base::UmaHistogramBoolean(base::StrCat({prefix, ".Eligible"}), false);
      base::UmaHistogramBoolean(
          base::StrCat({prefix, ".Eligible.ByTrigger.", trigger_str}), false);
      QuicMigrationAttemptFailureReason failure_reason;
      if (const auto* socket_details =
              std::get_if<SocketConfigFailureDetails>(&outcome_details_)) {
        // General failure reason logging.
        failure_reason = QuicMigrationAttemptFailureReason::kSocketConfigFailed;

        // Detailed, socket specific, failure logging.
        base::UmaHistogramEnumeration(
            base::StrCat({prefix, ".SocketConfigError.Step"}),
            socket_details->step);
        base::UmaHistogramEnumeration(
            base::StrCat(
                {prefix, ".SocketConfigError.Step.ByTrigger.", trigger_str}),
            socket_details->step);
        base::UmaHistogramSparse(
            base::StrCat({prefix, ".SocketConfigError.NetError"}),
            -socket_details->net_error);
        base::UmaHistogramSparse(
            base::StrCat({prefix, ".SocketConfigError.NetError.ByStep.",
                          QuicSocketConfigStepToString(socket_details->step)}),
            -socket_details->net_error);
        base::UmaHistogramSparse(
            base::StrCat({prefix, ".SocketConfigError.NetError.ByTrigger.",
                          trigger_str}),
            -socket_details->net_error);
      } else {
        failure_reason =
            std::get<QuicMigrationAttemptFailureReason>(outcome_details_);
      }

      base::UmaHistogramEnumeration(base::StrCat({prefix, ".FailureReason"}),
                                    failure_reason);
      base::UmaHistogramEnumeration(
          base::StrCat({prefix, ".FailureReason.ByTrigger.", trigger_str}),
          failure_reason);
      if (is_google_host_) {
        base::UmaHistogramEnumeration(
            base::StrCat({prefix, ".FailureReason.GoogleHost"}),
            failure_reason);
        base::UmaHistogramEnumeration(
            base::StrCat({prefix, ".FailureReason.ByTrigger.", trigger_str,
                          ".GoogleHost"}),
            failure_reason);
      }
      break;
    }
    case Outcome::kIneligible:
      RecordIneligibleWithPrefix(
          prefix, cause_,
          std::get<QuicMigrationAttemptIneligibleReason>(outcome_details_));
      break;
    case Outcome::kSuperseded:
      RecordSupersededWithPrefix(
          prefix, cause_,
          std::get<QuicMigrationAttemptCause>(outcome_details_));
      break;
    case Outcome::kUnknown:
      base::UmaHistogramBoolean(base::StrCat({prefix, ".UnclassifiedOutcome"}),
                                true);
      break;
    case Outcome::kConsumedByV6ToV4Fallback:
      NOTREACHED();
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
  CHECK_NE(reason, QuicMigrationAttemptFailureReason::kSocketConfigFailed);
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

void QuicMigrationAttemptContext::SetSocketConfigFailure(
    QuicSocketConfigStep step,
    int net_error) {
  CHECK_LT(net_error, 0);
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
  outcome_details_ = SocketConfigFailureDetails{step, net_error};
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
