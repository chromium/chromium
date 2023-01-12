// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/base_agent.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/trace_event/trace_log.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/mojom/constants.mojom.h"

namespace tracing {

BaseAgent::BaseAgent() {
  TracedProcessImpl::GetInstance()->RegisterAgent(this);
}

BaseAgent::~BaseAgent() {
  TracedProcessImpl::GetInstance()->UnregisterAgent(this);
}

void BaseAgent::GetCategories(std::set<std::string>* category_set) {}

}  // namespace tracing
