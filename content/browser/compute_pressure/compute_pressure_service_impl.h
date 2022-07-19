// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_SERVICE_IMPL_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/browser/compute_pressure/compute_pressure_quantizer.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/compute_pressure_manager.mojom.h"
#include "services/device/public/mojom/compute_pressure_state.mojom.h"
#include "third_party/blink/public/mojom/compute_pressure/compute_pressure.mojom.h"

namespace content {

class RenderFrameHost;

// Serves all the Compute Pressure API mojo requests for a frame.
// RenderFrameHostImpl owns an instance of this class.
//
// This class is not thread-safe, so each instance must be used on one sequence.
class CONTENT_EXPORT ComputePressureServiceImpl
    : public blink::mojom::ComputePressureService,
      public device::mojom::ComputePressureClient,
      public DocumentUserData<ComputePressureServiceImpl> {
 public:
  static constexpr base::TimeDelta kDefaultVisibleObserverRateLimit =
      base::Seconds(1);

  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ComputePressureService> receiver);

  ~ComputePressureServiceImpl() override;

  ComputePressureServiceImpl(const ComputePressureServiceImpl&) = delete;
  ComputePressureServiceImpl& operator=(const ComputePressureServiceImpl&) =
      delete;

  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::ComputePressureService> receiver);

  // blink::mojom::ComputePressureService implementation.
  void AddObserver(
      mojo::PendingRemote<blink::mojom::ComputePressureObserver> observer,
      blink::mojom::ComputePressureQuantizationPtr quantization,
      AddObserverCallback callback) override;

  // device::mojom::ComputePressureClient implementation.
  void ComputePressureStateChanged(device::mojom::ComputePressureStatePtr state,
                                   base::Time timestamp) override;

 private:
  friend class content::DocumentUserData<ComputePressureServiceImpl>;

  ComputePressureServiceImpl(RenderFrameHost* render_frame_host,
                             base::TimeDelta visible_observer_rate_limit);

  void OnObserverRemoteDisconnected(mojo::RemoteSetElementId /*id*/);

  void OnManagerRemoteDisconnected();

  void DidAddObserver(
      mojo::PendingRemote<blink::mojom::ComputePressureObserver> observer,
      AddObserverCallback callback,
      bool success);

  // Resets the state used to dispatch updates to observers.
  //
  // Called when the quantizing scheme changes.
  void ResetObserverState();

  SEQUENCE_CHECKER(sequence_checker_);

  // The minimum delay between two Update() calls for observers belonging to
  // the frame.
  const base::TimeDelta visible_observer_rate_limit_;

  // Implements the quantizing scheme used for all the frame's observers.
  ComputePressureQuantizer quantizer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The (quantized) sample that was last reported to this frame's observers.
  //
  // Stored to avoid sending updates when the underlying compute pressure state
  // changes, but quantization produces the same values that were reported in
  // the last update.
  device::mojom::ComputePressureState last_reported_state_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The last time the frame's observers received an update.
  //
  // Stored to implement rate-limiting.
  base::Time last_reported_timestamp_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::RemoteSet<blink::mojom::ComputePressureObserver> observers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::ReceiverSet<blink::mojom::ComputePressureService> receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Remote<device::mojom::ComputePressureManager> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Receiver<device::mojom::ComputePressureClient> GUARDED_BY_CONTEXT(
      sequence_checker_) client_{this};

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_SERVICE_IMPL_H_
