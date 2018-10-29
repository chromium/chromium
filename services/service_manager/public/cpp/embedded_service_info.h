// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_EMBEDDED_SERVICE_INFO_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_EMBEDDED_SERVICE_INFO_H_

#include <memory>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"

namespace service_manager {
class Service;

// EmbeddedServiceInfo provides details necessary to construct and bind new
// instances of embedded services.
struct COMPONENT_EXPORT(SERVICE_MANAGER_CPP) EmbeddedServiceInfo {
  using ServiceFactory =
      base::RepeatingCallback<std::unique_ptr<service_manager::Service>()>;

  EmbeddedServiceInfo();
  EmbeddedServiceInfo(const EmbeddedServiceInfo& other);
  ~EmbeddedServiceInfo();

  // A factory function which will be called to produce a new Service
  // instance for this service whenever one is needed.
  ServiceFactory factory;

  // The task runner on which to construct and bind new Service instances
  // for this service. If null, behavior depends on the value of
  // |use_own_thread| below.
  scoped_refptr<base::SequencedTaskRunner> task_runner;

  // If |task_runner| is null, setting this to |true| will give each instance of
  // this service its own thread to run on. Setting this to |false| (the
  // default) will instead run the service on the main thread's task runner.
  //
  // If |task_runner| is not null, this value is ignored.
  bool use_own_thread = false;

  // If the service uses its own thread, this determines the type of the message
  // loop used by the thread.
  base::MessageLoop::Type message_loop_type = base::MessageLoop::TYPE_DEFAULT;

  // If the service uses its own thread, this determines the priority of the
  // thread.
  base::ThreadPriority thread_priority = base::ThreadPriority::NORMAL;

  // If set, serves as a hint to the embedding environment that instances of
  // this service should share a process with similar instances of any other
  // services that are registered with the same group name. Choice of group
  // names is arbitrary.
  base::Optional<std::string> process_group;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_EMBEDDED_SERVICE_INFO_H_
