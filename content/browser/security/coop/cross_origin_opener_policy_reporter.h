// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_OPENER_POLICY_REPORTER_H_
#define CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_OPENER_POLICY_REPORTER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom.h"
#include "services/network/public/mojom/source_location.mojom-forward.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;
class StoragePartition;
class RenderFrameHostImpl;

// Used to report (potential) COOP breakages.
// A CrossOriginOpenerPolicyReporter lives in the browser process and is either
// held by the NavigationRequest during navigation or by the RenderFrameHostImpl
// after the document has committed.
// To make calls from other processes, create a mojo endpoint using Clone and
// pass the receiver to other processes.
// Any functions other than the destructor must not be called after the
// associated StoragePartition is destructed.
class CONTENT_EXPORT CrossOriginOpenerPolicyReporter {
 public:
  CrossOriginOpenerPolicyReporter(
      StoragePartition* storage_partition,
      const GURL& context_url,
      const GURL& context_referrer_url,
      const network::CrossOriginOpenerPolicy& coop,
      const base::UnguessableToken& reporting_source,
      const net::NetworkAnonymizationKey& network_anonymization_key);
  ~CrossOriginOpenerPolicyReporter();
  CrossOriginOpenerPolicyReporter(const CrossOriginOpenerPolicyReporter&) =
      delete;
  CrossOriginOpenerPolicyReporter& operator=(
      const CrossOriginOpenerPolicyReporter&) = delete;

  void set_storage_partition(StoragePartition* storage_partition) {
    storage_partition_ = storage_partition;
  }
  void set_reporting_source(const base::UnguessableToken& reporting_source) {
    reporting_source_ = reporting_source;
  }

  // Returns the information necessary for the renderer process to report
  // accesses between the COOP document and other pages.
  network::mojom::CrossOriginOpenerPolicyReporterParamsPtr CreateReporterParams(
      bool access_from_coop_page,
      FrameTreeNode* accessing_node,
      FrameTreeNode* accessed_node);

  void QueueAccessReport(network::mojom::CoopAccessReportType report_type,
                         const std::string& property,
                         network::mojom::SourceLocationPtr source_location,
                         const std::string& reported_window_url,
                         const std::string& initial_popup_url) const;

  // Sends reports when COOP causing a browsing context group switch that
  // breaks opener relationships.
  void QueueNavigationToCOOPReport(const GURL& previous_url,
                                   bool same_origin_with_previous,
                                   bool is_report_only);
  void QueueNavigationAwayFromCOOPReport(const GURL& next_url,
                                         bool is_current_source,
                                         bool same_origin_with_next,
                                         bool is_report_only);

 private:
  void QueueNavigationReport(base::Value::Dict body,
                             const std::string& endpoint,
                             bool is_report_only);

  // See the class comment.
  raw_ptr<StoragePartition> storage_partition_;
  base::UnguessableToken reporting_source_;
  GURL source_url_;
  GlobalRenderFrameHostId source_routing_id_;
  const GURL context_url_;
  const std::string context_referrer_url_;
  const network::CrossOriginOpenerPolicy coop_;
  const net::NetworkAnonymizationKey network_anonymization_key_;

  mojo::UniqueReceiverSet<network::mojom::CrossOriginOpenerPolicyReporter>
      receiver_set_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_OPENER_POLICY_REPORTER_H_
