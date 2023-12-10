// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_PARSER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_PARSER_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

struct AttributionSimulationEvent {
  attribution_reporting::SuitableOrigin reporting_origin;
  attribution_reporting::SuitableOrigin context_origin;
  // If null, the event represents a trigger. Otherwise, represents a source.
  absl::optional<attribution_reporting::mojom::SourceType> source_type;
  base::Value registration;
  base::Time time;
  bool debug_permission = false;

  AttributionSimulationEvent(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::SuitableOrigin context_origin);

  ~AttributionSimulationEvent();

  AttributionSimulationEvent(const AttributionSimulationEvent&) = delete;
  AttributionSimulationEvent& operator=(const AttributionSimulationEvent&) =
      delete;

  AttributionSimulationEvent(AttributionSimulationEvent&&);
  AttributionSimulationEvent& operator=(AttributionSimulationEvent&&);
};

using AttributionSimulationEvents = std::vector<AttributionSimulationEvent>;

// See //content/test/data/attribution_reporting/interop/README.md for the
// schema.

base::expected<AttributionSimulationEvents, std::string>
ParseAttributionInteropInput(base::Value::Dict input, base::Time offset_time);

struct AttributionInteropConfig {
  AttributionConfig attribution_config;
  double max_event_level_epsilon = 0;

  friend bool operator==(const AttributionInteropConfig&,
                         const AttributionInteropConfig&) = default;
};

base::expected<AttributionInteropConfig, std::string>
ParseAttributionInteropConfig(const base::Value::Dict&);

// Returns a non-empty string on failure.
[[nodiscard]] std::string MergeAttributionInteropConfig(
    const base::Value::Dict&,
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

  struct UnparsableRegistration {
    base::Time time;
    attribution_reporting::mojom::RegistrationType type;

    base::Value::Dict ToJson() const;

    friend bool operator==(const UnparsableRegistration&,
                           const UnparsableRegistration&) = default;
  };

  std::vector<Report> reports;
  std::vector<UnparsableRegistration> unparsable_registrations;

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

std::ostream& operator<<(
    std::ostream&,
    const AttributionInteropOutput::UnparsableRegistration&);

std::ostream& operator<<(std::ostream&, const AttributionInteropOutput&);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTEROP_PARSER_H_
