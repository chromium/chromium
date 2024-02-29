
#include "content/browser/attribution_reporting/partition.h"

#include "components/attribution_reporting/attribution_window.h"
#include "content/browser/attribution_reporting/stored_source.h"



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

}  // namespace content
