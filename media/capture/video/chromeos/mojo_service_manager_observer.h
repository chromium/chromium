// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_MOJO_SERVICE_MANAGER_OBSERVER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_MOJO_SERVICE_MANAGER_OBSERVER_H_

#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/mojo_service_manager/mojom/mojo_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {
// MojoServiceManagerObserver is used to observe the service state of the mojo
// service which can be requested from mojo service manager.
// Its construction and destruction have to be on the ui thread.
// Release the MojoServiceManagerObserver instance when the observation is no
// longer needed.
class MojoServiceManagerObserver
    : public chromeos::mojo_service_manager::mojom::ServiceObserver {
 public:
  // This function should only work when calling on the UI task runner.
  // TODO(b/303606330): Let MojoServiceManagerObserver handles thread-safety
  // itself.
  //
  // |on_register_callback| will be invoked
  //   1. when the MojoServiceManagerObserver instance is created if the service
  //      with |service_name| has been registered.
  //   2. whenever the service with |service_name| is registered after
  //      the MojoServiceManagerObserver instance is created.
  //
  // |on_unregister_callback| will be invoked when the service with
  // |service_name| is unregistered after the MojoServiceManagerObserver
  // instance is created.
  //
  // |on_register_callback| and |on_unregister_callback| will be run on the ui
  // thread.
  // It will return nullptr when the endpoint of mojo service manager is not
  // bound.
  static std::unique_ptr<MojoServiceManagerObserver> Create(
      const std::string& service_name,
      base::RepeatingClosure on_register_callback,
      base::RepeatingClosure on_unregister_callback);

  ~MojoServiceManagerObserver() override;
  MojoServiceManagerObserver(const MojoServiceManagerObserver&) = delete;
  MojoServiceManagerObserver& operator=(const MojoServiceManagerObserver&) =
      delete;

 private:
  MojoServiceManagerObserver(const std::string& service_name,
                             base::RepeatingClosure on_register_callback,
                             base::RepeatingClosure on_unregister_callback);

  void OnServiceEvent(
      chromeos::mojo_service_manager::mojom::ServiceEventPtr event) override;

  void QueryCallback(
      chromeos::mojo_service_manager::mojom::ErrorOrServiceStatePtr result);

  std::string service_name_;

  base::RepeatingClosure on_register_callback_;

  base::RepeatingClosure on_unregister_callback_;

  mojo::Receiver<chromeos::mojo_service_manager::mojom::ServiceObserver>
      observer_receiver_{this};

  base::WeakPtrFactory<MojoServiceManagerObserver> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_MOJO_SERVICE_MANAGER_OBSERVER_H_
