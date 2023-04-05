// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_recorder_client_interface_registry.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequence_manager/tasks.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace metrics {
namespace {

class UkmRecorderClientInterfaceRegistryStorage {
 public:
  // Struct Storage stores SequencedTaskRunner & WeakPtr to the
  // UkmRecorderClientInterfaceRegistry. It helps in guaranteeing that tasks in
  // registry run in sequence and are thread-safe and ensuring that none of the
  // member function can be called once the registry is destroyed or multiple
  // delegates are seen.
  struct Storage {
    base::WeakPtr<metrics::UkmRecorderClientInterfaceRegistry> weak_ptr;
    scoped_refptr<base::SequencedTaskRunner> task_runner;
  };

  static UkmRecorderClientInterfaceRegistryStorage& GetInstance() {
    static base::NoDestructor<UkmRecorderClientInterfaceRegistryStorage>
        g_instance;
    return *g_instance;
  }

  Storage GetStorage() {
    base::AutoLock lock(lock_);
    return storage_;
  }

  void SetStorage(Storage storage) {
    base::AutoLock lock(lock_);
    storage_ = storage;
  }

  void ClearStorage() {
    base::AutoLock lock(lock_);
    storage_ = Storage();
  }

 private:
  base::Lock lock_;
  Storage storage_ GUARDED_BY(lock_);
};
}  // namespace

UkmRecorderClientInterfaceRegistry::UkmRecorderClientInterfaceRegistry() {
  UkmRecorderClientInterfaceRegistryStorage::GetInstance().SetStorage(
      {weak_ptr_factory_.GetWeakPtr(),
       base::SequencedTaskRunner::GetCurrentDefault()});
  if (ukm::DelegatingUkmRecorder::Get()->HasMultipleDelegates()) {
    OnMultipleDelegates();
  }
}

UkmRecorderClientInterfaceRegistry::~UkmRecorderClientInterfaceRegistry() {
  UkmRecorderClientInterfaceRegistryStorage::GetInstance().ClearStorage();
}

// static
void UkmRecorderClientInterfaceRegistry::AddClientToCurrentRegistry(
    mojo::PendingRemote<ukm::mojom::UkmRecorderClientInterface>
        pending_remote) {
  auto storage =
      UkmRecorderClientInterfaceRegistryStorage::GetInstance().GetStorage();
  if (!storage.task_runner) {
    return;
  }
  // storage.weak_ptr will always be set if storage.task_runner is not null.
  if (storage.task_runner->RunsTasksInCurrentSequence()) {
    storage.weak_ptr->AddClientOnSequence(std::move(pending_remote));
    return;
  }
  storage.task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&UkmRecorderClientInterfaceRegistry::AddClientOnSequence,
                     std::move(storage.weak_ptr), std::move(pending_remote)));
}

// static
void UkmRecorderClientInterfaceRegistry::NotifyMultipleDelegates() {
  auto storage =
      UkmRecorderClientInterfaceRegistryStorage::GetInstance().GetStorage();

  if (!storage.task_runner) {
    return;
  }
  // storage.weak_ptr will always be set if storage.task_runner is not null.
  // Always PostTask to not block the calling code unnecessarily.
  storage.task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&UkmRecorderClientInterfaceRegistry::OnMultipleDelegates,
                     std::move(storage.weak_ptr)));
}

void UkmRecorderClientInterfaceRegistry::AddClientOnSequence(
    mojo::PendingRemote<ukm::mojom::UkmRecorderClientInterface>
        pending_remote) {
  auto id = clients_.Add(std::move(pending_remote));

  if (!params_) {
    return;
  }

  clients_.Get(id)->SetParameters(params_.Clone());
}

void UkmRecorderClientInterfaceRegistry::OnMultipleDelegates() {
  // Invalidates all weak pointers associated with weak_ptr_factory_ to remove
  // all pending tasks including tasks which might end up calling
  // AddClientOnSequence and add a new client after clearing clients_.
  weak_ptr_factory_.InvalidateWeakPtrs();

  clients_.Clear();
  // Remove registry from Storage after clearing all clients_.
  UkmRecorderClientInterfaceRegistryStorage::GetInstance().ClearStorage();
}

void UkmRecorderClientInterfaceRegistry::SetRecorderParameters(
    ukm::mojom::UkmRecorderParametersPtr params) {
  // If no change in params_, don't update the clients.
  if (params_ == params) {
    return;
  }

  params_ = std::move(params);

  for (auto& c : clients_) {
    c->SetParameters(params_->Clone());
  }
}

}  // namespace metrics
