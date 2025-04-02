// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/data_decoder_manager.h"

#include <memory>
#include <optional>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

class DataDecoderManagerTest : public testing::Test {
 public:
  DataDecoderManagerTest() = default;
  ~DataDecoderManagerTest() override = default;

 protected:
  // Tries to decode CBOR using `data_decoder`, and checks the result, to
  // validate the DataDecoder works.
  void ValidateDecoder(data_decoder::DataDecoder& data_decoder) {
    base::test::TestFuture<data_decoder::DataDecoder::ValueOrError> future;
    // Try to decode the CBOR string value "test".
    data_decoder.ParseCbor({0x64, 0x74, 0x65, 0x73, 0x74},
                           future.GetCallback());
    const auto& result = future.Get();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "test");
  }

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("https://origin1.test"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("https://origin2.test"));

  base::TimeDelta kTinyTime = base::Milliseconds(1);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  DataDecoderManager manager_;
};

// Destroy a Handle, and make sure it times out as expected.
TEST_F(DataDecoderManagerTest, CreateAndDestroyHandle) {
  // Try a number of different delays before destroying the Handle, to make sure
  // the idle timer starts when the Handle is destroyed.
  for (base::TimeDelta delay :
       {base::Seconds(0), DataDecoderManager::kIdleTimeout - kTinyTime,
        DataDecoderManager::kIdleTimeout, base::Hours(1)}) {
    auto handle = manager_.GetHandle(kOrigin1, kOrigin2);
    EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
    EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);
    ValidateDecoder(handle->data_decoder());
    EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
    EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);

    // No matter when the Handle is destroyed, the underlying DataDecoder should
    // be kept alive for `kIdleTimeout`.
    task_environment_.FastForwardBy(delay);
    ValidateDecoder(handle->data_decoder());
    EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
    EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);

    // Destroying the Handle shouldn't destroy the DataDecoder.
    handle.reset();
    EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
    EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 0u);

    // Wait until just before the idle timeout, and check that the DataDecoder
    // still exists.
    task_environment_.FastForwardBy(DataDecoderManager::kIdleTimeout -
                                    kTinyTime);
    EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
    EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 0u);

    // Check that the handle times out correctly.
    task_environment_.FastForwardBy(kTinyTime);
    EXPECT_EQ(manager_.NumDecodersForTesting(), 0u);
    EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2),
              std::nullopt);
  }
}

// Check multiple Handles for a single DataDecoder.
TEST_F(DataDecoderManagerTest, MultipleHandles) {
  auto handle1 = manager_.GetHandle(kOrigin1, kOrigin2);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);
  ValidateDecoder(handle1->data_decoder());

  // Create a second Handle, check that it gets the same DataDecoder.
  auto handle2 = manager_.GetHandle(kOrigin1, kOrigin2);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 2u);
  EXPECT_EQ(&handle1->data_decoder(), &handle2->data_decoder());
  ValidateDecoder(handle2->data_decoder());

  // Destroy one handle. The decoder should still exist and not be timed out
  // after `kIdleTimeout`.
  handle1.reset();
  EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);
  ValidateDecoder(handle2->data_decoder());

  task_environment_.FastForwardBy(DataDecoderManager::kIdleTimeout);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);
  ValidateDecoder(handle2->data_decoder());

  // Grab a raw pointer to the decoder (which consumers shouldn't be doing),
  // delete the last Handle, and wait until just before the DataDecoder times
  // out.
  auto* raw_decoder = &handle2->data_decoder();
  handle2.reset();
  task_environment_.FastForwardBy(DataDecoderManager::kIdleTimeout - kTinyTime);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 0u);

  // Get another DataDecoder, which should return a Handle to the same object as
  // before.
  auto handle3 = manager_.GetHandle(kOrigin1, kOrigin2);
  EXPECT_EQ(&handle3->data_decoder(), raw_decoder);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);
  ValidateDecoder(handle3->data_decoder());
  // Clear `raw_decoder`, as it's no longer needed.
  raw_decoder = nullptr;

  // Decoder should not be destroyed after `kIdleTimeout`, since there's still a
  // live Handle.
  task_environment_.FastForwardBy(DataDecoderManager::kIdleTimeout);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);
  ValidateDecoder(handle3->data_decoder());

  // Destroy the last Handle, and make sure the underlying DataDecoder is
  // destroyed after `kIdleTimeout`.
  handle3.reset();
  task_environment_.FastForwardBy(DataDecoderManager::kIdleTimeout - kTinyTime);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 0u);

  task_environment_.FastForwardBy(kTinyTime);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2),
            std::nullopt);
}

