// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_MOJOM_IMPL_H_
#define SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_MOJOM_IMPL_H_

#include <list>
#include <memory>
#include <set>

#include "base/component_export.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/on_device_model/backend_model.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace on_device_model {

inline constexpr base::TimeDelta kDefaultModelIdleTimeout = base::Minutes(5);

namespace {
class SessionWrapper;
}  // namespace

// The implementation of the OnDeviceModel mojom interface. This is a
// self-owned object that deletes itself when the model is no longer used.
class COMPONENT_EXPORT(ON_DEVICE_MODEL) OnDeviceModelMojomImpl
    : public mojom::OnDeviceModel {
 public:
  explicit OnDeviceModelMojomImpl(
      std::unique_ptr<BackendModel> model,
      mojo::PendingReceiver<mojom::OnDeviceModel> receiver,
      base::OnceCallback<void(base::WeakPtr<mojom::OnDeviceModel>)> on_delete);
  ~OnDeviceModelMojomImpl() override;

  OnDeviceModelMojomImpl(const OnDeviceModelMojomImpl&) = delete;
  OnDeviceModelMojomImpl& operator=(const OnDeviceModelMojomImpl&) = delete;

  void AddAndRunPendingTask(
      base::OnceCallback<void(base::OnceClosure finish_callback)> task,
      base::WeakPtr<SessionWrapper> session);

  void AddSession(mojo::PendingReceiver<mojom::Session> receiver,
                  std::unique_ptr<BackendSession> session,
                  mojom::Priority priority);

  void SetForceQueueingForTesting(bool force_queueing);

 private:
  // mojom::OnDeviceModel:
  void StartSession(mojo::PendingReceiver<mojom::Session> session,
                    mojom::SessionParamsPtr params) override;
  void ClassifyTextSafety(const std::string& text,
                          ClassifyTextSafetyCallback callback) override;
  void DetectLanguage(const std::string& text,
                      DetectLanguageCallback callback) override;
  void LoadAdaptation(mojom::LoadAdaptationParamsPtr params,
                      mojo::PendingReceiver<mojom::OnDeviceModel> model,
                      LoadAdaptationCallback callback) override;

  struct PendingTask;

  void SessionDisconnected(SessionWrapper* ptr);
  void ModelDisconnected();
  void LoadAdaptationInternal(mojom::LoadAdaptationParamsPtr params,
                              mojo::PendingReceiver<mojom::OnDeviceModel> model,
                              LoadAdaptationCallback callback);
  void RunTaskIfPossible();
  void TaskFinished();
  void RestartIdleTimer();
  void OnIdleTimeout();

  std::unique_ptr<BackendModel> model_;
  std::set<std::unique_ptr<SessionWrapper>, base::UniquePtrComparator>
      sessions_;
  mojo::ReceiverSet<mojom::OnDeviceModel,
                    std::unique_ptr<BackendModel::ScopedAdaptation>>
      receivers_;
  base::OnceCallback<void(base::WeakPtr<mojom::OnDeviceModel>)> on_delete_;
  std::list<std::unique_ptr<PendingTask>> pending_tasks_;
  bool is_running_ = false;
  bool force_queueing_for_testing_ = false;
  // This timer is active if there are no pending tasks. If the timer triggers,
  // the model remote will be reset.
  std::optional<base::OneShotTimer> idle_timer_;

  base::WeakPtrFactory<OnDeviceModelMojomImpl> weak_ptr_factory_{this};
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_ON_DEVICE_MODEL_MOJOM_IMPL_H_
