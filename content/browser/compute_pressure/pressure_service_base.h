// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_BASE_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_BASE_H_

#include <array>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/compute_pressure/pressure_client_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"

namespace content {

class RenderFrameHost;

// This class holds common functions for both frame and workers. It serves all
// the Compute Pressure API mojo requests.
//
// This class is not thread-safe, so each instance must be used on one sequence.
class CONTENT_EXPORT PressureServiceBase
    : public device::mojom::PressureManager {
 public:
  ~PressureServiceBase() override;

  PressureServiceBase(const PressureServiceBase&) = delete;
  PressureServiceBase& operator=(const PressureServiceBase&) = delete;

  // https://www.w3.org/TR/compute-pressure/#dfn-document-has-implicit-focus
  static bool HasImplicitFocus(RenderFrameHost* render_frame_host);

  void BindReceiver(
      mojo::PendingReceiver<device::mojom::PressureManager> receiver);

  virtual bool CanCallAddClient() const;

  // device::mojom::PressureManager implementation.
  void AddClient(mojo::PendingRemote<device::mojom::PressureClient> client,
                 device::mojom::PressureSource source,
                 AddClientCallback callback) override;

  // Verifies if the data should be delivered according to focus status.
  virtual bool ShouldDeliverUpdate() const = 0;

  bool IsManagerReceiverBoundForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return manager_receiver_.is_bound();
  }

  const PressureClientImpl& GetPressureClientForTesting(
      device::mojom::PressureSource source) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return source_to_client_[static_cast<size_t>(source)];
  }

 protected:
  PressureServiceBase();

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void OnPressureManagerDisconnected();

  // Services side.
  // Callback from |manager_receiver_| is passed to |manager_remote_| and the
  // Receiver should be destroyed first so that the callback is invalidated
  // before being discarded.
  mojo::Remote<device::mojom::PressureManager> manager_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Blink side.
  mojo::Receiver<device::mojom::PressureManager> GUARDED_BY_CONTEXT(
      sequence_checker_) manager_receiver_{this};

  std::array<PressureClientImpl,
             static_cast<size_t>(device::mojom::PressureSource::kMaxValue) + 1>
      source_to_client_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_BASE_H_
