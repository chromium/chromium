// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/network_resources_data.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// Regression test for crbug.com/501878477.
TEST(NetworkResourcesDataTest, ClearPreservesResourcesAndIds) {
  test::TaskEnvironment task_environment;
  // Initialize with small limits to trigger eviction easily.
  NetworkResourcesData* data =
      MakeGarbageCollected<NetworkResourcesData>(100, 100);

  String request_id = "request-id";
  String loader_id = "loader-id";
  KURL url("http://example.com");

  data->ResourceCreated(request_id, loader_id, url, nullptr);
  data->SetResourceContent(request_id, "content");

  // Clear with preserved loader ID.
  data->Clear(loader_id);
  EXPECT_EQ(data->Resources().size(), 1u);

  // Adding a new large resource should now successfully evict the preserved
  // resource because its ID was added back to the deque.
  String new_request_id = "new-request-id";
  data->ResourceCreated(new_request_id, "new-loader-id", url, nullptr);
  // Total size = initial_size + 100. Since limit is 100, eviction MUST happen.
  data->SetResourceContent(
      new_request_id,
      String(base::span<const char>(std::vector<char>(100, 'a'))));

  // The first resource should be evicted (content cleared).
  bool found_evicted = false;
  for (auto& resource : data->Resources()) {
    if (resource->RequestId() == request_id) {
      EXPECT_TRUE(resource->IsContentEvicted());
      found_evicted = true;
    }
  }
  EXPECT_TRUE(found_evicted);
}

// Regression test for crbug.com/501878477.
TEST(NetworkResourcesDataTest, EnsureFreeSpaceSafety) {
  test::TaskEnvironment task_environment;
  NetworkResourcesData* data =
      MakeGarbageCollected<NetworkResourcesData>(100, 100);

  // Manually desynchronize for testing the safety check (if possible, though
  // our fix for Clear makes it harder to reach this state via public API).
  // We can just rely on the fact that if we didn't have the Clear fix, this
  // would crash. With the safety check, even if we were in a bad state, we
  // won't OOB read.

  // Since we can't easily desynchronize anymore, this test mostly ensures
  // no regressions in basic eviction.
  data->ResourceCreated("id1", "loader", KURL("http://a.com"), nullptr);
  data->SetResourceContent("id1", "content");

  // Trigger immediate eviction if needed
  data->SetResourcesDataSizeLimits(10, 10);

  // Adding more should work safely.
  data->ResourceCreated("id2", "loader", KURL("http://b.com"), nullptr);
  data->SetResourceContent("id2", "small");
}

// Regression test for crbug.com/501878477.
TEST(NetworkResourcesDataTest, SetResourcesDataSizeLimitsEvictsCorrectly) {
  test::TaskEnvironment task_environment;
  NetworkResourcesData* data =
      MakeGarbageCollected<NetworkResourcesData>(1000, 1000);

  data->ResourceCreated("id1", "loader", KURL("http://a.com"), nullptr);
  data->SetResourceContent("id1", "some-data");
  size_t size1 = data->Resources().front()->ContentSize();

  data->ResourceCreated("id2", "loader", KURL("http://b.com"), nullptr);
  data->SetResourceContent("id2", "more-data");
  size_t size2 = 0;
  for (auto& r : data->Resources()) {
    if (r->RequestId() == "id2") {
      size2 = r->ContentSize();
    }
  }

  // Reduce limit to be less than size1 + size2.
  // This should trigger EnsureFreeSpace(0).
  data->SetResourcesDataSizeLimits(size1 + size2 - 1, 1000);

  // At least one resource should be evicted.
  bool any_evicted = false;
  for (auto& resource : data->Resources()) {
    if (resource->IsContentEvicted()) {
      any_evicted = true;
      break;
    }
  }
  EXPECT_TRUE(any_evicted);
}

}  // namespace blink
