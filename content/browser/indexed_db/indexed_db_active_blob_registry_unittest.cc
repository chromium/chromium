// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <set>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "content/browser/indexed_db/indexed_db_active_blob_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

struct ReportOutstandingState {
  int true_calls = 0;
  int false_calls = 0;

  bool no_calls() { return true_calls == 0 && false_calls == 0; }
};

void ReportOutstandingBlobs(ReportOutstandingState* state,
                            bool blobs_outstanding) {
  if (blobs_outstanding) {
    ++state->true_calls;
  } else {
    ++state->false_calls;
  }
}

struct UnusedBlob {
  int64_t database_id;
  int64_t blob_number;

  bool operator<(const UnusedBlob& other) const {
    if (database_id == other.database_id)
      return blob_number < other.blob_number;
    return database_id < other.database_id;
  }
};

void ReportUnusedBlob(std::set<UnusedBlob>* unused_blob_records,
                      int64_t database_id,
                      int64_t blob_number) {
  unused_blob_records->insert({database_id, blob_number});
}

// Base class for our test fixtures.
class IndexedDBActiveBlobRegistryTest : public testing::Test {
 public:
  static const int64_t kDatabaseId0 = 7;
  static const int64_t kDatabaseId1 = 12;
  static const int64_t kBlobNumber0 = 77;
  static const int64_t kBlobNumber1 = 14;

  IndexedDBActiveBlobRegistryTest()
      : registry_(std::make_unique<IndexedDBActiveBlobRegistry>(
            base::BindRepeating(ReportOutstandingBlobs,
                                &report_outstanding_state_),
            base::BindRepeating(&ReportUnusedBlob, &unused_blobs_))) {}

  IndexedDBActiveBlobRegistryTest(const IndexedDBActiveBlobRegistryTest&) =
      delete;
  IndexedDBActiveBlobRegistryTest& operator=(
      const IndexedDBActiveBlobRegistryTest&) = delete;

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  IndexedDBActiveBlobRegistry* registry() const { return registry_.get(); }

 protected:
  ReportOutstandingState report_outstanding_state_;
  std::set<UnusedBlob> unused_blobs_;

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<IndexedDBActiveBlobRegistry> registry_;
};

TEST_F(IndexedDBActiveBlobRegistryTest, DeleteUnused) {
  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());

  EXPECT_FALSE(registry()->MarkBlobInfoDeletedAndCheckIfReferenced(
      kDatabaseId0, kBlobNumber0));
  RunUntilIdle();

  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());
}

TEST_F(IndexedDBActiveBlobRegistryTest, SimpleUse) {
  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());

  auto add_ref =
      registry()->GetMarkBlobActiveCallback(kDatabaseId0, kBlobNumber0);
  auto release =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobNumber0);
  std::move(add_ref).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  std::move(release).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(1, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());
}

TEST_F(IndexedDBActiveBlobRegistryTest, DeleteWhileInUse) {
  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());

  auto add_ref =
      registry()->GetMarkBlobActiveCallback(kDatabaseId0, kBlobNumber0);
  auto release =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobNumber0);

  std::move(add_ref).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  EXPECT_TRUE(registry()->MarkBlobInfoDeletedAndCheckIfReferenced(
      kDatabaseId0, kBlobNumber0));
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  std::move(release).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(1, report_outstanding_state_.false_calls);
  UnusedBlob unused_blob = {kDatabaseId0, kBlobNumber0};
  EXPECT_EQ(1u, unused_blobs_.size());
  EXPECT_TRUE(base::Contains(unused_blobs_, unused_blob));
}

TEST_F(IndexedDBActiveBlobRegistryTest, MultipleBlobs) {
  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());

  auto add_ref_00 =
      registry()->GetMarkBlobActiveCallback(kDatabaseId0, kBlobNumber0);
  auto release_00 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobNumber0);
  auto add_ref_01 =
      registry()->GetMarkBlobActiveCallback(kDatabaseId0, kBlobNumber1);
  auto release_01 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobNumber1);
  auto add_ref_10 =
      registry()->GetMarkBlobActiveCallback(kDatabaseId1, kBlobNumber0);
  auto release_10 =
      registry()->GetFinalReleaseCallback(kDatabaseId1, kBlobNumber0);
  auto add_ref_11 =
      registry()->GetMarkBlobActiveCallback(kDatabaseId1, kBlobNumber1);
  auto release_11 =
      registry()->GetFinalReleaseCallback(kDatabaseId1, kBlobNumber1);

  std::move(add_ref_00).Run();
  std::move(add_ref_01).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  std::move(release_00).Run();
  std::move(add_ref_10).Run();
  std::move(add_ref_11).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  EXPECT_TRUE(registry()->MarkBlobInfoDeletedAndCheckIfReferenced(
      kDatabaseId0, kBlobNumber1));
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  std::move(release_01).Run();
  std::move(release_11).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  UnusedBlob unused_blob = {kDatabaseId0, kBlobNumber1};
  EXPECT_TRUE(base::Contains(unused_blobs_, unused_blob));
  EXPECT_EQ(1u, unused_blobs_.size());

  std::move(release_10).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(1, report_outstanding_state_.false_calls);
  unused_blob = {kDatabaseId0, kBlobNumber1};
  EXPECT_TRUE(base::Contains(unused_blobs_, unused_blob));
  EXPECT_EQ(1u, unused_blobs_.size());
}

TEST_F(IndexedDBActiveBlobRegistryTest, ForceShutdown) {
  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());

  auto add_ref_0 =
      registry()->GetMarkBlobActiveCallback(kDatabaseId0, kBlobNumber0);
  auto release_0 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobNumber0);
  auto add_ref_1 =
      registry()->GetMarkBlobActiveCallback(kDatabaseId0, kBlobNumber1);
  auto release_1 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobNumber1);

  std::move(add_ref_0).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  registry()->ForceShutdown();

  std::move(add_ref_1).Run();
  RunUntilIdle();

  // Nothing changes.
  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  std::move(release_0).Run();
  std::move(release_1).Run();
  RunUntilIdle();

  // Nothing changes.
  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());
}

}  // namespace

}  // namespace content
