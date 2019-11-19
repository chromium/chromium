// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_REMOVER_DELEGATE_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_REMOVER_DELEGATE_H_

#include <list>
#include <memory>

#include "base/time/time.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover_delegate.h"

namespace content {

// A BrowsingDataRemoverDelegate that only records RemoveEmbedderData() calls.
class MockBrowsingDataRemoverDelegate : public BrowsingDataRemoverDelegate {
 public:
  MockBrowsingDataRemoverDelegate();
  ~MockBrowsingDataRemoverDelegate() override;

  // BrowsingDataRemoverDelegate:
  BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher GetOriginTypeMatcher()
      override;
  bool MayRemoveDownloadHistory() override;
  void RemoveEmbedderData(const base::Time& delete_begin,
                          const base::Time& delete_end,
                          int remove_mask,
                          BrowsingDataFilterBuilder* filter_builder,
                          int origin_type_mask,
                          base::OnceClosure callback) override;

  // Add an expected call for testing.
  void ExpectCall(const base::Time& delete_begin,
                  const base::Time& delete_end,
                  int remove_mask,
                  int origin_type_mask,
                  BrowsingDataFilterBuilder* filter_builder);

  // Add an expected call that doesn't have to match the filter_builder.
  void ExpectCallDontCareAboutFilterBuilder(const base::Time& delete_begin,
                                            const base::Time& delete_end,
                                            int remove_mask,
                                            int origin_type_mask);

  // Verify that expected and actual calls match.
  void VerifyAndClearExpectations();

 private:
  class CallParameters {
   public:
    CallParameters(const base::Time& delete_begin,
                   const base::Time& delete_end,
                   int remove_mask,
                   int origin_type_mask,
                   std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
                   bool should_compare_filter);
    ~CallParameters();

    bool operator==(const CallParameters& other) const;

   private:
    friend std::ostream& operator<<(std::ostream& os,
                                    const CallParameters& params);

    base::Time delete_begin_;
    base::Time delete_end_;
    int remove_mask_;
    int origin_type_mask_;
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_;
    bool should_compare_filter_;
  };
  friend std::ostream& operator<<(std::ostream& os,
                                  const CallParameters& params);

  std::list<CallParameters> actual_calls_;
  std::list<CallParameters> expected_calls_;
};

std::ostream& operator<<(
    std::ostream& os,
    const MockBrowsingDataRemoverDelegate::CallParameters& params);

}  // content

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_REMOVER_DELEGATE_H_
