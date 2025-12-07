// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_serving_handle.h"

#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/browser/preloading/prefetch/prefetch_single_redirect_hop.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/public/browser/preloading.h"
#include "url/gurl.h"

namespace content {
namespace {

void RecordCookieCopyTimes(
    const base::TimeTicks& cookie_copy_start_time,
    const base::TimeTicks& cookie_read_end_and_write_start_time,
    const base::TimeTicks& cookie_copy_end_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieReadTime",
      cookie_read_end_and_write_start_time - cookie_copy_start_time,
      base::TimeDelta(), base::Seconds(5), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieWriteTime",
      cookie_copy_end_time - cookie_read_end_and_write_start_time,
      base::TimeDelta(), base::Seconds(5), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyTime",
      cookie_copy_end_time - cookie_copy_start_time, base::TimeDelta(),
      base::Seconds(5), 50);
}

}  // namespace

PrefetchServingHandle::PrefetchServingHandle()
    : PrefetchServingHandle(nullptr, 0) {}

PrefetchServingHandle::PrefetchServingHandle(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    size_t index_redirect_chain_to_serve)
    : prefetch_container_(std::move(prefetch_container)),
      index_redirect_chain_to_serve_(index_redirect_chain_to_serve) {}

PrefetchServingHandle::PrefetchServingHandle(PrefetchServingHandle&&) = default;
PrefetchServingHandle& PrefetchServingHandle::operator=(
    PrefetchServingHandle&&) = default;
PrefetchServingHandle::~PrefetchServingHandle() = default;

PrefetchServingHandle PrefetchServingHandle::Clone() const {
  return PrefetchServingHandle(prefetch_container_,
                               index_redirect_chain_to_serve_);
}

const std::vector<std::unique_ptr<PrefetchSingleRedirectHop>>&
PrefetchServingHandle::redirect_chain() const {
  CHECK(GetPrefetchContainer());
  return GetPrefetchContainer()->redirect_chain(
      base::PassKey<PrefetchServingHandle>());
}

PrefetchNetworkContext* PrefetchServingHandle::GetCurrentNetworkContextToServe()
    const {
  const PrefetchSingleRedirectHop& this_prefetch =
      GetCurrentSingleRedirectHopToServe();
  return GetPrefetchContainer()->GetNetworkContext(
      this_prefetch.is_isolated_network_context_required_);
}

bool PrefetchServingHandle::HaveDefaultContextCookiesChanged() const {
  const PrefetchSingleRedirectHop& this_prefetch =
      GetCurrentSingleRedirectHopToServe();
  if (this_prefetch.cookie_listener_) {
    return this_prefetch.cookie_listener_->HaveCookiesChanged();
  }
  return false;
}

bool PrefetchServingHandle::HasIsolatedCookieCopyStarted() const {
  switch (GetCurrentSingleRedirectHopToServe().cookie_copy_status_) {
    case PrefetchSingleRedirectHop::CookieCopyStatus::kNotStarted:
      return false;
    case PrefetchSingleRedirectHop::CookieCopyStatus::kInProgress:
    case PrefetchSingleRedirectHop::CookieCopyStatus::kCompleted:
      return true;
  }
}

bool PrefetchServingHandle::IsIsolatedCookieCopyInProgress() const {
  switch (GetCurrentSingleRedirectHopToServe().cookie_copy_status_) {
    case PrefetchSingleRedirectHop::CookieCopyStatus::kNotStarted:
    case PrefetchSingleRedirectHop::CookieCopyStatus::kCompleted:
      return false;
    case PrefetchSingleRedirectHop::CookieCopyStatus::kInProgress:
      return true;
  }
}

void PrefetchServingHandle::OnIsolatedCookieCopyStart() const {
  DCHECK(!IsIsolatedCookieCopyInProgress());

  // We should temporarily ignore the cookie monitoring by
  // `PrefetchCookieListener` during the isolated cookie is written to the
  // default network context.
  // `PrefetchCookieListener` should monitor whether the cookie is changed from
  // what we stored in isolated network context when prefetching so that we can
  // avoid serving the stale prefetched content. Currently
  // `PrefetchCookieListener` will also catch isolated cookie copy as a cookie
  // change. To handle this event as a false positive (as the cookie isn't
  // changed from what we stored on prefetching), we can pause the lisner during
  // copying, keeping the prefetch servable.
  GetPrefetchContainer()->PauseAllCookieListeners();

  GetCurrentSingleRedirectHopToServe().cookie_copy_status_ =
      PrefetchSingleRedirectHop::CookieCopyStatus::kInProgress;

  GetCurrentSingleRedirectHopToServe().cookie_copy_start_time_ =
      base::TimeTicks::Now();
}

void PrefetchServingHandle::OnIsolatedCookiesReadCompleteAndWriteStart() const {
  DCHECK(IsIsolatedCookieCopyInProgress());

  GetCurrentSingleRedirectHopToServe().cookie_read_end_and_write_start_time_ =
      base::TimeTicks::Now();
}

void PrefetchServingHandle::OnIsolatedCookieCopyComplete() const {
  DCHECK(IsIsolatedCookieCopyInProgress());

  // Resumes `PrefetchCookieListener` so that we can keep monitoring the
  // cookie change for the prefetch, which may be served again.
  GetPrefetchContainer()->ResumeAllCookieListeners();

  const auto& this_prefetch = GetCurrentSingleRedirectHopToServe();

  this_prefetch.cookie_copy_status_ =
      PrefetchSingleRedirectHop::CookieCopyStatus::kCompleted;

  if (this_prefetch.cookie_copy_start_time_.has_value() &&
      this_prefetch.cookie_read_end_and_write_start_time_.has_value()) {
    RecordCookieCopyTimes(
        this_prefetch.cookie_copy_start_time_.value(),
        this_prefetch.cookie_read_end_and_write_start_time_.value(),
        base::TimeTicks::Now());
  }

  if (this_prefetch.on_cookie_copy_complete_callback_) {
    std::move(this_prefetch.on_cookie_copy_complete_callback_).Run();
  }
}

