// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_
#define CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_

#include <memory>

#include "services/tracing/public/cpp/base_agent.h"

namespace content {

// TODO(crbug.com/41386726): Remove once we have replaced the legacy tracing
// service with perfetto.
class CrOSTracingAgent : public tracing::BaseAgent {
 public:
  CrOSTracingAgent();

  CrOSTracingAgent(const CrOSTracingAgent&) = delete;
  CrOSTracingAgent& operator=(const CrOSTracingAgent&) = delete;

 private:
  friend std::default_delete<CrOSTracingAgent>;

  ~CrOSTracingAgent() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_
