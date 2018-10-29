// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_registration_notifier.h"

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace content {
namespace {

const char kDeveloperId[] = "my-fetch";
const char kPrimaryUniqueId[] = "7e57ab1e-c0de-a150-ca75-1e75f005ba11";
const char kSecondaryUniqueId[] = "bb48a9fb-c21f-4c2d-a9ae-58bd48a9fb53";

constexpr uint64_t kDownloadTotal = 1;
constexpr uint64_t kDownloaded = 2;

class TestRegistrationObserver
    : public blink::mojom::BackgroundFetchRegistrationObserver {
 public:
  struct ProgressUpdate {
    ProgressUpdate(uint64_t upload_total,
                   uint64_t uploaded,
                   uint64_t download_total,
                   uint64_t downloaded,
                   blink::mojom::BackgroundFetchResult result,
                   blink::mojom::BackgroundFetchFailureReason failure_reason)
        : upload_total(upload_total),
          uploaded(uploaded),
          download_total(download_total),
          downloaded(downloaded),
          result(result),
          failure_reason(failure_reason) {}

    uint64_t upload_total = 0;
    uint64_t uploaded = 0;
    uint64_t download_total = 0;
    uint64_t downloaded = 0;
    blink::mojom::BackgroundFetchResult result =
        blink::mojom::BackgroundFetchResult::UNSET;
    blink::mojom::BackgroundFetchFailureReason failure_reason =
        blink::mojom::BackgroundFetchFailureReason::NONE;
  };

  TestRegistrationObserver() : binding_(this) {}
  ~TestRegistrationObserver() override = default;

  // Closes the bindings associated with this observer.
  void Close() { binding_.Close(); }

  // Returns an InterfacePtr to this observer.
  blink::mojom::BackgroundFetchRegistrationObserverPtr GetPtr() {
    blink::mojom::BackgroundFetchRegistrationObserverPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // Returns the vector of progress updates received by this observer.
  const std::vector<ProgressUpdate>& progress_updates() const {
    return progress_updates_;
  }

  bool records_available() const { return records_available_; }

  // blink::mojom::BackgroundFetchRegistrationObserver implementation.
  void OnProgress(
      uint64_t upload_total,
      uint64_t uploaded,
      uint64_t download_total,
      uint64_t downloaded,
      blink::mojom::BackgroundFetchResult result,
      blink::mojom::BackgroundFetchFailureReason failure_reason) override {
    progress_updates_.emplace_back(upload_total, uploaded, download_total,
                                   downloaded, result, failure_reason);
  }

  void OnRecordsUnavailable() override { records_available_ = false; }

 private:
  std::vector<ProgressUpdate> progress_updates_;
  mojo::Binding<blink::mojom::BackgroundFetchRegistrationObserver> binding_;
  bool records_available_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestRegistrationObserver);
};

class BackgroundFetchRegistrationNotifierTest : public ::testing::Test {
 public:
  BackgroundFetchRegistrationNotifierTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        handle_(task_runner_),
        notifier_(std::make_unique<BackgroundFetchRegistrationNotifier>()) {}

  ~BackgroundFetchRegistrationNotifierTest() override = default;

  // Notifies all observers for the |unique_id| of the made progress, and waits
  // until the task runner managing the Mojo connection has finished.
  void Notify(const BackgroundFetchRegistration& registration) {
    notifier_->Notify(registration);
    task_runner_->RunUntilIdle();
  }

  void NotifyRecordsUnavailable(const std::string& unique_id) {
    notifier_->NotifyRecordsUnavailable(unique_id);
    task_runner_->RunUntilIdle();
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle handle_;

  std::unique_ptr<BackgroundFetchRegistrationNotifier> notifier_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchRegistrationNotifierTest);
};

TEST_F(BackgroundFetchRegistrationNotifierTest, NotifySingleObserver) {
  auto observer = std::make_unique<TestRegistrationObserver>();

  notifier_->AddObserver(kPrimaryUniqueId, observer->GetPtr());
  ASSERT_EQ(observer->progress_updates().size(), 0u);

  Notify(BackgroundFetchRegistration(
      kDeveloperId, kPrimaryUniqueId,
      /* upload_total*/ 0, /* uploaded*/ 0, kDownloadTotal, kDownloaded,
      blink::mojom::BackgroundFetchResult::UNSET,
      blink::mojom::BackgroundFetchFailureReason::NONE));

  ASSERT_EQ(observer->progress_updates().size(), 1u);

  auto& update = observer->progress_updates()[0];
  // TODO(crbug.com/774054): Uploads are not yet supported.
  EXPECT_EQ(update.upload_total, 0u);
  EXPECT_EQ(update.uploaded, 0u);
  EXPECT_EQ(update.download_total, kDownloadTotal);
  EXPECT_EQ(update.downloaded, kDownloaded);
  EXPECT_EQ(update.result, blink::mojom::BackgroundFetchResult::UNSET);
  EXPECT_EQ(update.failure_reason,
            blink::mojom::BackgroundFetchFailureReason::NONE);
}

TEST_F(BackgroundFetchRegistrationNotifierTest, NotifyMultipleObservers) {
  std::vector<std::unique_ptr<TestRegistrationObserver>> primary_observers;
  primary_observers.push_back(std::make_unique<TestRegistrationObserver>());
  primary_observers.push_back(std::make_unique<TestRegistrationObserver>());
  primary_observers.push_back(std::make_unique<TestRegistrationObserver>());

  auto secondary_observer = std::make_unique<TestRegistrationObserver>();

  for (auto& observer : primary_observers) {
    notifier_->AddObserver(kPrimaryUniqueId, observer->GetPtr());
    ASSERT_EQ(observer->progress_updates().size(), 0u);
  }

  notifier_->AddObserver(kSecondaryUniqueId, secondary_observer->GetPtr());
  ASSERT_EQ(secondary_observer->progress_updates().size(), 0u);

  // Notify the |kPrimaryUniqueId|.
  Notify(BackgroundFetchRegistration(
      kDeveloperId, kPrimaryUniqueId,
      /* upload_total*/ 0, /* uploaded*/ 0, kDownloadTotal, kDownloaded,
      blink::mojom::BackgroundFetchResult::UNSET,
      blink::mojom::BackgroundFetchFailureReason::NONE));

  for (auto& observer : primary_observers) {
    ASSERT_EQ(observer->progress_updates().size(), 1u);

    auto& update = observer->progress_updates()[0];
    // TODO(crbug.com/774054): Uploads are not yet supported.
    EXPECT_EQ(update.upload_total, 0u);
    EXPECT_EQ(update.uploaded, 0u);
    EXPECT_EQ(update.download_total, kDownloadTotal);
    EXPECT_EQ(update.downloaded, kDownloaded);
    EXPECT_EQ(update.result, blink::mojom::BackgroundFetchResult::UNSET);
    EXPECT_EQ(update.failure_reason,
              blink::mojom::BackgroundFetchFailureReason::NONE);
  }

  // The observer for |kSecondaryUniqueId| should not have been notified.
  ASSERT_EQ(secondary_observer->progress_updates().size(), 0u);
}

TEST_F(BackgroundFetchRegistrationNotifierTest,
       NotifyFollowingObserverInitiatedRemoval) {
  auto observer = std::make_unique<TestRegistrationObserver>();

  notifier_->AddObserver(kPrimaryUniqueId, observer->GetPtr());
  ASSERT_EQ(observer->progress_updates().size(), 0u);

  Notify(BackgroundFetchRegistration(
      kDeveloperId, kPrimaryUniqueId,
      /* upload_total*/ 0, /* uploaded*/ 0, kDownloadTotal, kDownloaded,
      blink::mojom::BackgroundFetchResult::UNSET,
      blink::mojom::BackgroundFetchFailureReason::NONE));

  ASSERT_EQ(observer->progress_updates().size(), 1u);

  // Closes the binding as would be done from the renderer process.
  observer->Close();

  Notify(BackgroundFetchRegistration(
      kDeveloperId, kPrimaryUniqueId,
      /* upload_total*/ 0, /* uploaded*/ 0, kDownloadTotal, kDownloaded,
      blink::mojom::BackgroundFetchResult::UNSET,
      blink::mojom::BackgroundFetchFailureReason::NONE));

  // The observers for |kPrimaryUniqueId| were removed, so no second update
  // should have been received by the |observer|.
  ASSERT_EQ(observer->progress_updates().size(), 1u);
}

TEST_F(BackgroundFetchRegistrationNotifierTest, NotifyWithoutObservers) {
  auto observer = std::make_unique<TestRegistrationObserver>();

  notifier_->AddObserver(kPrimaryUniqueId, observer->GetPtr());
  ASSERT_EQ(observer->progress_updates().size(), 0u);

  Notify(BackgroundFetchRegistration(
      kDeveloperId, kSecondaryUniqueId,
      /* upload_total*/ 0, /* uploaded*/ 0, kDownloadTotal, kDownloaded,
      blink::mojom::BackgroundFetchResult::UNSET,
      blink::mojom::BackgroundFetchFailureReason::NONE));

  // Because the notification was for |kSecondaryUniqueId|, no progress updates
  // should be received by the |observer|.
  EXPECT_EQ(observer->progress_updates().size(), 0u);
}

TEST_F(BackgroundFetchRegistrationNotifierTest, NotifyRecordsUnavailable) {
  auto observer = std::make_unique<TestRegistrationObserver>();

  notifier_->AddObserver(kPrimaryUniqueId, observer->GetPtr());
  ASSERT_TRUE(observer->records_available());

  NotifyRecordsUnavailable(kPrimaryUniqueId);
  ASSERT_FALSE(observer->records_available());
}

}  // namespace
}  // namespace content