void PrefetchServingHandle::OnInterceptorCheckCookieCopy() const {
  if (!GetCurrentSingleRedirectHopToServe().cookie_copy_start_time_) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyStartToInterceptorCheck",
      base::TimeTicks::Now() -
          GetCurrentSingleRedirectHopToServe().cookie_copy_start_time_.value(),
      base::TimeDelta(), base::Seconds(5), 50);
}

void PrefetchServingHandle::SetOnCookieCopyCompleteCallback(
    base::OnceClosure callback) const {
  DCHECK(IsIsolatedCookieCopyInProgress());

  GetCurrentSingleRedirectHopToServe().on_cookie_copy_complete_callback_ =
      std::move(callback);
}

std::pair<PrefetchRequestHandler, base::WeakPtr<ServiceWorkerClient>>
PrefetchServingHandle::CreateRequestHandler() {
  // Create a `PrefetchRequestHandler` from the current
  // `PrefetchSingleRedirectHop` and its corresponding
  // `PrefetchStreamingURLLoader`.
  auto handler = GetCurrentSingleRedirectHopToServe()
                     .response_reader_->CreateRequestHandler();

  // Advance the current `PrefetchSingleRedirectHop` position.
  AdvanceCurrentURLToServe();

  return handler;
}

bool PrefetchServingHandle::VariesOnCookieIndices() const {
  return GetCurrentSingleRedirectHopToServe()
      .response_reader_->VariesOnCookieIndices();
}

bool PrefetchServingHandle::MatchesCookieIndices(
    base::span<const std::pair<std::string, std::string>> cookies) const {
  return GetCurrentSingleRedirectHopToServe()
      .response_reader_->MatchesCookieIndices(cookies);
}

void PrefetchServingHandle::OnPrefetchProbeResult(
    PrefetchProbeResult probe_result) const {
  GetPrefetchContainer()->SetProbeResult(base::PassKey<PrefetchServingHandle>(),
                                         probe_result);

  // TODO(https:crbug.com/433057364): Clean up the code below, so that we no
  // longer need `TriggeringOutcomeFromStatusForServingHandle`.

  // It's possible for the prefetch to fail (e.g., due to a network error) while
  // the origin probe is running. We avoid overwriting the status in that case.
  if (PrefetchContainer::TriggeringOutcomeFromStatusForServingHandle(
          base::PassKey<PrefetchServingHandle>(), GetPrefetchStatus()) ==
      PreloadingTriggeringOutcome::kFailure) {
    return;
  }

  switch (probe_result) {
    case PrefetchProbeResult::kNoProbing:
    case PrefetchProbeResult::kDNSProbeSuccess:
    case PrefetchProbeResult::kTLSProbeSuccess:
      // Wait to update the prefetch status until the probe for the final
      // redirect hop is a success.
      if (index_redirect_chain_to_serve_ == redirect_chain().size() - 1) {
        GetPrefetchContainer()->SetPrefetchStatus(
            PrefetchStatus::kPrefetchResponseUsed);
      }
      break;
    case PrefetchProbeResult::kDNSProbeFailure:
    case PrefetchProbeResult::kTLSProbeFailure:
      GetPrefetchContainer()->SetPrefetchStatus(
          PrefetchStatus::kPrefetchNotUsedProbeFailed);
      break;
    default:
      NOTIMPLEMENTED();
  }
}

bool PrefetchServingHandle::DoesCurrentURLToServeMatch(const GURL& url) const {
  CHECK(index_redirect_chain_to_serve_ >= 1);
  return GetCurrentSingleRedirectHopToServe().url_ == url;
}

bool PrefetchServingHandle::IsEnd() const {
  CHECK(index_redirect_chain_to_serve_ <= redirect_chain().size());
  return index_redirect_chain_to_serve_ >= redirect_chain().size();
}

const PrefetchSingleRedirectHop&
PrefetchServingHandle::GetCurrentSingleRedirectHopToServe() const {
  CHECK(index_redirect_chain_to_serve_ >= 0 &&
        index_redirect_chain_to_serve_ < redirect_chain().size());
  return *redirect_chain()[index_redirect_chain_to_serve_];
}

const GURL& PrefetchServingHandle::GetCurrentURLToServe() const {
  return GetCurrentSingleRedirectHopToServe().url_;
}

bool PrefetchServingHandle::IsIsolatedNetworkContextRequiredToServe() const {
  const PrefetchSingleRedirectHop& this_prefetch =
      GetCurrentSingleRedirectHopToServe();
  return this_prefetch.is_isolated_network_context_required_;
}

base::WeakPtr<PrefetchResponseReader>
PrefetchServingHandle::GetCurrentResponseReaderToServeForTesting() {
  return GetCurrentSingleRedirectHopToServe().response_reader_->GetWeakPtr();
}

PrefetchServableState PrefetchServingHandle::GetServableState(
    base::TimeDelta cacheable_duration) const {
  return GetPrefetchContainer()->GetServableState(cacheable_duration);
}

bool PrefetchServingHandle::HasPrefetchStatus() const {
  return GetPrefetchContainer()->HasPrefetchStatus();
}

PrefetchStatus PrefetchServingHandle::GetPrefetchStatus() const {
  return GetPrefetchContainer()->GetPrefetchStatus();
}

}  // namespace content
