// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_BROWSING_DATA_REMOVER_DELEGATE_H_
#define CONTENT_PUBLIC_TEST_MOCK_BROWSING_DATA_REMOVER_DELEGATE_H_

#include <list>
#include <memory>

#include "base/time/time.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover_delegate.h"

namespace content {

class StoragePartition;

// A BrowsingDataRemoverDelegate that only records RemoveEmbedderData() calls.
class MockBrowsingDataRemoverDelegate : public BrowsingDataRemoverDelegate {
 public:
  MockBrowsingDataRemoverDelegate();
  ~MockBrowsingDataRemoverDelegate() override;

  // BrowsingDataRemoverDelegate:
  BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher GetOriginTypeMatcher()
      override;
  bool MayRemoveDownloadHistory() override;
  std::vector<std::string> GetDomainsForDeferredCookieDeletion(
      StoragePartition* storage_partition,
      uint64_t remove_mask) override;
  void RemoveEmbedderData(const base::Time& delete_begin,
                          const base::Time& delete_end,
                          uint64_t remove_mask,
                          BrowsingDataFilterBuilder* filter_builder,
                          uint64_t origin_type_mask,
                          base::OnceCallback<void(uint64_t)> callback) override;

  // Add an expected call for testing.
  void ExpectCall(const base::Time& delete_begin,
                  const base::Time& delete_end,
                  uint64_t remove_mask,
                  uint64_t origin_type_mask,
                  BrowsingDataFilterBuilder* filter_builder);

  // Add an expected call that doesn't have to match the filter_builder.
  void ExpectCallDontCareAboutFilterBuilder(const base::Time& delete_begin,
                                            const base::Time& delete_end,
                                            uint64_t remove_mask,
                                            uint64_t origin_type_mask);

  // Verify that expected and actual calls match.
  void VerifyAndClearExpectations();

 private:
  class CallParameters {
   public:
    CallParameters(const base::Time& delete_begin,
                   const base::Time& delete_end,
                   uint64_t remove_mask,
                   uint64_t origin_type_mask,
                   std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
                   bool should_compare_filter);
    ~CallParameters();

    bool operator==(const CallParameters& other) const;

   private:
    friend std::ostream& operator<<(std::ostream& os,
                                    const CallParameters& params);

    base::Time delete_begin_;
    base::Time delete_end_;
    uint64_t remove_mask_;
    uint64_t origin_type_mask_;
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

#endif  // CONTENT_PUBLIC_TEST_MOCK_BROWSING_DATA_REMOVER_DELEGATE_H_
