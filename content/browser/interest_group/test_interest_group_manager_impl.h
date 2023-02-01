// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_MANAGER_IMPL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_TEST_INTEREST_GROUP_MANAGER_IMPL_H_

#include <list>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// An implementation of InterestGroupManagerImpl for tests. It tracks reports
// queued by EnqueueReports(). It uses in-memory storage only, and does not send
// any requests.
class TestInterestGroupManagerImpl : public InterestGroupManagerImpl {
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

  void EnqueueReports(
      ReportType report_type,
      std::vector<GURL> report_urls,
      const url::Origin& frame_origin,
      const network::mojom::ClientSecurityState& client_security_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  // Expect that reports queued by EnqueueReports() matches `expected_reports`.
  // Does not check report order. All queued reports are cleared after the
  // comparison. Destructor logs a test failure if reports were queued after the
  // last ExpectReports() call.
  void ExpectReports(const std::vector<Report>& expected_reports);

  // Alternate way of validating URLs. Returns all the URLs of the requested
  // type, removing them from internals list of reports in the process.
  std::vector<GURL> TakeReportUrlsOfType(ReportType report_type);

 private:
  const url::Origin expected_frame_origin_;
  const network::mojom::ClientSecurityStatePtr expected_client_security_state_;
  const scoped_refptr<network::SharedURLLoaderFactory>
      expected_url_loader_factory_;

  std::list<Report> reports_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_MANAGER_IMPL_H_
