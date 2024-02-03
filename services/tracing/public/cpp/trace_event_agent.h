// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_AGENT_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_AGENT_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/base_agent.h"

namespace tracing {

class COMPONENT_EXPORT(TRACING_CPP) TraceEventAgent : public BaseAgent {
 public:
  static TraceEventAgent* GetInstance();

  TraceEventAgent(const TraceEventAgent&) = delete;
  TraceEventAgent& operator=(const TraceEventAgent&) = delete;

  void GetCategories(std::set<std::string>* category_set) override;

 private:
  friend base::NoDestructor<tracing::TraceEventAgent>;

  TraceEventAgent();
  ~TraceEventAgent() override;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace tracing
#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_AGENT_H_
