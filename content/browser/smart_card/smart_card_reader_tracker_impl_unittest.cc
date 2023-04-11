// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/smart_card_reader_tracker_impl.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/smart_card/mock_smart_card_context_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/smart_card/smart_card.mojom.h"

using base::test::TestFuture;
using blink::mojom::SmartCardReaderInfo;
using blink::mojom::SmartCardReaderInfoPtr;
using blink::mojom::SmartCardReaderState;
using device::mojom::SmartCardContext;
using device::mojom::SmartCardError;
using device::mojom::SmartCardReaderStateFlags;
using device::mojom::SmartCardReaderStateOut;
using device::mojom::SmartCardReaderStateOutPtr;

using testing::_;
using testing::ElementsAre;
using testing::InSequence;

bool operator==(const SmartCardReaderInfo& a, const SmartCardReaderInfo& b) {
  return a.name == b.name && a.state == b.state && a.atr == b.atr;
}

MATCHER_P3(IsReaderInfo, name, state, atr, "") {
  return !arg.is_null() && arg->name == name && arg->state == state &&
         arg->atr == atr;
}

namespace blink::mojom {

void PrintTo(const SmartCardReaderInfo& reader, std::ostream* os) {
  *os << "SmartCardReaderInfo(" << reader.name << ", " << reader.state
      << ", ATR{";

  bool first = true;
  for (uint8_t num : reader.atr) {
    if (!first) {
      *os << ",";
    }
    // Treat it as a number instead of as a char.
    *os << unsigned(num);
    first = false;
  }
  *os << "})";
}

void PrintTo(const SmartCardReaderInfoPtr& reader_ptr, std::ostream* os) {
  if (!reader_ptr) {
    *os << "SmartCardReaderInfoPtr(NULL)";
    return;
  }
  PrintTo(*reader_ptr, os);
}

}  // namespace blink::mojom

namespace content {
namespace {

class SmartCardReaderTrackerImplTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

class MockTrackerObserver : public SmartCardReaderTracker::Observer {
 public:
  MOCK_METHOD(void,
              OnReaderAdded,
              (const SmartCardReaderInfo& reader_info),
              (override));

  MOCK_METHOD(void,
              OnReaderRemoved,
              (const SmartCardReaderInfo& reader_info),
              (override));

  MOCK_METHOD(void,
              OnReaderChanged,
              (const SmartCardReaderInfo& reader_info),
              (override));

  MOCK_METHOD(void, OnError, (SmartCardError error), (override));
};

TEST_F(SmartCardReaderTrackerImplTest, ReaderChanged) {
  MockSmartCardContextFactory mock_context_factory;
  SmartCardReaderTrackerImpl tracker(mock_context_factory.GetRemote(),
                                     /*context_supports_reader_added=*/true);
  TestFuture<SmartCardContext::GetStatusChangeCallback>
      last_get_status_callback;
  {
    InSequence s;

    // Request what readers are currently available.
    EXPECT_CALL(mock_context_factory, ListReaders(_))
        .WillOnce([](SmartCardContext::ListReadersCallback callback) {
          std::vector<std::string> readers{"Reader A", "Reader B"};
          auto result =
              device::mojom::SmartCardListReadersResult::NewReaders(readers);
          std::move(callback).Run(std::move(result));
        });

    // Request the state of each of those readers.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 2U);
              ASSERT_EQ(states_in[0]->reader, "Reader A");
              EXPECT_TRUE(states_in[0]->current_state->unaware);
              EXPECT_FALSE(states_in[0]->current_state->ignore);
              EXPECT_FALSE(states_in[0]->current_state->changed);
              EXPECT_FALSE(states_in[0]->current_state->unknown);
              EXPECT_FALSE(states_in[0]->current_state->unavailable);
              EXPECT_FALSE(states_in[0]->current_state->empty);
              EXPECT_FALSE(states_in[0]->current_state->present);
              EXPECT_FALSE(states_in[0]->current_state->exclusive);
              EXPECT_FALSE(states_in[0]->current_state->inuse);
              EXPECT_FALSE(states_in[0]->current_state->mute);
              EXPECT_FALSE(states_in[0]->current_state->unpowered);

              ASSERT_EQ(states_in[1]->reader, "Reader B");
              EXPECT_TRUE(states_in[1]->current_state->unaware);
              EXPECT_FALSE(states_in[1]->current_state->ignore);
              EXPECT_FALSE(states_in[1]->current_state->changed);
              EXPECT_FALSE(states_in[1]->current_state->unknown);
              EXPECT_FALSE(states_in[1]->current_state->unavailable);
              EXPECT_FALSE(states_in[1]->current_state->empty);
              EXPECT_FALSE(states_in[1]->current_state->present);
              EXPECT_FALSE(states_in[1]->current_state->exclusive);
              EXPECT_FALSE(states_in[1]->current_state->inuse);
              EXPECT_FALSE(states_in[1]->current_state->mute);
              EXPECT_FALSE(states_in[1]->current_state->unpowered);

              std::vector<SmartCardReaderStateOutPtr> states_out;

              auto state_flags = SmartCardReaderStateFlags::New();
              state_flags->empty = true;
              states_out.push_back(SmartCardReaderStateOut::New(
                  "Reader A", std::move(state_flags), std::vector<uint8_t>()));

              state_flags = SmartCardReaderStateFlags::New();
              state_flags->present = true;
              state_flags->inuse = true;
              states_out.push_back(SmartCardReaderStateOut::New(
                  "Reader B", std::move(state_flags),
                  std::vector<uint8_t>({1u, 2u, 3u, 4u})));

              auto result =
                  device::mojom::SmartCardStatusChangeResult::NewReaderStates(
                      std::move(states_out));
              std::move(callback).Run(std::move(result));
            });

