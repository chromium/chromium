#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_PARTITION_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_PARTITION_H_

#include "components/attribution_reporting/attribution_window.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"

namespace content {


// Groups together possibly many epochs
class CONTENT_EXPORT Partition {
 public:

  struct CONTENT_EXPORT ReportValuePair {
    ReportValuePair();
    ReportValuePair(const ReportValuePair&);
    ReportValuePair& operator=(const ReportValuePair&);
    ReportValuePair(ReportValuePair&&);
    ReportValuePair& operator=(ReportValuePair&&);
    ~ReportValuePair();

    std::vector<AggregatableHistogramContribution> report;
    double value;
  };

  Partition(attribution_reporting::AttributionWindow attribution_window,
            std::string attribution_logic);

  ~Partition();

  Partition(const Partition&);
  Partition(Partition&&);

  double compute_sensitivity(const char* sensitivity_metric);

  void null_report();


  const attribution_reporting::AttributionWindow attribution_window;
  const std::string attribution_logic;
  base::flat_map<std::string, ReportValuePair> report_value_pairs;
  base::flat_map<uint64_t, std::vector<StoredSource*>> sources_per_epoch;
  std::optional<StoredSource*> logging_source;

};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_PARTITION_H_
