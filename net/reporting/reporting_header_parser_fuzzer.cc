// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_header_parser.h"
#include "net/reporting/reporting_policy.pb.h"
#include "net/reporting/reporting_test_util.h"
#include "url/gurl.h"

#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

// Silence logging from the protobuf library.
protobuf_mutator::protobuf::LogSilencer log_silencer;

// TODO: consider including proto definition for URL after moving that to
// testing/libfuzzer/proto and creating a separate converter.
const GURL kUrl_ = GURL("https://origin/path");

namespace net_reporting_header_parser_fuzzer {

void FuzzReportingHeaderParser(const std::string& data_json,
                               const net::ReportingPolicy& policy) {
  net::TestReportingContext context(base::DefaultClock::GetInstance(),
                                    base::DefaultTickClock::GetInstance(),
                                    policy);
  // Emulate what ReportingService::OnHeader does before calling
  // ReportingHeaderParser::ParseHeader.
  std::unique_ptr<base::Value> data_value =
      base::JSONReader::ReadDeprecated("[" + data_json + "]");
  if (!data_value)
    return;

  net::ReportingHeaderParser::ParseHeader(&context, kUrl_,
                                          std::move(data_value));
  if (context.cache()->GetEndpointCount() == 0) {
    return;
  }
}

void InitializeReportingPolicy(
    net::ReportingPolicy& policy,
    const net_reporting_policy_proto::ReportingPolicy& policy_data) {
  policy.max_report_count = policy_data.max_report_count();
  policy.max_endpoint_count = policy_data.max_endpoint_count();
  policy.delivery_interval =
      base::TimeDelta::FromMicroseconds(policy_data.delivery_interval_us());
  policy.persistence_interval =
      base::TimeDelta::FromMicroseconds(policy_data.persistence_interval_us());
  policy.persist_reports_across_restarts =
      policy_data.persist_reports_across_restarts();
  policy.persist_clients_across_restarts =
      policy_data.persist_clients_across_restarts();
  policy.garbage_collection_interval = base::TimeDelta::FromMicroseconds(
      policy_data.garbage_collection_interval_us());
  policy.max_report_age =
      base::TimeDelta::FromMicroseconds(policy_data.max_report_age_us());
  policy.max_report_attempts = policy_data.max_report_attempts();
  policy.persist_reports_across_network_changes =
      policy_data.persist_reports_across_network_changes();
  policy.persist_clients_across_network_changes =
      policy_data.persist_clients_across_network_changes();
  if (policy_data.has_max_endpoints_per_origin())
    policy.max_endpoints_per_origin = policy_data.max_endpoints_per_origin();
  if (policy_data.has_max_group_staleness_us()) {
    policy.max_group_staleness =
        base::TimeDelta::FromMicroseconds(policy_data.max_report_age_us());
  }
}

DEFINE_BINARY_PROTO_FUZZER(
    const net_reporting_policy_proto::ReportingHeaderParserFuzzInput& input) {
  net::ReportingPolicy policy;
  InitializeReportingPolicy(policy, input.policy());

  json_proto::JsonProtoConverter converter;
  auto data = converter.Convert(input.headers());

  FuzzReportingHeaderParser(data, policy);
}

}  // namespace net_reporting_header_parser_fuzzer
