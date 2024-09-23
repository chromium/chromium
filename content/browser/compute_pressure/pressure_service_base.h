// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_BASE_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_BASE_H_

#include <array>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "content/browser/compute_pressure/pressure_client_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "third_party/blink/public/mojom/compute_pressure/web_pressure_manager.mojom.h"

namespace content {

class RenderFrameHost;

// This class holds common functions for both frame and workers. It serves all
// the Compute Pressure API mojo requests.
//
// This class is not thread-safe, so each instance must be used on one sequence.
class CONTENT_EXPORT PressureServiceBase
    : public blink::mojom::WebPressureManager {
 public:
  ~PressureServiceBase() override;

  PressureServiceBase(const PressureServiceBase&) = delete;
  PressureServiceBase& operator=(const PressureServiceBase&) = delete;

  // https://www.w3.org/TR/compute-pressure/#dfn-document-has-implicit-focus
  static bool HasImplicitFocus(RenderFrameHost* render_frame_host);

  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::WebPressureManager> receiver);

  virtual bool CanCallAddClient() const;

  // blink::mojom::WebPressureManager implementation.
  void AddClient(device::mojom::PressureSource source,
                 AddClientCallback callback) override;

  // Verifies if the data should be delivered according to focus status.
  virtual bool ShouldDeliverUpdate() const = 0;

  // Returns a token for use with automation calls when one is set.
  virtual std::optional<base::UnguessableToken> GetTokenFor(
      device::mojom::PressureSource) const = 0;

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

  void DidAddClient(device::mojom::PressureSource source,
                    AddClientCallback client_callback,
                    device::mojom::PressureManagerAddClientResultPtr);

  // Services side.
  // Callback from |manager_receiver_| is passed to |manager_remote_| and the
  // Receiver should be destroyed first so that the callback is invalidated
  // before being discarded.
  mojo::Remote<device::mojom::PressureManager> manager_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Blink side.
  mojo::Receiver<blink::mojom::WebPressureManager> GUARDED_BY_CONTEXT(
      sequence_checker_) manager_receiver_{this};

  std::array<PressureClientImpl,
             static_cast<size_t>(device::mojom::PressureSource::kMaxValue) + 1>
      source_to_client_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PressureServiceBase> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_BASE_H_
