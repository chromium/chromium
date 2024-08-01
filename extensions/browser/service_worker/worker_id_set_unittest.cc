// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/service_worker/worker_id_set.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kIdA[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kIdB[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr char kIdC[] = "cccccccccccccccccccccccccccccccc";
constexpr char kIdD[] = "dddddddddddddddddddddddddddddddd";
constexpr char kIdE[] = "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
constexpr char kIdF[] = "ffffffffffffffffffffffffffffffff";
constexpr char kIdG[] = "gggggggggggggggggggggggggggggggg";
constexpr char kIdX[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

// A vector based implementation of WorkerIdSet.
// GetAllForExtension()/Contains() are O(n).
class VectorWorkerIdListImpl {
 public:
  explicit VectorWorkerIdListImpl(const std::vector<WorkerId> worker_ids)
      : workers_(worker_ids) {}

  VectorWorkerIdListImpl(const VectorWorkerIdListImpl&) = delete;
  VectorWorkerIdListImpl& operator=(const VectorWorkerIdListImpl&) = delete;

  ~VectorWorkerIdListImpl() = default;

  std::vector<WorkerId> GetAllForExtension(const ExtensionId& extension_id,
                                           int render_process_id) const {
    std::vector<WorkerId> matching_workers;
    for (const WorkerId& worker_id : workers_) {
      if (worker_id.extension_id == extension_id &&
          worker_id.render_process_id == render_process_id) {
        matching_workers.push_back(worker_id);
      }
    }
    return matching_workers;
  }

  bool Contains(const WorkerId& worker_id) const {
    return base::Contains(workers_, worker_id);
  }

 private:
  std::vector<WorkerId> workers_;
};

std::vector<WorkerId> GenerateWorkerIds(
    const std::vector<ExtensionId>& extension_ids,
    const std::vector<int>& render_process_ids,
    const std::vector<int64_t>& worker_version_ids,
    const std::vector<int>& worker_thread_ids) {
  std::vector<WorkerId> worker_ids;
  for (const ExtensionId& extension_id : extension_ids) {
    for (int render_process_id : render_process_ids) {
      for (int64_t worker_version_id : worker_version_ids) {
        for (int worker_thread_id : worker_thread_ids) {
          worker_ids.push_back(WorkerId({extension_id, render_process_id,
                                         worker_version_id, worker_thread_id}));
        }
      }
    }
  }
  return worker_ids;
}

}  // namespace

class WorkerIdSetTest : public ExtensionsTest {
 public:
  WorkerIdSetTest()
      : allow_multiple_workers_per_extension_(
            WorkerIdSet::AllowMultipleWorkersPerExtensionForTesting()) {}

  WorkerIdSetTest(const WorkerIdSetTest&) = delete;
  WorkerIdSetTest& operator=(const WorkerIdSetTest&) = delete;

  bool AreWorkerIdsEqual(const std::vector<WorkerId>& expected,
                         const std::vector<WorkerId>& actual) {
    if (expected.size() != actual.size()) {
      return false;
    }

    std::vector<WorkerId> expected_copy = expected;
    std::vector<WorkerId> actual_copy = actual;
    std::sort(expected_copy.begin(), expected_copy.end());
    std::sort(actual_copy.begin(), actual_copy.end());
    return expected_copy == actual_copy;
  }

  std::unique_ptr<WorkerIdSet> CreateWorkerIdSet(
      const std::vector<WorkerId>& worker_ids) {
    auto worker_id_set = std::make_unique<WorkerIdSet>();
    for (const WorkerId& worker_id : worker_ids) {
      worker_id_set->Add(worker_id, browser_context());
    }
    return worker_id_set;
  }

 private:
  base::AutoReset<bool> allow_multiple_workers_per_extension_;
};

TEST_F(WorkerIdSetTest, GetAllForExtension) {
  std::unique_ptr<WorkerIdSet> workers = CreateWorkerIdSet({{kIdA, 1, 100, 1},
                                                            {kIdA, 1, 101, 2},
                                                            {kIdA, 1, 102, 1},
                                                            {kIdA, 1, 103, 3},
                                                            {kIdB, 2, 100, 3},
                                                            {kIdB, 2, 100, 4},
                                                            {kIdC, 2, 110, 5},
                                                            {kIdA, 9, 100, 4}});

  EXPECT_TRUE(AreWorkerIdsEqual({{kIdA, 1, 100, 1},
                                 {kIdA, 1, 101, 2},
                                 {kIdA, 1, 102, 1},
                                 {kIdA, 1, 103, 3}},
                                workers->GetAllForExtension(kIdA, 1)));
  EXPECT_TRUE(AreWorkerIdsEqual({{kIdB, 2, 100, 4}, {kIdB, 2, 100, 3}},
                                workers->GetAllForExtension(kIdB, 2)));
  EXPECT_TRUE(AreWorkerIdsEqual({{kIdC, 2, 110, 5}},
                                workers->GetAllForExtension(kIdC, 2)));
  EXPECT_TRUE(AreWorkerIdsEqual({{kIdA, 9, 100, 4}},
                                workers->GetAllForExtension(kIdA, 9)));

  // No matches.
  EXPECT_TRUE(workers->GetAllForExtension(kIdX, 1).empty());
  EXPECT_TRUE(workers->GetAllForExtension(kIdB, 1).empty());
  EXPECT_TRUE(workers->GetAllForExtension(kIdA, 2).empty());
  EXPECT_TRUE(workers->GetAllForExtension(kIdX, 2).empty());
  EXPECT_TRUE(workers->GetAllForExtension(kIdB, 9).empty());
  EXPECT_TRUE(workers->GetAllForExtension(kIdC, 9).empty());
  EXPECT_TRUE(workers->GetAllForExtension(kIdX, 9).empty());
  EXPECT_TRUE(workers->GetAllForExtension(kIdA, 10).empty());
  EXPECT_TRUE(workers->GetAllForExtension(kIdB, 10).empty());
  EXPECT_TRUE(workers->GetAllForExtension(kIdC, 10).empty());
  EXPECT_TRUE(workers->GetAllForExtension(kIdX, 10).empty());
}

