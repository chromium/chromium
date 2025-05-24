// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_CLIENT_IMPL_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_CLIENT_IMPL_H_

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom-forward.h"
#include "third_party/blink/public/mojom/compute_pressure/web_pressure_manager.mojom.h"

namespace content {

class PressureServiceBase;

// PressureServiceBase owns instances of this class for different
// PressureSources.
//
// This class implements the "device::mojom::PressureClient" interface to
// receive "device::mojom::PressureUpdate" from "device::PressureManagerImpl"
// and broadcasts the information to "blink::PressureClientImpl".
//
// This class is not thread-safe, so each instance must be used on one sequence.
class CONTENT_EXPORT PressureClientImpl : public device::mojom::PressureClient {
 public:
  explicit PressureClientImpl(PressureServiceBase* service);
  ~PressureClientImpl() override;

  PressureClientImpl(const PressureClientImpl&) = delete;
  PressureClientImpl& operator=(const PressureClientImpl&) = delete;

  enum class PressureSourceType { kUnknown = 0, kNonVirtual = 1, kVirtual = 2 };

  // device::mojom::PressureClient implementation.
  void OnPressureUpdated(device::mojom::PressureUpdatePtr update) override;

  void Reset();

  // Set the services-side mojo::Receiver pressure source type owned by this
  // class.
  void SetPressureSourceType(bool is_virtual_source);

  // Binds the associated remote from the Blink-side.
  void BindPendingAssociatedRemote(
      mojo::PendingAssociatedRemote<blink::mojom::WebPressureClient>);

  // Create pending remote endpoint to //services.
  mojo::PendingAssociatedRemote<device::mojom::PressureClient>
  BindNewEndpointAndPassRemote();

  bool is_client_associated_remote_bound() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return client_associated_remote_.is_bound();
  }

  PressureSourceType pressure_source_type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return pressure_source_type_;
  }

  // Client to //services.
  bool is_client_receiver_bound() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return client_associated_receiver_.is_bound();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // This is safe because PressureServiceBase owns this class.
  raw_ptr<PressureServiceBase> GUARDED_BY_CONTEXT(sequence_checker_) service_;

  // Tracks if the source is virtual.
  PressureSourceType pressure_source_type_
      GUARDED_BY_CONTEXT(sequence_checker_) = PressureSourceType::kUnknown;

  // Services side.
  mojo::AssociatedReceiver<device::mojom::PressureClient> GUARDED_BY_CONTEXT(
      sequence_checker_) client_associated_receiver_{this};

  // Blink side.
  mojo::AssociatedRemote<blink::mojom::WebPressureClient>
      client_associated_remote_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_CLIENT_IMPL_H_
