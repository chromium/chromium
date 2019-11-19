// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_
#define CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_

#include <memory>
#include <string>

#include "services/tracing/public/cpp/base_agent.h"

namespace content {

// TODO(crbug.com/839086): Remove once we have replaced the legacy tracing
// service with perfetto.
class CrOSTracingAgent : public tracing::BaseAgent {
 public:
  CrOSTracingAgent();

 private:
  friend std::default_delete<CrOSTracingAgent>;

  ~CrOSTracingAgent() override;

  DISALLOW_COPY_AND_ASSIGN(CrOSTracingAgent);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_CROS_TRACING_AGENT_H_
