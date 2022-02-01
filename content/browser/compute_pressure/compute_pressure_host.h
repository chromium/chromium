// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_HOST_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_HOST_H_

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/browser/compute_pressure/compute_pressure_quantizer.h"
#include "content/browser/compute_pressure/compute_pressure_sample.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom.h"
#include "url/origin.h"

namespace content {

// Serves all the Compute Pressure API mojo requests for an origin.
//
// ComputePressureManager owns an instance of this class for each origin that
// has at least one active mojo connection associated with the Compute Pressure
// API. Instances are dynamically created and destroyed to meet incoming
// requests from renderers.
//
// This class is not thread-safe, so each instance must be used on one sequence.
class CONTENT_EXPORT ComputePressureHost
    : public blink::mojom::ComputePressureHost {
 public:
  static constexpr base::TimeDelta kDefaultVisibleObserverRateLimit =
      base::Seconds(1);

  // `did_connection_change` is called on changes to the mojo connections
  // handled by this host are changed. The callback will not be Run() after
  // the ComputePressureHost instance goes out of scope.
  //
  // If `is_supported` is false, UpdateObservers() will never get called.
  // Observer requests will be bounced.
  //
  // `visible_observer_rate_limit` is exposed to avoid idling in tests.
  // Production code should pass
  // ComputePressureHost::kDefaultVisibleObserverRateLimit.
  explicit ComputePressureHost(
      url::Origin origin,
      bool is_supported,
      base::TimeDelta visible_observer_rate_limit,
      base::RepeatingCallback<void(ComputePressureHost*)>
          did_connections_change);

  ~ComputePressureHost() override;

  const url::Origin& origin() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return origin_;
  }
  bool has_receivers() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !receivers_.empty();
  }
  bool has_observers() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !observers_.empty();
  }

  void BindReceiver(
      GlobalRenderFrameHostId frame_id,
      mojo::PendingReceiver<blink::mojom::ComputePressureHost> receiver);

  // blink::mojom::ComputePressureHost implementation.
  void AddObserver(
      mojo::PendingRemote<blink::mojom::ComputePressureObserver> observer,
      blink::mojom::ComputePressureQuantizationPtr quantization,
      AddObserverCallback callback) override;

  // Called periodically by the ComputePressureManager that owns this host.
  void UpdateObservers(ComputePressureSample sample, base::Time sample_time);

 private:
  void OnReceiverDisconnected();

  void OnObserverRemoteDisconnected(mojo::RemoteSetElementId observer_id);

  // Resets the state used to dispatch updates to observers.
  //
  // Called when the quantizing scheme changes. The caller is responsible for
  // invoking `did_connection_change_callback_`.
  void ResetObserverState();

  SEQUENCE_CHECKER(sequence_checker_);

  // The origin whose connections are handled by this host.
  const url::Origin origin_;

  // If false, the platform does not support compute pressure reports.
  const bool is_supported_;

  // The minimum delay between two Update() calls for observers belonging to
  // visibile frames.
  const base::TimeDelta visible_observer_rate_limit_;

  // Keeps track of the frame associated with each receiver.
  mojo::ReceiverSet<blink::mojom::ComputePressureHost, GlobalRenderFrameHostId>
      receivers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Called to inform the owner when the set of receivers or remotes changes.
  //
  // The owner may destroy this ComputePressureHost while the callback is
  // running.
  base::RepeatingCallback<void(ComputePressureHost*)>
      did_connections_change_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Active observers that belong to this origin.
  mojo::RemoteSet<blink::mojom::ComputePressureObserver> observers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Keeps track of the frame associated with each observer.
  //
  // Used to determine the rate-limiting appropriate for each observer.
  std::map<mojo::RemoteSetElementId, GlobalRenderFrameHostId> observer_contexts_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Implements the quantizing scheme used for all the origin's observers.
  ComputePressureQuantizer quantizer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The (quantized) sample that was last reported to this origin's observers.
  //
  // Stored to avoid sending updates when the underlying compute pressure state
  // changes, but quantization produces the same values that were reported in
  // the last update.
  blink::mojom::ComputePressureState last_report_state_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The last time the origin's observers received an update.
  //
  // Stored to implement rate-limiting.
  base::Time last_report_time_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_HOST_H_
