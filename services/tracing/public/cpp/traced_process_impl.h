// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACED_PROCESS_IMPL_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACED_PROCESS_IMPL_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/mojom/system_tracing_service.mojom.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"

namespace tracing {

class BaseAgent;

// Each running process will bind this singleton
// to the mojom::TracedProcess interface, which will be
// connected to by the tracing service to enable tracing
// support.
class COMPONENT_EXPORT(TRACING_CPP) TracedProcessImpl
    : public mojom::TracedProcess {
 public:
  static TracedProcessImpl* GetInstance();

  TracedProcessImpl(const TracedProcessImpl&) = delete;
  TracedProcessImpl& operator=(const TracedProcessImpl&) = delete;

  void ResetTracedProcessReceiver();
  void OnTracedProcessRequest(
      mojo::PendingReceiver<mojom::TracedProcess> receiver);
  void EnableSystemTracingService(
      mojo::PendingRemote<mojom::SystemTracingService> remote);

  // Set which taskrunner to bind any incoming requests on.
  void SetTaskRunner(scoped_refptr<base::SequencedTaskRunner> task_runner);

  void RegisterAgent(BaseAgent* agent);
  void UnregisterAgent(BaseAgent* agent);

  // Populate categories from all of the registered agents.
  void GetCategories(std::set<std::string>* category_set);

  mojo::Remote<mojom::SystemTracingService>& system_tracing_service();

 private:
  friend class base::NoDestructor<TracedProcessImpl>;
  TracedProcessImpl();
  ~TracedProcessImpl() override;

  // tracing::mojom::TracedProcess:
  void ConnectToTracingService(
      mojom::ConnectToTracingRequestPtr request,
      ConnectToTracingServiceCallback callback) override;

  base::Lock agents_lock_;  // Guards access to |agents_|.
  std::set<raw_ptr<BaseAgent, SetExperimental>> agents_;
  mojo::Receiver<tracing::mojom::TracedProcess> receiver_{this};
  mojo::Remote<mojom::SystemTracingService> system_tracing_service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACED_PROCESS_IMPL_H_