// Create 4 different DataDecoders at once. Make sure they're all backed by
// different DataDecoders. Destroy all Handles at once, and check that the
// underlying DataDecoders are all destroyed together as well.
TEST_F(DataDecoderManagerTest, MultipleDataDecoders) {
  auto handle1 = manager_.GetHandle(kOrigin1, kOrigin1);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 1u);
  ValidateDecoder(handle1->data_decoder());

  auto handle2 = manager_.GetHandle(kOrigin1, kOrigin2);
  EXPECT_NE(&handle1->data_decoder(), &handle2->data_decoder());
  EXPECT_EQ(manager_.NumDecodersForTesting(), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);
  ValidateDecoder(handle2->data_decoder());

  auto handle3 = manager_.GetHandle(kOrigin2, kOrigin1);
  EXPECT_NE(&handle1->data_decoder(), &handle3->data_decoder());
  EXPECT_NE(&handle2->data_decoder(), &handle3->data_decoder());
  EXPECT_EQ(manager_.NumDecodersForTesting(), 3u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin1), 1u);
  ValidateDecoder(handle3->data_decoder());

  auto handle4 = manager_.GetHandle(kOrigin2, kOrigin2);
  EXPECT_NE(&handle1->data_decoder(), &handle4->data_decoder());
  EXPECT_NE(&handle2->data_decoder(), &handle4->data_decoder());
  EXPECT_NE(&handle3->data_decoder(), &handle4->data_decoder());
  EXPECT_EQ(manager_.NumDecodersForTesting(), 4u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin1), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin2), 1u);
  ValidateDecoder(handle4->data_decoder());

  // Create duplicates of each Handle, make sure the correct Handle is reused.

  auto handle1_2 = manager_.GetHandle(kOrigin1, kOrigin1);
  EXPECT_EQ(&handle1->data_decoder(), &handle1_2->data_decoder());
  EXPECT_EQ(manager_.NumDecodersForTesting(), 4u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin1), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin2), 1u);

  auto handle2_2 = manager_.GetHandle(kOrigin1, kOrigin2);
  EXPECT_EQ(&handle2->data_decoder(), &handle2_2->data_decoder());
  EXPECT_EQ(manager_.NumDecodersForTesting(), 4u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin1), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin2), 1u);

  auto handle3_2 = manager_.GetHandle(kOrigin2, kOrigin1);
  EXPECT_EQ(&handle3->data_decoder(), &handle3_2->data_decoder());
  EXPECT_EQ(manager_.NumDecodersForTesting(), 4u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin1), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin2), 1u);

  auto handle4_2 = manager_.GetHandle(kOrigin2, kOrigin2);
  EXPECT_EQ(&handle4->data_decoder(), &handle4_2->data_decoder());
  EXPECT_EQ(manager_.NumDecodersForTesting(), 4u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin1), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin2), 2u);

  // Tear down all Handles.

  handle1.reset();
  handle1_2.reset();
  EXPECT_EQ(manager_.NumDecodersForTesting(), 4u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin1), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin2), 2u);

  handle2.reset();
  handle2_2.reset();
  EXPECT_EQ(manager_.NumDecodersForTesting(), 4u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin1), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin2), 2u);

  handle3.reset();
  handle3_2.reset();
  EXPECT_EQ(manager_.NumDecodersForTesting(), 4u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin1), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin2), 2u);

  handle4.reset();
  handle4_2.reset();
  EXPECT_EQ(manager_.NumDecodersForTesting(), 4u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin1), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin2), 0u);

  // Wait until just before `kIdleTimeout`. No DataDecoders should be destroyed.
  task_environment_.FastForwardBy(DataDecoderManager::kIdleTimeout - kTinyTime);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 4u);

  // Wait until timeout. All DataDecoders should be timed out at once.
  task_environment_.FastForwardBy(kTinyTime);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1),
            std::nullopt);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2),
            std::nullopt);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin1),
            std::nullopt);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin2, kOrigin2),
            std::nullopt);
}

// Create 2 Handles, destroy one immediately, then destroy the other some time
// later, but while the timer for the other one is pending. Finally, check when
// the DataDecoders are actually destroyed.
TEST_F(DataDecoderManagerTest, OverlappingCleanupTimers) {
  const base::TimeDelta kHalfTimeout = DataDecoderManager::kIdleTimeout / 2;

  auto handle1 = manager_.GetHandle(kOrigin1, kOrigin1);
  auto handle2 = manager_.GetHandle(kOrigin1, kOrigin2);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);

  handle1.reset();
  EXPECT_EQ(manager_.NumDecodersForTesting(), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 1u);

  // Wait until half the timeout has passed before destroying `handle2`. Both
  // DataDecoders should still exist, but have no Handles.
  task_environment_.FastForwardBy(kHalfTimeout);
  handle2.reset();
  EXPECT_EQ(manager_.NumDecodersForTesting(), 2u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 0u);

  // Wait until just before `kIdleTimeout` from when `handle` was destroyed. No
  // DataDecoders should be destroyed.
  task_environment_.FastForwardBy(DataDecoderManager::kIdleTimeout -
                                  kHalfTimeout - kTinyTime);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 2u);

  // At exactly the timeout from when `handle1` was destroyed, its DataDecoder
  // should be destroyed. `handle2's` DataDecoder should still exist.
  task_environment_.FastForwardBy(kTinyTime);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1),
            std::nullopt);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2), 0u);

  // Due to the cleanup task throttling mechanism, the other DataDecoder will
  // only be destroyed after an additional `kIdleTimeout` has passed from when
  // the other one was timed out.
  task_environment_.FastForwardBy(DataDecoderManager::kIdleTimeout - kTinyTime);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 1u);
  task_environment_.FastForwardBy(kTinyTime);
  EXPECT_EQ(manager_.NumDecodersForTesting(), 0u);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin1),
            std::nullopt);
  EXPECT_EQ(manager_.GetHandleCountForTesting(kOrigin1, kOrigin2),
            std::nullopt);
}

}  // namespace
}  // namespace content
