// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/child_memory_consumer_registry_host.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "content/common/buildflags.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {

// ChildMemoryConsumerRegistryHost::RenderProcessExitedObserver ----------------

// Helper class that observes a `RenderProcessHost` for exit signals.
//
// In general, `RenderProcessHost` instances can be reused after shutdown. In
// a normal configuration, the child process is forcefully terminated and all
// associated Mojo pipes are closed before a new child is spawned. This
// ensures a 1:1 mapping between a `ChildProcessId` and an active Mojo
// connection: an invariant the memory coordinator relies on to track the
// hosting process of a child `MemoryConsumer`.
//
// However, if `ChildProcessLauncher::terminate_child_on_shutdown_` is false,
// the initial child process may outlive the start of the new one. Because
// the old Mojo pipe isn't closed immediately, the new process binds its own
// pipe while the old one is still active, breaking the 1:1 invariant.
//
// This class uses `RenderProcessHostObserver` to bridge that gap, as
// `RenderProcessExited` provides the reliable signal needed to clean up
// state even when the process outlives its host's initial shutdown phase.
class ChildMemoryConsumerRegistryHost::RenderProcessExitedObserver
    : public RenderProcessHostObserver {
 public:
  RenderProcessExitedObserver(RenderProcessHost* host,
                              base::OnceClosure on_exited)
      : on_exited_(std::move(on_exited)) {
    observation_.Observe(host);
  }

  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    // Calling `on_exited_` will delete `this`.
    std::move(on_exited_).Run();
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    NOTREACHED();
  }

 private:
  base::ScopedObservation<RenderProcessHost, RenderProcessHostObserver>
      observation_{this};
  base::OnceClosure on_exited_;
};

// ChildMemoryConsumerRegistryHost --------------------------------------------

ChildMemoryConsumerRegistryHost::ChildMemoryConsumerRegistryHost(
    MemoryConsumerGroupController& controller,
    ProcessType process_type,
    ChildProcessId child_process_id,
    mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver,
    base::OnceClosure disconnect_handler)
    : controller_(controller),
      process_type_(process_type),
      child_process_id_(child_process_id),
      receiver_(this, std::move(receiver)),
      disconnect_handler_(std::move(disconnect_handler)) {
  CHECK(disconnect_handler_);

  controller_->AddMemoryConsumerGroupHost(child_process_id_, this);

  // The use of Unretained is safe here because `this` owns the receiver and
  // will always outlive it.
  receiver_.set_disconnect_handler(
      base::BindOnce(&ChildMemoryConsumerRegistryHost::RunDisconnectHandler,
                     base::Unretained(this)));

  if (process_type_ == PROCESS_TYPE_RENDERER) {
    RenderProcessHost* rph = RenderProcessHost::FromID(child_process_id_);
    CHECK(rph);
    // The use of Unretained is safe here because `this` owns the observer and
    // will always outlive it.
    process_observer_ = std::make_unique<RenderProcessExitedObserver>(
        rph,
        base::BindOnce(&ChildMemoryConsumerRegistryHost::RunDisconnectHandler,
                       base::Unretained(this)));
  }
}

ChildMemoryConsumerRegistryHost::~ChildMemoryConsumerRegistryHost() {
  for (const auto& consumer_id : consumers_) {
    controller_->OnConsumerGroupRemoved(consumer_id, child_process_id_);
  }
  controller_->RemoveMemoryConsumerGroupHost(child_process_id_);
}

void ChildMemoryConsumerRegistryHost::BindCoordinator(
    mojo::PendingRemote<mojom::ChildMemoryCoordinator> coordinator_remote) {
  if (coordinator_remote_.is_bound()) {
    mojo::ReportBadMessage("BindCoordinator called more than once");
    return;
  }
  coordinator_remote_.Bind(std::move(coordinator_remote));
  // The use of Unretained is safe here because `this` owns the remote and will
  // always outlive it.
  coordinator_remote_.set_disconnect_handler(
      base::BindOnce(&ChildMemoryConsumerRegistryHost::RunDisconnectHandler,
                     base::Unretained(this)));

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  // If diagnostics were enabled before BindCoordinator, now we can bind the
  // diagnostic interface.
  if (diagnostics_enabled_) {
    EnableReportingImpl();
  }
#endif
}

void ChildMemoryConsumerRegistryHost::Register(
    const std::string& consumer_id,
    std::optional<base::MemoryConsumerTraits> traits) {
  if (!coordinator_remote_.is_bound()) {
    mojo::ReportBadMessage("Register called before BindCoordinator");
    return;
  }

  auto [_, inserted] = consumers_.insert(consumer_id);
  if (!inserted) {
    mojo::ReportBadMessage("Register called for an existing consumer_id");
    return;
  }

  controller_->OnConsumerGroupAdded(consumer_id, traits, process_type_,
                                    child_process_id_);
}

void ChildMemoryConsumerRegistryHost::Unregister(
    const std::string& consumer_id) {
  auto it = consumers_.find(consumer_id);
  if (it == consumers_.end()) {
    mojo::ReportBadMessage("Unregister called for a non-existing consumer_id");
    return;
  }

  controller_->OnConsumerGroupRemoved(consumer_id, child_process_id_);
  consumers_.erase(it);
}

void ChildMemoryConsumerRegistryHost::UpdateConsumers(
    std::vector<MemoryConsumerUpdate> updates) {
  coordinator_remote_->UpdateConsumers(std::move(updates));
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
void ChildMemoryConsumerRegistryHost::OnMemoryLimitChanged(
    const std::string& consumer_id,
    int32_t memory_limit) {
  if (memory_limit < 0) {
    mojo::ReportBadMessage("OnMemoryLimitChanged: out of range");
    return;
  }
  controller_->OnMemoryLimitChanged(consumer_id, child_process_id_,
                                    memory_limit);
}

void ChildMemoryConsumerRegistryHost::EnableDiagnosticsReporting() {
  CHECK(!diagnostics_enabled_);
  diagnostics_enabled_ = true;

  if (coordinator_remote_.is_bound()) {
    EnableReportingImpl();
  }
}

void ChildMemoryConsumerRegistryHost::DisableDiagnosticsReporting() {
  CHECK(diagnostics_enabled_);
  diagnostics_enabled_ = false;

  diagnostics_host_receiver_.reset();
}
#endif  // BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

void ChildMemoryConsumerRegistryHost::RunDisconnectHandler() {
  // Calling `disconnect_handler_` will delete `this`.
  std::move(disconnect_handler_).Run();
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
void ChildMemoryConsumerRegistryHost::EnableReportingImpl() {
  CHECK(diagnostics_enabled_);
  CHECK(coordinator_remote_.is_bound());
  CHECK(!diagnostics_host_receiver_.is_bound());
  coordinator_remote_->EnableDiagnosticsReporting(
      diagnostics_host_receiver_.BindNewPipeAndPassRemote());
  // The use of Unretained is safe here because `this` owns the remote and
  // will always outlive it.
  diagnostics_host_receiver_.set_disconnect_handler(
      base::BindOnce(&ChildMemoryConsumerRegistryHost::RunDisconnectHandler,
                     base::Unretained(this)));
}
#endif  // BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

}  // namespace content