    // Request to be notified of state changes on those readers and on the
    // addition of a new reader.
    // SmartCardContext reports that "Reader B" has changed (card was removed,
    // thus it's now empty).
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 3U);

              EXPECT_EQ(states_in[0]->reader, R"(\\?PnP?\Notification)");
              EXPECT_FALSE(states_in[0]->current_state->unaware);
              EXPECT_FALSE(states_in[0]->current_state->ignore);
              EXPECT_FALSE(states_in[0]->current_state->changed);
              EXPECT_FALSE(states_in[0]->current_state->unknown);
              EXPECT_FALSE(states_in[0]->current_state->unavailable);
              EXPECT_FALSE(states_in[0]->current_state->empty);
              EXPECT_FALSE(states_in[0]->current_state->present);
              EXPECT_FALSE(states_in[0]->current_state->exclusive);
              EXPECT_FALSE(states_in[0]->current_state->inuse);
              EXPECT_FALSE(states_in[0]->current_state->mute);
              EXPECT_FALSE(states_in[0]->current_state->unpowered);

              EXPECT_EQ(states_in[1]->reader, "Reader A");
              EXPECT_FALSE(states_in[1]->current_state->unaware);
              EXPECT_FALSE(states_in[1]->current_state->ignore);
              EXPECT_FALSE(states_in[1]->current_state->changed);
              EXPECT_FALSE(states_in[1]->current_state->unknown);
              EXPECT_FALSE(states_in[1]->current_state->unavailable);
              EXPECT_TRUE(states_in[1]->current_state->empty);
              EXPECT_FALSE(states_in[1]->current_state->present);
              EXPECT_FALSE(states_in[1]->current_state->exclusive);
              EXPECT_FALSE(states_in[1]->current_state->inuse);
              EXPECT_FALSE(states_in[1]->current_state->mute);
              EXPECT_FALSE(states_in[1]->current_state->unpowered);

              EXPECT_EQ(states_in[2]->reader, "Reader B");
              EXPECT_FALSE(states_in[2]->current_state->unaware);
              EXPECT_FALSE(states_in[2]->current_state->ignore);
              EXPECT_FALSE(states_in[2]->current_state->changed);
              EXPECT_FALSE(states_in[2]->current_state->unknown);
              EXPECT_FALSE(states_in[2]->current_state->unavailable);
              EXPECT_FALSE(states_in[2]->current_state->empty);
              EXPECT_TRUE(states_in[2]->current_state->present);
              EXPECT_FALSE(states_in[2]->current_state->exclusive);
              EXPECT_TRUE(states_in[2]->current_state->inuse);
              EXPECT_FALSE(states_in[2]->current_state->mute);
              EXPECT_FALSE(states_in[2]->current_state->unpowered);

              std::vector<SmartCardReaderStateOutPtr> states_out;

              auto state_flags = SmartCardReaderStateFlags::New();
              // Nothing was added or removed (thus all flags are false)
              states_out.push_back(SmartCardReaderStateOut::New(
                  R"(\\?PnP?\Notification)", std::move(state_flags),
                  std::vector<uint8_t>()));

              state_flags = SmartCardReaderStateFlags::New();
              // Nothing changed for Reader A.
              state_flags->empty = true;
              states_out.push_back(SmartCardReaderStateOut::New(
                  "Reader A", std::move(state_flags), std::vector<uint8_t>()));

              state_flags = SmartCardReaderStateFlags::New();
              // Reader be has changed. It's now empty as well.
              state_flags->changed = true;
              state_flags->empty = true;
              states_out.push_back(SmartCardReaderStateOut::New(
                  "Reader B", std::move(state_flags), std::vector<uint8_t>()));

