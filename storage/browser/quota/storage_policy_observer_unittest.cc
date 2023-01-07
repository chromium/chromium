// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/storage_policy_observer.h"

#include "base/test/task_environment.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace storage {
namespace {

constexpr char kOrigin1[] = "http://origin1.com";
constexpr char kOrigin2[] = "http://origin2.com";
constexpr char kSessionOrigin1[] = "http://session-origin1.com";
constexpr char kSessionOrigin2[] = "http://session-origin2.com";

class StoragePolicyObserverTest : public testing::Test {
 public:
  StoragePolicyObserverTest()
      : mock_policy_(base::MakeRefCounted<MockSpecialStoragePolicy>()),
        observer_(std::make_unique<StoragePolicyObserver>(
            base::BindRepeating(&StoragePolicyObserverTest::OnPolicyUpdates,
                                base::Unretained(this)),
            task_environment_.GetMainThreadTaskRunner(),
            mock_policy_)) {
    mock_policy_->AddSessionOnly(GURL(kSessionOrigin1));
    mock_policy_->AddSessionOnly(GURL(kSessionOrigin2));
    // Make sure the IO thread observer is created.
    task_environment_.RunUntilIdle();
  }

  ~StoragePolicyObserverTest() override {
    observer_.reset();
    // Make sure the IO thread observer is destroyed.
    task_environment_.RunUntilIdle();
  }

 protected:
  void OnPolicyUpdates(
      std::vector<storage::mojom::StoragePolicyUpdatePtr> updates) {
    ASSERT_TRUE(latest_updates_.empty());
    latest_updates_ = std::move(updates);
  }

  std::vector<storage::mojom::StoragePolicyUpdatePtr> TakeLatestUpdates() {
    return std::move(latest_updates_);
  }

  void ExpectUpdates(std::map<url::Origin, bool> expectations) {
    auto updates = TakeLatestUpdates();
    EXPECT_EQ(updates.size(), expectations.size());
    for (const auto& update : updates) {
      auto it = expectations.find(update->origin);
      ASSERT_NE(it, expectations.end());
      EXPECT_EQ(it->second, update->purge_on_shutdown);
    }
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockSpecialStoragePolicy> mock_policy_;
  std::unique_ptr<StoragePolicyObserver> observer_;

 private:
  std::vector<storage::mojom::StoragePolicyUpdatePtr> latest_updates_;
};

TEST_F(StoragePolicyObserverTest, NonSessionOnlyOriginDoesNotCreateUpdate) {
  observer_->StartTrackingOrigins({url::Origin::Create(GURL(kOrigin1)),
                                   url::Origin::Create(GURL(kOrigin2))});
  auto updates = TakeLatestUpdates();
  EXPECT_TRUE(updates.empty());
}

TEST_F(StoragePolicyObserverTest, SessionOnlyOriginCreateUpdate) {
  url::Origin origin1 = url::Origin::Create(GURL(kSessionOrigin1));
  observer_->StartTrackingOrigin(origin1);
  ExpectUpdates({{origin1, true}});

  url::Origin origin2 = url::Origin::Create(GURL(kSessionOrigin2));
  observer_->StartTrackingOrigin(origin2);
  ExpectUpdates({{origin2, true}});

  mock_policy_->RemoveSessionOnly(GURL(kSessionOrigin1));
  mock_policy_->RemoveSessionOnly(GURL(kSessionOrigin2));
  observer_->OnPolicyChanged();
  ExpectUpdates({{origin1, false}, {origin2, false}});
}

}  // namespace
}  // namespace storage
