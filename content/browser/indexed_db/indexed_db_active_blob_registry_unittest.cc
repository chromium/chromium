// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "content/browser/indexed_db/indexed_db_active_blob_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

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
  int64_t blob_key;

  bool operator<(const UnusedBlob& other) const {
    if (database_id == other.database_id)
      return blob_key < other.blob_key;
    return database_id < other.database_id;
  }
};

void ReportUnusedBlob(std::set<UnusedBlob>* unused_blob_records,
                      int64_t database_id,
                      int64_t blob_key) {
  unused_blob_records->insert({database_id, blob_key});
}

// Base class for our test fixtures.
class IndexedDBActiveBlobRegistryTest : public testing::Test {
 public:
  typedef IndexedDBBlobInfo::ReleaseCallback ReleaseCallback;

  static const int64_t kDatabaseId0 = 7;
  static const int64_t kDatabaseId1 = 12;
  static const int64_t kBlobKey0 = 77;
  static const int64_t kBlobKey1 = 14;

  IndexedDBActiveBlobRegistryTest()
      : registry_(std::make_unique<IndexedDBActiveBlobRegistry>(
            base::BindRepeating(ReportOutstandingBlobs,
                                &report_outstanding_state_),
            base::BindRepeating(&ReportUnusedBlob, &unused_blobs_))) {}

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  IndexedDBActiveBlobRegistry* registry() const { return registry_.get(); }

 protected:
  ReportOutstandingState report_outstanding_state_;
  std::set<UnusedBlob> unused_blobs_;

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<IndexedDBActiveBlobRegistry> registry_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBActiveBlobRegistryTest);
};

TEST_F(IndexedDBActiveBlobRegistryTest, DeleteUnused) {
  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());

  EXPECT_FALSE(registry()->MarkDeletedCheckIfUsed(kDatabaseId0, kBlobKey0));
  RunUntilIdle();

  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());
}

TEST_F(IndexedDBActiveBlobRegistryTest, SimpleUse) {
  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());

  base::Closure add_ref =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey0);
  ReleaseCallback release =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey0);
  std::move(add_ref).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  std::move(release).Run(base::FilePath());
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(1, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());
}

TEST_F(IndexedDBActiveBlobRegistryTest, DeleteWhileInUse) {
  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());

  base::Closure add_ref =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey0);
  ReleaseCallback release =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey0);

  std::move(add_ref).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  EXPECT_TRUE(registry()->MarkDeletedCheckIfUsed(kDatabaseId0, kBlobKey0));
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  std::move(release).Run(base::FilePath());
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(1, report_outstanding_state_.false_calls);
  UnusedBlob unused_blob = {kDatabaseId0, kBlobKey0};
  EXPECT_EQ(1u, unused_blobs_.size());
  EXPECT_TRUE(base::Contains(unused_blobs_, unused_blob));
}

TEST_F(IndexedDBActiveBlobRegistryTest, MultipleBlobs) {
  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());

  base::Closure add_ref_00 =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey0);
  ReleaseCallback release_00 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey0);
  base::Closure add_ref_01 =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey1);
  ReleaseCallback release_01 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey1);
  base::Closure add_ref_10 =
      registry()->GetAddBlobRefCallback(kDatabaseId1, kBlobKey0);
  ReleaseCallback release_10 =
      registry()->GetFinalReleaseCallback(kDatabaseId1, kBlobKey0);
  base::Closure add_ref_11 =
      registry()->GetAddBlobRefCallback(kDatabaseId1, kBlobKey1);
  ReleaseCallback release_11 =
      registry()->GetFinalReleaseCallback(kDatabaseId1, kBlobKey1);

  std::move(add_ref_00).Run();
  std::move(add_ref_01).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  std::move(release_00).Run(base::FilePath());
  std::move(add_ref_10).Run();
  std::move(add_ref_11).Run();
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  EXPECT_TRUE(registry()->MarkDeletedCheckIfUsed(kDatabaseId0, kBlobKey1));
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());

  std::move(release_01).Run(base::FilePath());
  std::move(release_11).Run(base::FilePath());
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  UnusedBlob unused_blob = {kDatabaseId0, kBlobKey1};
  EXPECT_TRUE(base::Contains(unused_blobs_, unused_blob));
  EXPECT_EQ(1u, unused_blobs_.size());

  std::move(release_10).Run(base::FilePath());
  RunUntilIdle();

  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(1, report_outstanding_state_.false_calls);
  unused_blob = {kDatabaseId0, kBlobKey1};
  EXPECT_TRUE(base::Contains(unused_blobs_, unused_blob));
  EXPECT_EQ(1u, unused_blobs_.size());
}

TEST_F(IndexedDBActiveBlobRegistryTest, ForceShutdown) {
  EXPECT_TRUE(report_outstanding_state_.no_calls());
  EXPECT_TRUE(unused_blobs_.empty());

  base::Closure add_ref_0 =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey0);
  ReleaseCallback release_0 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey0);
  base::Closure add_ref_1 =
      registry()->GetAddBlobRefCallback(kDatabaseId0, kBlobKey1);
  ReleaseCallback release_1 =
      registry()->GetFinalReleaseCallback(kDatabaseId0, kBlobKey1);

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

  std::move(release_0).Run(base::FilePath());
  std::move(release_1).Run(base::FilePath());
  RunUntilIdle();

  // Nothing changes.
  EXPECT_EQ(1, report_outstanding_state_.true_calls);
  EXPECT_EQ(0, report_outstanding_state_.false_calls);
  EXPECT_TRUE(unused_blobs_.empty());
}

}  // namespace

}  // namespace content
