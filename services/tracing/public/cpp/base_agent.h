// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_BASE_AGENT_H_
#define SERVICES_TRACING_PUBLIC_CPP_BASE_AGENT_H_

#include <set>
#include <string>

#include "base/component_export.h"

namespace tracing {

// TODO(oysteine): Remove once we have a way of enumerating available
// categories via Perfetto.
class COMPONENT_EXPORT(TRACING_CPP) BaseAgent {
 public:
  BaseAgent(const BaseAgent&) = delete;
  BaseAgent& operator=(const BaseAgent&) = delete;

  virtual ~BaseAgent();

  // May be called on any thread.
  virtual void GetCategories(std::set<std::string>* category_set);

 protected:
  BaseAgent();
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_BASE_AGENT_H_
