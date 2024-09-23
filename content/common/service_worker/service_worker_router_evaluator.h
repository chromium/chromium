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
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ServiceWorkerRouterEvaluatorErrorEnums {
  kNoError = 0,
  kInvalidType = 1,
  kParseError = 2,
  kCompileError = 3,
  kEmptyCondition = 4,
  kEmptySource = 5,
  kInvalidSource = 6,
  kInvalidCondition = 7,
  kExceedMaxConditionDepth = 8,
  kExceedMaxRouterSize = 9,
  kFetchSourceWithoutFetchHandler = 10,
  kMaxValue = kFetchSourceWithoutFetchHandler,
};

namespace content {

class CONTENT_EXPORT ServiceWorkerRouterEvaluator {
 public:
  explicit ServiceWorkerRouterEvaluator(blink::ServiceWorkerRouterRules rules);
  ~ServiceWorkerRouterEvaluator();

  bool IsValid() const { return is_valid_; }

  struct CONTENT_EXPORT Result {
    Result();
    ~Result();
    Result(Result&&);
    Result& operator=(Result&&) = default;

    Result(const Result& other) = delete;
    Result& operator=(const Result&) = delete;

    std::uint32_t id = 0;
    std::vector<blink::ServiceWorkerRouterSource> sources;
  };

  // Returns an empty list if nothing matched.
  std::optional<Result> Evaluate(
      const network::ResourceRequest& request,
      blink::EmbeddedWorkerStatus running_status) const;
  std::optional<Result> EvaluateWithoutRunningStatus(
      const network::ResourceRequest& request) const;

  const blink::ServiceWorkerRouterRules& rules() const { return rules_; }
  bool need_running_status() const { return need_running_status_; }
  bool has_fetch_event_source() const { return has_fetch_event_source_; }
  bool has_non_fetch_event_source() const {
    return has_non_fetch_event_source_;
  }

  base::Value ToValue() const;
  std::string ToString() const;
  void RecordRouterRuleInfo() const;
  std::tuple<size_t, size_t> GetMaxDepthAndWidth() const;
  const std::optional<ServiceWorkerRouterEvaluatorErrorEnums>&
  invalid_error_code() const {
    return invalid_error_code_;
  }

 private:
  class RouterRule;
  void Compile();
  std::optional<Result> EvaluateInternal(
      const network::ResourceRequest& request,
      std::optional<blink::EmbeddedWorkerStatus> running_status) const;

  const blink::ServiceWorkerRouterRules rules_;
  std::vector<std::unique_ptr<RouterRule>> compiled_rules_;
  bool is_valid_ = false;
  bool need_running_status_ = false;
  bool has_fetch_event_source_ = false;
  bool has_non_fetch_event_source_ = false;
  std::optional<ServiceWorkerRouterEvaluatorErrorEnums> invalid_error_code_;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_EVALUATOR_H_
