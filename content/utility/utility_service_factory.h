// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_SERVICE_FACTORY_H_
#define CONTENT_UTILITY_UTILITY_SERVICE_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {

// Helper for handling incoming RunService requests on UtilityThreadImpl.
class UtilityServiceFactory {
 public:
  UtilityServiceFactory();

  UtilityServiceFactory(const UtilityServiceFactory&) = delete;
  UtilityServiceFactory& operator=(const UtilityServiceFactory&) = delete;

  ~UtilityServiceFactory();

  void RunService(const std::string& service_name,
                  mojo::ScopedMessagePipeHandle service_pipe);
};

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_SERVICE_FACTORY_H_