              auto result =
                  device::mojom::SmartCardStatusChangeResult::NewReaderStates(
                      std::move(states_out));
              std::move(callback).Run(std::move(result));
            });

    ////
    // Now rinse and repeat

    // Request what readers are currently available.
    // Done just in case a reader was added in between PC/SC calls or their
    // processing. Not a water-tight solution but it's the best the tracker can
    // do given PC/SC limitations. A tracker user is free to call Start() again
    // to force a refresh.
    //
    // Still the same readers.
    EXPECT_CALL(mock_context_factory, ListReaders(_))
        .WillOnce([](SmartCardContext::ListReadersCallback callback) {
          std::vector<std::string> readers{"Reader A", "Reader B"};
          auto result =
              device::mojom::SmartCardListReadersResult::NewReaders(readers);
          std::move(callback).Run(std::move(result));
        });

    // Since ListReaders did not return any reader unknown to the tracker,
    // it will now skip to waiting to be notified on any changes.
    EXPECT_CALL(mock_context_factory, GetStatusChange(_, _, _))
        .WillOnce(
            [](base::TimeDelta timeout,
               std::vector<device::mojom::SmartCardReaderStateInPtr> states_in,
               SmartCardContext::GetStatusChangeCallback callback) {
              ASSERT_EQ(states_in.size(), 3U);

              EXPECT_EQ(states_in[0]->reader, R"(\\?PnP?\Notification)");
              EXPECT_FALSE(states_in[0]->current_state->unaware);
              EXPECT_FALSE(states_in[0]->current_state->ignore);
              EXPECT_FALSE(states_in[0]->current_state->changed);
              EXPECT_FALSE(states_in[0]->current_state->unknown);
              EXPECT_FALSE(states_in[0]->current_state->unavailable);
              EXPECT_FALSE(states_in[0]->current_state->empty);
              EXPECT_FALSE(states_in[0]->current_state->present);
              EXPECT_FALSE(states_in[0]->current_state->exclusive);
              EXPECT_FALSE(states_in[0]->current_state->inuse);
              EXPECT_FALSE(states_in[0]->current_state->mute);
              EXPECT_FALSE(states_in[0]->current_state->unpowered);

              EXPECT_EQ(states_in[1]->reader, "Reader A");
              EXPECT_FALSE(states_in[1]->current_state->unaware);
              EXPECT_FALSE(states_in[1]->current_state->ignore);
              EXPECT_FALSE(states_in[1]->current_state->changed);
              EXPECT_FALSE(states_in[1]->current_state->unknown);
              EXPECT_FALSE(states_in[1]->current_state->unavailable);
              EXPECT_TRUE(states_in[1]->current_state->empty);
              EXPECT_FALSE(states_in[1]->current_state->present);
              EXPECT_FALSE(states_in[1]->current_state->exclusive);
              EXPECT_FALSE(states_in[1]->current_state->inuse);
              EXPECT_FALSE(states_in[1]->current_state->mute);
              EXPECT_FALSE(states_in[1]->current_state->unpowered);

              // Note that the tracker now states that Reader B is empty as
              // well.
              EXPECT_EQ(states_in[2]->reader, "Reader B");
              EXPECT_FALSE(states_in[1]->current_state->unaware);
              EXPECT_FALSE(states_in[1]->current_state->ignore);
              EXPECT_FALSE(states_in[1]->current_state->changed);
              EXPECT_FALSE(states_in[1]->current_state->unknown);
              EXPECT_FALSE(states_in[1]->current_state->unavailable);
              EXPECT_TRUE(states_in[1]->current_state->empty);
              EXPECT_FALSE(states_in[1]->current_state->present);
              EXPECT_FALSE(states_in[1]->current_state->exclusive);
              EXPECT_FALSE(states_in[1]->current_state->inuse);
              EXPECT_FALSE(states_in[1]->current_state->mute);
              EXPECT_FALSE(states_in[1]->current_state->unpowered);

              std::vector<SmartCardReaderStateOutPtr> states_out;

              // Let the test code run this callback.
              std::move(callback).Run(
                  device::mojom::SmartCardStatusChangeResult::NewError(
                      SmartCardError::kNoService));
            });
  }

  MockTrackerObserver observer;
  {
    InSequence s;

    EXPECT_CALL(observer, OnReaderAdded(SmartCardReaderInfo(
                              "Reader A", SmartCardReaderState::kEmpty,
                              std::vector<uint8_t>())));

    EXPECT_CALL(observer, OnReaderAdded(SmartCardReaderInfo(
                              "Reader B", SmartCardReaderState::kInuse,
                              std::vector<uint8_t>({1u, 2u, 3u, 4u}))));

    EXPECT_CALL(observer, OnReaderChanged(SmartCardReaderInfo(
                              "Reader B", SmartCardReaderState::kEmpty,
                              std::vector<uint8_t>({}))));

    EXPECT_CALL(observer, OnError(SmartCardError::kNoService));
  }

  TestFuture<blink::mojom::SmartCardGetReadersResultPtr> start_future;
  tracker.Start(&observer, start_future.GetCallback());

  blink::mojom::SmartCardGetReadersResultPtr result = start_future.Take();
  ASSERT_TRUE(result->is_readers());

  std::vector<blink::mojom::SmartCardReaderInfoPtr>& readers =
      result->get_readers();
  // NB: Ideally would use `testing::UnorderedElementsAre`, but it doesn't work
  // with non-copiable types.
  // It returns the state of the readers *before* the change.
  ASSERT_EQ(readers.size(), 2U);
  EXPECT_THAT(readers[0], IsReaderInfo("Reader A", SmartCardReaderState::kEmpty,
                                       std::vector<uint8_t>()));
  EXPECT_THAT(readers[1], IsReaderInfo("Reader B", SmartCardReaderState::kInuse,
                                       std::vector<uint8_t>({1u, 2u, 3u, 4u})));

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

}  // namespace
}  // namespace content
