// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_INTEROP_PARSER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_INTEROP_PARSER_H_

#include <stdint.h>

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace content {

struct AttributionSimulationEvent {
  struct StartRequest {
    int64_t request_id;
    attribution_reporting::SuitableOrigin context_origin;
    network::mojom::AttributionReportingEligibility eligibility;
    bool fenced = false;
  };

  struct Response {
    int64_t request_id;
    GURL url;
    scoped_refptr<net::HttpResponseHeaders> response_headers;
    // Only relevant for sources, not triggers.
    attribution_reporting::RandomizedResponse randomized_response;
    // Only relevant for triggers, not sources.
    base::flat_set<int> null_aggregatable_reports_days;
    bool debug_permission = false;

    Response(int64_t request_id,
             GURL url,
             scoped_refptr<net::HttpResponseHeaders>,
             attribution_reporting::RandomizedResponse,
             base::flat_set<int> null_aggregatable_reports_days,
             bool debug_permission);

    ~Response();

    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;

    Response(Response&&);
    Response& operator=(Response&&);
  };

  struct EndRequest {
    int64_t request_id;
  };

  using Data = absl::variant<StartRequest, Response, EndRequest>;

  base::Time time;
  Data data;

  AttributionSimulationEvent(base::Time, Data);

  ~AttributionSimulationEvent();

  AttributionSimulationEvent(const AttributionSimulationEvent&) = delete;
  AttributionSimulationEvent& operator=(const AttributionSimulationEvent&) =
      delete;

  AttributionSimulationEvent(AttributionSimulationEvent&&);
  AttributionSimulationEvent& operator=(AttributionSimulationEvent&&);
};

// See //content/test/data/attribution_reporting/interop/README.md for the
// schema.

base::expected<std::vector<AttributionSimulationEvent>, std::string>
    ParseAttributionInteropInput(base::Value::Dict);

struct AttributionInteropConfig {
  AttributionConfig attribution_config;
  double max_event_level_epsilon = 0;
  uint32_t max_trigger_state_cardinality = 0;
  bool needs_cross_app_web = false;
  bool needs_aggregatable_debug = false;
  bool needs_source_destination_limit = false;
  bool needs_aggregatable_filtering_ids = false;
  bool needs_attribution_scopes = false;
  std::vector<url::Origin> aggregation_coordinator_origins;

  AttributionInteropConfig();

  AttributionInteropConfig(const AttributionInteropConfig&);
  AttributionInteropConfig& operator=(const AttributionInteropConfig&);

  AttributionInteropConfig(AttributionInteropConfig&&);
  AttributionInteropConfig& operator=(AttributionInteropConfig&&);

  ~AttributionInteropConfig();

  friend bool operator==(const AttributionInteropConfig&,
                         const AttributionInteropConfig&) = default;
};

base::expected<AttributionInteropConfig, std::string>
    ParseAttributionInteropConfig(base::Value::Dict);

base::expected<void, std::string> MergeAttributionInteropConfig(
    base::Value::Dict,
    AttributionInteropConfig&);

struct AttributionInteropOutput {
  struct Report {
    base::Time time;
    GURL url;
    base::Value payload;

    Report();
    Report(base::Time time, GURL url, base::Value payload);

    // These are necessary because `base::Value` is not copyable.
    Report(const Report&);
    Report& operator=(const Report&);

    base::Value::Dict ToJson() const;

    // TODO(apaseltiner): The payload comparison here is too brittle. Reports
    // can be logically equivalent without having exactly the same JSON
    // structure.
    friend bool operator==(const Report&, const Report&) = default;
  };

  std::vector<Report> reports;

  AttributionInteropOutput();
  ~AttributionInteropOutput();

  AttributionInteropOutput(const AttributionInteropOutput&) = delete;
  AttributionInteropOutput& operator=(const AttributionInteropOutput&) = delete;

  AttributionInteropOutput(AttributionInteropOutput&&);
  AttributionInteropOutput& operator=(AttributionInteropOutput&&);

  base::Value::Dict ToJson() const;

  static base::expected<AttributionInteropOutput, std::string> Parse(
      base::Value::Dict);
};

std::ostream& operator<<(std::ostream&,
                         const AttributionInteropOutput::Report&);

std::ostream& operator<<(std::ostream&, const AttributionInteropOutput&);

struct AttributionInteropRun {
  std::vector<AttributionSimulationEvent> events;
  AttributionInteropConfig config;

  static base::expected<AttributionInteropRun, std::string> Parse(
      base::Value::Dict,
      const AttributionInteropConfig& default_config);

  AttributionInteropRun();

  ~AttributionInteropRun();

  AttributionInteropRun(const AttributionInteropRun&) = delete;
  AttributionInteropRun& operator=(const AttributionInteropRun&) = delete;

  AttributionInteropRun(AttributionInteropRun&&);
  AttributionInteropRun& operator=(AttributionInteropRun&&);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_INTEROP_PARSER_H_
