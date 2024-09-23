// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_MANAGER_IMPL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_MANAGER_IMPL_H_

#include <cstdint>
#include <list>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// An implementation of InterestGroupManagerImpl for tests. It tracks a number
// of calls to InterestGroupManagerImpl. Its EnqueueReports() overload uses
// in-memory storage to track reports, rather than sending real network
// requests. Other calls that are tracked do not block calls to the underlying
// InterestGroupManagerImpl.
class TestInterestGroupManagerImpl
    : public InterestGroupManagerImpl,
      public InterestGroupManagerImpl::InterestGroupObserver,
      public KAnonymityServiceDelegate {
 public:
  // Information about a report queued by an EnqueueReports() call. Doesn't
  // include values that are passed to the TestInterestGroupManagerImpl()
  // constructor, as those shouldn't vary between reports in a single test.
  struct Report {
    bool operator==(const Report& other) const {
      return report_type == other.report_type && report_url == other.report_url;
    }

    ReportType report_type;
    GURL report_url;
  };

  // The passed in expected values are compared against the arguments passed to
  // EnqueueReports. They should be the same for all reports from a particular
  // frame, so shouldn't vary in any unit test.
  TestInterestGroupManagerImpl(
      const url::Origin& expected_frame_origin,
      network::mojom::ClientSecurityStatePtr expected_client_security_state,
      scoped_refptr<network::SharedURLLoaderFactory>
          expected_url_loader_factory);

  ~TestInterestGroupManagerImpl() override;

  // InterestGroupManagerImpl implementation:
  void EnqueueReports(
      ReportType report_type,
      std::vector<GURL> report_urls,
      FrameTreeNodeId frame_tree_node_id,
      const url::Origin& frame_origin,
      const network::mojom::ClientSecurityState& client_security_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  // InterestGroupManagerImpl implementation:
  void EnqueueRealTimeReports(
      std::map<url::Origin, RealTimeReportingContributions> contributions,
      AdAuctionPageDataCallback ad_auction_page_data_callback,
      FrameTreeNodeId frame_tree_node_id,
      const url::Origin& frame_origin,
      const network::mojom::ClientSecurityState& client_security_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  // InterestGroupManagerImpl::InterestGroupObserver implementation:
  //
  // This is used instead of a virtual method for tracking bids, since it has
  // all the information that's needed.
  void OnInterestGroupAccessed(
      base::optional_ref<const std::string> devtools_auction_id,
      base::Time access_time,
      AccessType type,
      const url::Origin& owner_origin,
      const std::string& name,
      base::optional_ref<const url::Origin> component_seller_origin,
      std::optional<double> bid,
      base::optional_ref<const std::string> bid_currency) override;

  // KAnonymityServiceDelegate implementation:
  void JoinSet(std::string id,
               base::OnceCallback<void(bool)> callback) override;
  void QuerySets(std::vector<std::string> ids,
                 base::OnceCallback<void(std::vector<bool>)> callback) override;
  base::TimeDelta GetJoinInterval() override;
  base::TimeDelta GetQueryInterval() override;

  // Clears all logged data. Does not affect state of the interest group
  // database.
  void ClearLoggedData();

  // Expect that reports queued by EnqueueReports() matches `expected_reports`.
  // Does not check report order. All queued reports are cleared after the
  // comparison.
  void ExpectReports(const std::vector<Report>& expected_reports);

  void set_use_real_enqueue_reports(bool use_parents_enqueue);

  // Alternate way of validating URLs. Returns all the URLs of the requested
  // type, removing them from the internal list in the process.
  std::vector<GURL> TakeReportUrlsOfType(ReportType report_type);

  // Returns all real time reporting contributions. All contributions are
  // cleared afterwards.
  std::map<url::Origin, RealTimeReportingContributions>
  TakeRealTimeContributions();

  // Returns all interest groups that bid, removing them from the internal list
  // in the process. This is based on observer events, not database ones.
  std::vector<blink::InterestGroupKey> TakeInterestGroupsThatBid();

  // Returns all K-anon sets that have been joined, removing them from the
  // internal list in the process. Note that joining k-anon sets involves
  // a couple thread hops, including one off thread, so the caller should
  // run all messages loops until idle (not just until the current one is idle)
  // to make sure that all k-anon set joins have been processed.
  std::vector<std::string> TakeJoinedKAnonSets();

  // Retrieves the specified interest group if it exists, spinning a RunLoop
  // until the group is retrieved.
  std::optional<SingleStorageInterestGroup> BlockingGetInterestGroup(
      const url::Origin& owner,
      const std::string& name);

 private:
  const url::Origin expected_frame_origin_;
  const network::mojom::ClientSecurityStatePtr expected_client_security_state_;
  const scoped_refptr<network::SharedURLLoaderFactory>
      expected_url_loader_factory_;
  bool use_real_enqueue_reports_ = false;

  std::list<Report> reports_;
  std::map<url::Origin, RealTimeReportingContributions>
      real_time_contributions_;
  std::vector<blink::InterestGroupKey> interest_groups_that_bid_;
  std::vector<std::string> joined_k_anon_sets_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_MANAGER_IMPL_H_
