// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_IMPL_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom.h"
#include "third_party/blink/public/mojom/compute_pressure/pressure_service.mojom.h"

namespace content {

class RenderFrameHost;

// Serves all the Compute Pressure API mojo requests for a frame.
// RenderFrameHostImpl owns an instance of this class.
//
// This class is not thread-safe, so each instance must be used on one sequence.
class CONTENT_EXPORT PressureServiceImpl
    : public blink::mojom::PressureService,
      public device::mojom::PressureClient,
      public DocumentUserData<PressureServiceImpl> {
 public:
  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::PressureService> receiver);

  ~PressureServiceImpl() override;

  PressureServiceImpl(const PressureServiceImpl&) = delete;
  PressureServiceImpl& operator=(const PressureServiceImpl&) = delete;

  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::PressureService> receiver);

  // blink::mojom::PressureService implementation.
  void BindObserver(
      mojo::PendingRemote<blink::mojom::PressureObserver> observer,
      BindObserverCallback callback) override;

  // device::mojom::PressureClient implementation.
  void PressureStateChanged(device::mojom::PressureUpdatePtr update) override;

 private:
  explicit PressureServiceImpl(RenderFrameHost* render_frame_host);

  void OnObserverRemoteDisconnected();

  void OnManagerRemoteDisconnected();

  void DidBindObserver(BindObserverCallback callback, bool success);

  // Resets the state used to dispatch updates to observer.
  void ResetObserverState();

  SEQUENCE_CHECKER(sequence_checker_);

  // Callback from |receiver_| is passed to |remote_| and the Receiver
  // should be destroyed first so that the callback is invalidated before
  // being discarded.
  mojo::Remote<device::mojom::PressureManager> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Receiver<device::mojom::PressureClient> GUARDED_BY_CONTEXT(
      sequence_checker_) client_{this};

  mojo::Remote<blink::mojom::PressureObserver> observer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::Receiver<blink::mojom::PressureService> GUARDED_BY_CONTEXT(
      sequence_checker_) receiver_{this};

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_IMPL_H_
