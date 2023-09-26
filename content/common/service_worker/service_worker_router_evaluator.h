// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_EVALUATOR_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_EVALUATOR_H_

#include <memory>

#include "base/values.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"

namespace content {

class CONTENT_EXPORT ServiceWorkerRouterEvaluator {
 public:
  explicit ServiceWorkerRouterEvaluator(blink::ServiceWorkerRouterRules rules);
  ~ServiceWorkerRouterEvaluator();

  bool IsValid() const { return is_valid_; }

  // Returns an empty list if nothing matched.
  std::vector<blink::ServiceWorkerRouterSource> Evaluate(
      const network::ResourceRequest& request,
      blink::EmbeddedWorkerStatus running_status) const;
  std::vector<blink::ServiceWorkerRouterSource> EvaluateWithoutRunningStatus(
      const network::ResourceRequest& request) const;

  const blink::ServiceWorkerRouterRules& rules() const { return rules_; }
  bool need_running_status() const { return need_running_status_; }

  base::Value ToValue() const;
  std::string ToString() const;

 private:
  class RouterRule;
  void Compile();
  std::vector<blink::ServiceWorkerRouterSource> EvaluateInternal(
      const network::ResourceRequest& request,
      absl::optional<blink::EmbeddedWorkerStatus> running_status) const;

  const blink::ServiceWorkerRouterRules rules_;
  std::vector<std::unique_ptr<RouterRule>> compiled_rules_;
  bool is_valid_ = false;
  bool need_running_status_ = false;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_EVALUATOR_H_
