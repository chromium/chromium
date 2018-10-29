// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_EMBEDDED_INSTANCE_MANAGER_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_EMBEDDED_INSTANCE_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
class Thread;

enum class ThreadPriority : int;
}  // namespace base

namespace service_manager {

class EmbeddedInstanceManagerTestApi;

// EmbeddedInstanceManager is an implementation detail of EmbeddedServiceRunner.
// Outside of tests there is no need to use it directly.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) EmbeddedInstanceManager
    : public base::RefCountedThreadSafe<EmbeddedInstanceManager> {
 public:
  EmbeddedInstanceManager(const base::StringPiece& name,
                          const EmbeddedServiceInfo& info,
                          const base::Closure& quit_closure);

  void BindServiceRequest(service_manager::mojom::ServiceRequest request);

  void ShutDown();

 private:
  friend class base::RefCountedThreadSafe<EmbeddedInstanceManager>;
  friend class EmbeddedInstanceManagerTestApi;

  ~EmbeddedInstanceManager();

  void BindServiceRequestOnServiceSequence(
      service_manager::mojom::ServiceRequest request);

  void OnInstanceLost(int instance_id);

  void QuitOnServiceSequence();

  void QuitOnRunnerThread();

  const std::string name_;
  const EmbeddedServiceInfo::ServiceFactory factory_callback_;
  const bool use_own_thread_;
  base::MessageLoop::Type message_loop_type_;
  base::ThreadPriority thread_priority_;
  const base::Closure quit_closure_;
  const scoped_refptr<base::SingleThreadTaskRunner> quit_task_runner_;

  // Thread checker used to ensure certain operations happen only on the
  // runner's (i.e. our owner's) thread.
  THREAD_CHECKER(runner_thread_checker_);

  // These fields must only be accessed from the runner's thread.
  std::unique_ptr<base::Thread> thread_;
  scoped_refptr<base::SequencedTaskRunner> service_task_runner_;

  // These fields must only be accessed from the service thread, except in
  // the destructor which may run on either the runner thread or the service
  // thread.

  // A map which owns all existing Service instances for this service.
  using ServiceContextMap =
      std::map<service_manager::ServiceContext*,
               std::unique_ptr<service_manager::ServiceContext>>;
  ServiceContextMap contexts_;

  int next_instance_id_ = 0;

  // A mapping from instance ID to (not owned) ServiceContext.
  //
  // TODO(rockot): Remove this once we get rid of the quit closure argument to
  // service factory functions.
  std::map<int, service_manager::ServiceContext*> id_to_context_map_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedInstanceManager);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_EMBEDDED_INSTANCE_MANAGER_H_
