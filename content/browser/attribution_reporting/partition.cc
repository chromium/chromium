
#include "content/browser/attribution_reporting/partition.h"

#include "components/attribution_reporting/attribution_window.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"


namespace content {

Partition::ReportValuePair::ReportValuePair() {}

Partition::ReportValuePair::ReportValuePair(const ReportValuePair&) =
    default;

Partition::ReportValuePair& Partition::ReportValuePair::operator=(
    const ReportValuePair&) = default;

Partition::ReportValuePair::ReportValuePair(ReportValuePair&&) = default;

Partition::ReportValuePair& Partition::ReportValuePair::operator=(
    ReportValuePair&&) = default;

Partition::ReportValuePair::~ReportValuePair() = default;

Partition::Partition(
    attribution_reporting::AttributionWindow attribution_window, 
    std::string attribution_logic)
    : attribution_window(attribution_window),
      attribution_logic(attribution_logic) {}

Partition::~Partition() = default;

Partition::Partition(const Partition&) = default;

Partition::Partition(Partition&&) = default;

double Partition::compute_sensitivity(const char* sensitivity_metric) {
  double individual_sensitivity = 0;
  if (std::strcmp(sensitivity_metric, "L1") == 0) {
    for (auto& pair : report_value_pairs) {
      for (auto& bucket : pair.second.report) {
        individual_sensitivity += bucket.value();
      }
    }
    return individual_sensitivity;
  } else if (std::strcmp(sensitivity_metric, "L2") == 0) {
    // TODO(kelly)
    // return AggregatableResult::kInternalError;
  } else {
    // return AggregatableResult::kInternalError;
  }
  return -1;
}

void Partition::null_report() {
  for (auto& pair : report_value_pairs) {
    pair.second.report = {};
  }
}


}  // namespace content
