// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/compute_pressure_host.h"

#include <utility>

#include "base/sequence_checker.h"
#include "content/browser/compute_pressure/compute_pressure_sample.h"
#include "content/browser/compute_pressure/compute_pressure_sampler.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom-shared.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"

namespace content {

constexpr base::TimeDelta ComputePressureHost::kDefaultVisibleObserverRateLimit;

ComputePressureHost::ComputePressureHost(
    url::Origin origin,
    bool is_supported,
    base::TimeDelta visible_observer_rate_limit,
    base::RepeatingCallback<void(ComputePressureHost*)> did_connection_change)
    : origin_(std::move(origin)),
      is_supported_(is_supported),
      visible_observer_rate_limit_(visible_observer_rate_limit),
      did_connections_change_callback_(std::move(did_connection_change)) {
  DCHECK(network::IsOriginPotentiallyTrustworthy(origin_));

  // base::Unretained use is safe because mojo guarantees the callback will not
  // be called after `receivers_` is deallocated, and `receivers_` is owned by
  // ComputePressureHost.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &ComputePressureHost::OnReceiverDisconnected, base::Unretained(this)));

  // base::Unretained use is safe because mojo guarantees the callback will not
  // be called after `observers_` is deallocated, and `observers_` is owned by
  // ComputePressureHost.
  observers_.set_disconnect_handler(
      base::BindRepeating(&ComputePressureHost::OnObserverRemoteDisconnected,
                          base::Unretained(this)));
}

ComputePressureHost::~ComputePressureHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ComputePressureHost::BindReceiver(
    GlobalRenderFrameHostId frame_id,
    mojo::PendingReceiver<blink::mojom::ComputePressureHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_id);

  receivers_.Add(this, std::move(receiver), frame_id);
  did_connections_change_callback_.Run(this);
}

void ComputePressureHost::AddObserver(
    mojo::PendingRemote<blink::mojom::ComputePressureObserver> observer,
    blink::mojom::ComputePressureQuantizationPtr quantization,
    AddObserverCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ComputePressureQuantizer::IsValid(*quantization)) {
    mojo::ReportBadMessage("Invalid quantization");
    std::move(callback).Run(
        blink::mojom::ComputePressureStatus::kSecurityError);
    return;
  }

  GlobalRenderFrameHostId frame_id = receivers_.current_context();
  RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
  if (!rfh || !rfh->IsActive()) {
    std::move(callback).Run(
        blink::mojom::ComputePressureStatus::kSecurityError);
    return;
  }

  // The API is only available in frames served from the same origin as the
  // top-level frame.
  //
  // This same-origin frame requirement is stricter than the usual same-site
  // requirement for first-party iframes.
  //
  // The same-origin frame requirement is aligned with the requirement that all
  // active observers belonging to the same origin must use the same
  // quantization scheme, which limits information exposure. Web pages must not
  // be able to bypass the quantization scheme limitations via iframes and
  // postMessage(), so any frames that can communicate via postMessage() must
  // either be constrained to the same quantization scheme, or be blocked from
  // using the API.
  if (rfh->GetLastCommittedOrigin() != origin_) {
    std::move(callback).Run(
        blink::mojom::ComputePressureStatus::kSecurityError);
    return;
  }

  if (observers_.empty() || !quantizer_.IsSame(*quantization)) {
    ResetObserverState();
    quantizer_.Assign(std::move(quantization));
  }

  if (!is_supported_) {
    std::move(callback).Run(blink::mojom::ComputePressureStatus::kNotSupported);
    return;
  }

  mojo::RemoteSetElementId observer_id = observers_.Add(std::move(observer));

  bool success = observer_contexts_.emplace(observer_id, frame_id).second;
  DCHECK(success) << "Observers set contains duplicate RemoteSetElementId "
                  << observer_id;

  std::move(callback).Run(blink::mojom::ComputePressureStatus::kOk);
  did_connections_change_callback_.Run(this);
}

void ComputePressureHost::UpdateObservers(ComputePressureSample sample,
                                          base::Time sample_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blink::mojom::ComputePressureState quantized_state =
      quantizer_.Quantize(sample);

  // TODO(pwnall): Rate-limit observers in non-visible frames instead of
  //               cutting off their updates completely.
  if (sample_time - last_report_time_ < visible_observer_rate_limit_) {
    return;
  }

  // No need to send an update if previous value is similar.
  if (last_report_state_ == quantized_state)
    return;

  for (auto it = observers_.begin(); it != observers_.end(); ++it) {
    mojo::RemoteSetElementId observer_id = it.id();

    DCHECK(observer_contexts_.count(observer_id))
        << "AddObserver() failed to register an observer in the map "
        << observer_id;
    GlobalRenderFrameHostId frame_id = observer_contexts_[observer_id];

    RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id);
    if (!rfh || !rfh->IsActive()) {
      // TODO(pwnall): Is it safe to disconnect observers in this state?
      continue;
    }

    if (rfh->GetVisibilityState() !=
        blink::mojom::PageVisibilityState::kVisible) {
      // TODO(pwnall): Rate-limit observers in non-visible frames instead of
      //               cutting off their updates completely.
      continue;
    }

    // `last_report_time_` is updated inside the loop so it remains unchanged if
    // no observer receives an Update() call. This logic will change when we
    // implement sending (less frequent) updates to observers in non-visible
    // frames.
    last_report_time_ = sample_time;
    last_report_state_ = quantized_state;

    (*it)->OnUpdate(quantized_state.Clone());
  }
}

void ComputePressureHost::OnReceiverDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  did_connections_change_callback_.Run(this);
}

void ComputePressureHost::OnObserverRemoteDisconnected(
    mojo::RemoteSetElementId observer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(observer_contexts_.count(observer_id))
      << "AddObserver() failed to register an observer in the map "
      << observer_id;
  observer_contexts_.erase(observer_id);

  did_connections_change_callback_.Run(this);
}

void ComputePressureHost::ResetObserverState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.Clear();
  observer_contexts_.clear();

  // Makes sure that rate-limiting can't be bypassed by changing the
  // quantization scheme often.
  last_report_time_ = base::Time::Now();

  // Setting to an invalid value, so any state is considered an update.
  last_report_state_ = {/* cpu_utilization */ -1,
                        /* cpu_speed */ -1};
}

}  // namespace content
