// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_CLIENT_IMPL_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_CLIENT_IMPL_H_

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom-forward.h"

namespace content {

class PressureServiceBase;

// PressureServiceImpl owns instances of this class for different
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

  // device::mojom::PressureClient implementation.
  void OnPressureUpdated(device::mojom::PressureUpdatePtr update) override;

  void AddClient(
      device::mojom::PressureManager* pressure_manager,
      mojo::PendingRemote<device::mojom::PressureClient> pending_client,
      device::mojom::PressureSource source,
      device::mojom::PressureManager::AddClientCallback callback);

  void Reset();

  bool has_remote() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return client_remote_.is_bound();
  }

  bool IsClientReceiverBoundForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return client_receiver_.is_bound();
  }

  bool IsClientRemoteBoundForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return client_remote_.is_bound();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // This is safe because PressureServiceBase owns this class.
  raw_ptr<PressureServiceBase> GUARDED_BY_CONTEXT(sequence_checker_) service_;

  // Services side.
  mojo::Receiver<device::mojom::PressureClient> GUARDED_BY_CONTEXT(
      sequence_checker_) client_receiver_{this};

  // Blink side.
  mojo::Remote<device::mojom::PressureClient> client_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_CLIENT_IMPL_H_