TEST_F(WorkerIdSetTest, RemoveAllForExtension) {
  // Some worker ids with multiple workers pointing to same extension(s).
  std::vector<WorkerId> workers_vector = {
      {kIdA, 10, 1, 2}, {kIdA, 10, 3, 4}, {kIdA, 11, 1, 2}, {kIdA, 12, 1, 2},
      {kIdB, 10, 1, 2}, {kIdB, 14, 1, 2}, {kIdB, 15, 1, 2}, {kIdB, 16, 1, 2},
      {kIdC, 20, 7, 3}, {kIdD, 20, 7, 3}, {kIdD, 20, 8, 2}, {kIdD, 21, 7, 1},
      {kIdD, 22, 9, 9}};
  std::unique_ptr<WorkerIdSet> worker_id_set =
      CreateWorkerIdSet(workers_vector);
  EXPECT_EQ(workers_vector.size(), worker_id_set->count_for_testing());

  size_t expected_entries_removed = 0u;
  auto test_removal = [&](const ExtensionId& extension_id_to_remove,
                          size_t expected_removal_count) {
    std::vector<WorkerId> worker_ids_to_remove =
        worker_id_set->GetAllForExtension(extension_id_to_remove);
    for (const auto& worker_id : worker_ids_to_remove) {
      if (!worker_id_set->Remove(worker_id)) {
        return ::testing::AssertionFailure()
               << "WorkerId not found to Remove: "
               << "{" << worker_id.extension_id
               << ", rph = " << worker_id.render_process_id
               << ", version_id = " << worker_id.version_id
               << ", thread = " << worker_id.thread_id << "}";
      }
    }
    expected_entries_removed += expected_removal_count;

    const size_t num_expected_entries =
        workers_vector.size() - expected_entries_removed;
    const size_t num_actual_entries = worker_id_set->count_for_testing();
    if (num_expected_entries != num_actual_entries) {
      return ::testing::AssertionFailure()
             << "Expected number of workers: " << num_expected_entries
             << ". Actual number of workers: " << num_actual_entries;
    }

    // No entry with |extension_id_to_remove| should be present.
    for (const WorkerId& worker_id : workers_vector) {
      if (worker_id.extension_id == extension_id_to_remove) {
        if (worker_id_set->Contains(worker_id)) {
          return ::testing::AssertionFailure()
                 << "Worker with extension_id: " << extension_id_to_remove
                 << " was not removed.";
        }
      }
    }
    return ::testing::AssertionSuccess();
  };

  // Removes 4 entries.
  EXPECT_TRUE(test_removal(kIdB, 4u));

  // The next attempt to remove the same extension wouldn't remove anything.
  EXPECT_TRUE(test_removal(kIdB, 0u));

  // Removes 1 entry.
  EXPECT_TRUE(test_removal(kIdC, 1u));

  // Removes 4 entries.
  EXPECT_TRUE(test_removal(kIdA, 4u));

  // Removes last entries.
  EXPECT_TRUE(test_removal(kIdD, 4u));

  EXPECT_EQ(0u, worker_id_set->count_for_testing());
}

// Tests parity of WorkerIdSet methods with a std::vector implementation.
TEST_F(WorkerIdSetTest, ExtensiveCases) {
  std::vector<WorkerId> worker_ids_vector =
      GenerateWorkerIds({kIdA, kIdC, kIdE, kIdG}, {1, 3, 5, 7, 9},
                        {10, 30, 50, 70}, {100, 300, 500, 700});
  VectorWorkerIdListImpl test_impl(worker_ids_vector);
  std::unique_ptr<WorkerIdSet> impl = CreateWorkerIdSet(worker_ids_vector);

  // Worker ids that we're going to use to test |worker_ids_vector|.
  std::vector<WorkerId> test_case_worker_ids_vector = GenerateWorkerIds(
      {kIdA, kIdB, kIdC, kIdD, kIdE, kIdF, kIdG}, {1, 2, 3, 4, 5, 6, 7, 8, 9},
      {10, 20, 30, 40, 50, 60, 70, 80, 90},
      {100, 200, 300, 400, 500, 600, 700, 800, 900});

  // Test that GetAllForExtension/Contains return the same value as vector based
  // implementation.
  for (const WorkerId& w : test_case_worker_ids_vector) {
    EXPECT_TRUE(AreWorkerIdsEqual(
        test_impl.GetAllForExtension(w.extension_id, w.render_process_id),
        impl->GetAllForExtension(w.extension_id, w.render_process_id)));
    EXPECT_EQ(test_impl.Contains(w), impl->Contains(w));
  }
}

}  // namespace extensions
