// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/audio_mirroring_manager.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::AudioOutputStream;
using media::AudioParameters;
using media::AudioPushSink;
using testing::_;
using testing::Invoke;
using testing::NotNull;
using testing::Ref;
using testing::Return;
using testing::ReturnRef;

namespace content {

namespace {

class MockDiverter : public AudioMirroringManager::Diverter {
 public:
  MOCK_METHOD0(GetAudioParameters, const AudioParameters&());
  MOCK_METHOD1(StartDiverting, void(AudioOutputStream*));
  MOCK_METHOD0(StopDiverting, void());
  MOCK_METHOD1(StartDuplicating, void(AudioPushSink*));
  MOCK_METHOD1(StopDuplicating, void(AudioPushSink*));
};

class MockMirroringDestination
    : public AudioMirroringManager::MirroringDestination {
 public:
  MockMirroringDestination(int render_process_id,
                           int render_frame_id,
                           bool is_duplication)
      : render_process_id_(render_process_id),
        render_frame_id_(render_frame_id),
        query_count_(0),
        is_duplication_(is_duplication) {}

  void QueryForMatches(const std::set<GlobalFrameRoutingId>& candidates,
                       MatchesCallback results_callback) override {
    // The indirection is needed, because gmock has trouble with move-only
    // parameters (like |results_callback|).
    MockedQueryForMatches(candidates, &results_callback);
  }
  MOCK_METHOD2(MockedQueryForMatches,
               void(const std::set<GlobalFrameRoutingId>& candidates,
                    MatchesCallback* results_callback));

  MOCK_METHOD1(AddInput,
               media::AudioOutputStream*(const media::AudioParameters& params));

  MOCK_METHOD1(AddPushInput,
               media::AudioPushSink*(const media::AudioParameters& params));

  void SimulateQuery(const std::set<GlobalFrameRoutingId>& candidates,
                     MatchesCallback* results_callback) {
    ++query_count_;

    std::set<GlobalFrameRoutingId> result;
    if (candidates.find(GlobalFrameRoutingId(
            render_process_id_, render_frame_id_)) != candidates.end()) {
      result.insert(GlobalFrameRoutingId(render_process_id_, render_frame_id_));
    }
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(std::move(*results_callback),
                                  std::move(result), is_duplication_));
  }

  media::AudioOutputStream* SimulateAddInput(
      const media::AudioParameters& params) {
    static AudioOutputStream* const kNonNullPointer =
        reinterpret_cast<AudioOutputStream*>(0x11111110);
    return kNonNullPointer;
  }

  media::AudioPushSink* SimulateAddPushInput(
      const media::AudioParameters& params) {
    static AudioPushSink* const kNonNullPointer =
        reinterpret_cast<AudioPushSink*>(0x11111110);
    return kNonNullPointer;
  }

  int query_count() const {
    return query_count_;
  }

 private:
  const int render_process_id_;
  const int render_frame_id_;
  int query_count_;
  bool is_duplication_;
};

}  // namespace

class AudioMirroringManagerTest : public testing::Test {
 public:
  typedef AudioMirroringManager::Diverter Diverter;
  typedef AudioMirroringManager::MirroringDestination MirroringDestination;
  typedef AudioMirroringManager::StreamRoutes StreamRoutes;

  AudioMirroringManagerTest()
      : params_(AudioParameters::AUDIO_FAKE,
                media::CHANNEL_LAYOUT_STEREO,
                AudioParameters::kAudioCDSampleRate,
                AudioParameters::kAudioCDSampleRate / 10) {}

  MockDiverter* CreateStream(int render_process_id,
                             int render_frame_id,
                             int expected_times_diverted,
                             int expected_times_duplicated) {
    MockDiverter* const diverter = new MockDiverter();

    if (expected_times_diverted + expected_times_duplicated > 0) {
      EXPECT_CALL(*diverter, GetAudioParameters())
          .Times(expected_times_diverted + expected_times_duplicated)
          .WillRepeatedly(ReturnRef(params_));
    }

    if (expected_times_diverted > 0) {
      EXPECT_CALL(*diverter, StartDiverting(NotNull()))
          .Times(expected_times_diverted);
      EXPECT_CALL(*diverter, StopDiverting())
          .Times(expected_times_diverted);
    }

    if (expected_times_duplicated > 0) {
      EXPECT_CALL(*diverter, StartDuplicating(NotNull()))
          .Times(expected_times_duplicated);
      EXPECT_CALL(*diverter, StopDuplicating(NotNull()))
          .Times(expected_times_duplicated);
    }

    mirroring_manager_.AddDiverter(
        render_process_id, render_frame_id, diverter);
    RunAllPendingInMessageLoop();

    return diverter;
  }

  void KillStream(MockDiverter* diverter) {
    mirroring_manager_.RemoveDiverter(diverter);
    delete diverter;
  }

  void StartMirroringTo(const std::unique_ptr<MockMirroringDestination>& dest,
                        int expected_inputs_added,
                        int expected_push_inputs_added) {
    EXPECT_CALL(*dest, MockedQueryForMatches(_, _))
        .WillRepeatedly(
            Invoke(dest.get(), &MockMirroringDestination::SimulateQuery));
    if (expected_inputs_added > 0) {
      EXPECT_CALL(*dest, AddInput(Ref(params_)))
          .Times(expected_inputs_added)
          .WillRepeatedly(Invoke(dest.get(),
                                 &MockMirroringDestination::SimulateAddInput))
          .RetiresOnSaturation();
    }

    if (expected_push_inputs_added > 0) {
      EXPECT_CALL(*dest, AddPushInput(Ref(params_)))
          .Times(expected_push_inputs_added)
          .WillRepeatedly(Invoke(
              dest.get(), &MockMirroringDestination::SimulateAddPushInput))
          .RetiresOnSaturation();
    }

    mirroring_manager_.StartMirroring(dest.get());
    RunAllPendingInMessageLoop();
  }

  void StopMirroringTo(const std::unique_ptr<MockMirroringDestination>& dest) {
    mirroring_manager_.StopMirroring(dest.get());
    RunAllPendingInMessageLoop();
  }

  int CountStreamsDivertedTo(
      const std::unique_ptr<MockMirroringDestination>& dest) const {
    int count = 0;
    for (auto it = mirroring_manager_.routes_.begin();
         it != mirroring_manager_.routes_.end(); ++it) {
      if (it->destination == dest.get())
        ++count;
    }
    return count;
  }

  int CountStreamsDuplicatedTo(
      const std::unique_ptr<MockMirroringDestination>& dest) const {
    int count = 0;
    for (auto it = mirroring_manager_.routes_.begin();
         it != mirroring_manager_.routes_.end(); ++it) {
      if (it->duplications.find(dest.get()) != it->duplications.end())
        ++count;
    }
    return count;
  }

  void ExpectNoLongerManagingAnything() const {
    EXPECT_TRUE(mirroring_manager_.routes_.empty());
    EXPECT_TRUE(mirroring_manager_.sessions_.empty());
  }

 private:
  BrowserTaskEnvironment task_environment_;
  AudioParameters params_;
  AudioMirroringManager mirroring_manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioMirroringManagerTest);
};

namespace {
const int kRenderProcessId = 123;
const int kRenderFrameId = 456;
const int kAnotherRenderProcessId = 789;
const int kAnotherRenderFrameId = 1234;
const int kYetAnotherRenderProcessId = 4560;
const int kYetAnotherRenderFrameId = 7890;
}

TEST_F(AudioMirroringManagerTest, MirroringSessionOfNothing) {
  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 0, 0);
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(0, destination->query_count());

  ExpectNoLongerManagingAnything();
}

TEST_F(AudioMirroringManagerTest, TwoMirroringSessionsOfNothing) {
  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 0, 0);
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(0, destination->query_count());

  const std::unique_ptr<MockMirroringDestination> another_destination(
      new MockMirroringDestination(kAnotherRenderProcessId,
                                   kAnotherRenderFrameId, false));
  StartMirroringTo(another_destination, 0, 0);
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));

  StopMirroringTo(another_destination);
  EXPECT_EQ(0, another_destination->query_count());

  ExpectNoLongerManagingAnything();
}

// Tests that a mirroring session starts after, and ends before, a stream that
// will be diverted to it.
TEST_F(AudioMirroringManagerTest, StreamLifetimeAroundMirroringSession) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 1, 0);
  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  ExpectNoLongerManagingAnything();
}

// Tests that a mirroring session starts before, and ends after, a stream that
// will be diverted to it.
TEST_F(AudioMirroringManagerTest, StreamLifetimeWithinMirroringSession) {
  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 1, 0);
  EXPECT_EQ(0, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  ExpectNoLongerManagingAnything();
}

// Tests that a stream is diverted correctly as two mirroring sessions come and
// go.
TEST_F(AudioMirroringManagerTest, StreamLifetimeAcrossTwoMirroringSessions) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 2, 0);

  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  const std::unique_ptr<MockMirroringDestination> second_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(second_destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, second_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(second_destination));

  StopMirroringTo(second_destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, second_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(second_destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, second_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(second_destination));

  ExpectNoLongerManagingAnything();
}

// Tests that a stream does not flip-flop between two destinations that are a
// match for it.
TEST_F(AudioMirroringManagerTest, StreamDivertingStickyToOneDestination_1) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 2, 0);

  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  const std::unique_ptr<MockMirroringDestination> replacement_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(replacement_destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, replacement_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(replacement_destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  ExpectNoLongerManagingAnything();
}

// Same as StreamDivertingStickyToOneDestination_1, with a different order of
// operations that should have the same effects.
TEST_F(AudioMirroringManagerTest, StreamDivertingStickyToOneDestination_2) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 2, 0);

  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  const std::unique_ptr<MockMirroringDestination> replacement_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(replacement_destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, replacement_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(replacement_destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(replacement_destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  ExpectNoLongerManagingAnything();
}

// Same as StreamDivertingStickyToOneDestination_1, except that the stream is
// killed before the first destination is stopped.  Therefore, the second
// destination should never see the stream.
TEST_F(AudioMirroringManagerTest, StreamDivertingStickyToOneDestination_3) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 1, 0);

  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  const std::unique_ptr<MockMirroringDestination> replacement_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(replacement_destination, 0, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  StopMirroringTo(replacement_destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, replacement_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(replacement_destination));

  ExpectNoLongerManagingAnything();
}

// Tests that multiple streams are diverted/mixed to one destination.
TEST_F(AudioMirroringManagerTest, MultipleStreamsInOneMirroringSession) {
  MockDiverter* const stream1 =
      CreateStream(kRenderProcessId, kRenderFrameId, 1, 0);

  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 3, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  MockDiverter* const stream2 =
      CreateStream(kRenderProcessId, kRenderFrameId, 1, 0);
  EXPECT_EQ(2, destination->query_count());
  EXPECT_EQ(2, CountStreamsDivertedTo(destination));

  MockDiverter* const stream3 =
      CreateStream(kRenderProcessId, kRenderFrameId, 1, 0);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(3, CountStreamsDivertedTo(destination));

  KillStream(stream2);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(2, CountStreamsDivertedTo(destination));

  StopMirroringTo(destination);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  KillStream(stream3);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  KillStream(stream1);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));

  ExpectNoLongerManagingAnything();
}

// A random interleaving of operations for three separate targets, each of which
// has one stream mirrored to one destination.
TEST_F(AudioMirroringManagerTest, ThreeSeparateMirroringSessions) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 1, 0);

  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  const std::unique_ptr<MockMirroringDestination> another_destination(
      new MockMirroringDestination(kAnotherRenderProcessId,
                                   kAnotherRenderFrameId, false));
  StartMirroringTo(another_destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));

  MockDiverter* const another_stream =
      CreateStream(kAnotherRenderProcessId, kAnotherRenderFrameId, 1, 0);
  EXPECT_EQ(2, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(another_destination));

  KillStream(stream);
  EXPECT_EQ(2, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(another_destination));

  MockDiverter* const yet_another_stream =
      CreateStream(kYetAnotherRenderProcessId, kYetAnotherRenderFrameId, 1, 0);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(3, another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(another_destination));

  const std::unique_ptr<MockMirroringDestination> yet_another_destination(
      new MockMirroringDestination(kYetAnotherRenderProcessId,
                                   kYetAnotherRenderFrameId, false));
  StartMirroringTo(yet_another_destination, 1, 0);
  EXPECT_EQ(3, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(3, another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(1, yet_another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(yet_another_destination));

  StopMirroringTo(another_destination);
  EXPECT_EQ(4, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(3, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(2, yet_another_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(yet_another_destination));

  StopMirroringTo(yet_another_destination);
  EXPECT_EQ(5, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(3, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(2, yet_another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(yet_another_destination));

  StopMirroringTo(destination);
  EXPECT_EQ(5, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(3, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(2, yet_another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(yet_another_destination));

  KillStream(another_stream);
  EXPECT_EQ(5, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(3, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(2, yet_another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(yet_another_destination));

  KillStream(yet_another_stream);
  EXPECT_EQ(5, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(3, another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(another_destination));
  EXPECT_EQ(2, yet_another_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(yet_another_destination));

  ExpectNoLongerManagingAnything();
}

// Tests that a stream can be successfully duplicated.
TEST_F(AudioMirroringManagerTest, DuplicationToOneDestination) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 1, 1);

  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  const std::unique_ptr<MockMirroringDestination> duplicated_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, true));
  StartMirroringTo(duplicated_destination, 0, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, duplicated_destination->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, duplicated_destination->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination));

  StopMirroringTo(duplicated_destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, duplicated_destination->query_count());
  EXPECT_EQ(0, CountStreamsDuplicatedTo(duplicated_destination));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, duplicated_destination->query_count());
  EXPECT_EQ(0, CountStreamsDuplicatedTo(duplicated_destination));

  ExpectNoLongerManagingAnything();
}

// Tests that a stream can be successfully duplicated to multiple destinations
// simultaneously.
TEST_F(AudioMirroringManagerTest, DuplicationToMultipleDestinations) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 1, 2);

  const std::unique_ptr<MockMirroringDestination> destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(destination, 1, 0);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));

  const std::unique_ptr<MockMirroringDestination> duplicated_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, true));
  StartMirroringTo(duplicated_destination, 0, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, duplicated_destination->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination));

  const std::unique_ptr<MockMirroringDestination> duplicated_destination2(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, true));
  StartMirroringTo(duplicated_destination2, 0, 1);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(destination));
  EXPECT_EQ(1, duplicated_destination->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination));
  EXPECT_EQ(1, duplicated_destination2->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination2));

  StopMirroringTo(destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, duplicated_destination->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination));
  EXPECT_EQ(2, duplicated_destination2->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination2));

  StopMirroringTo(duplicated_destination);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, duplicated_destination->query_count());
  EXPECT_EQ(0, CountStreamsDuplicatedTo(duplicated_destination));
  EXPECT_EQ(2, duplicated_destination2->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination2));

  StopMirroringTo(duplicated_destination2);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, duplicated_destination->query_count());
  EXPECT_EQ(0, CountStreamsDuplicatedTo(duplicated_destination));
  EXPECT_EQ(2, duplicated_destination2->query_count());
  EXPECT_EQ(0, CountStreamsDuplicatedTo(duplicated_destination2));

  KillStream(stream);
  EXPECT_EQ(1, destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(destination));
  EXPECT_EQ(2, duplicated_destination->query_count());
  EXPECT_EQ(0, CountStreamsDuplicatedTo(duplicated_destination));
  EXPECT_EQ(2, duplicated_destination2->query_count());
  EXPECT_EQ(0, CountStreamsDuplicatedTo(duplicated_destination2));

  ExpectNoLongerManagingAnything();
}

// Tests that duplication should not be affected when the major flow gets
// diverted to another destination
TEST_F(AudioMirroringManagerTest,
       DuplicationUnaffectedBySwitchingDivertedFlow) {
  MockDiverter* const stream =
      CreateStream(kRenderProcessId, kRenderFrameId, 2, 1);

  const std::unique_ptr<MockMirroringDestination> duplicated_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, true));
  StartMirroringTo(duplicated_destination, 0, 1);
  EXPECT_EQ(1, duplicated_destination->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination));

  const std::unique_ptr<MockMirroringDestination> divert_destination(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(divert_destination, 1, 0);
  EXPECT_EQ(1, divert_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(divert_destination));
  EXPECT_EQ(1, duplicated_destination->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination));

  const std::unique_ptr<MockMirroringDestination> divert_destination2(
      new MockMirroringDestination(kRenderProcessId, kRenderFrameId, false));
  StartMirroringTo(divert_destination2, 1, 0);
  EXPECT_EQ(1, divert_destination->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(divert_destination));
  EXPECT_EQ(1, duplicated_destination->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination));
  EXPECT_EQ(1, divert_destination2->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(divert_destination2));

  StopMirroringTo(divert_destination);
  EXPECT_EQ(1, divert_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(divert_destination));
  EXPECT_EQ(2, duplicated_destination->query_count());
  EXPECT_EQ(1, CountStreamsDuplicatedTo(duplicated_destination));
  EXPECT_EQ(2, divert_destination2->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(divert_destination2));

  StopMirroringTo(duplicated_destination);
  EXPECT_EQ(1, divert_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(divert_destination));
  EXPECT_EQ(2, duplicated_destination->query_count());
  EXPECT_EQ(0, CountStreamsDuplicatedTo(duplicated_destination));
  EXPECT_EQ(2, divert_destination2->query_count());
  EXPECT_EQ(1, CountStreamsDivertedTo(divert_destination2));

  StopMirroringTo(divert_destination2);
  EXPECT_EQ(1, divert_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(divert_destination));
  EXPECT_EQ(2, duplicated_destination->query_count());
  EXPECT_EQ(0, CountStreamsDuplicatedTo(duplicated_destination));
  EXPECT_EQ(2, divert_destination2->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(divert_destination2));

  KillStream(stream);
  EXPECT_EQ(1, divert_destination->query_count());
  EXPECT_EQ(0, CountStreamsDivertedTo(divert_destination));
  EXPECT_EQ(2, duplicated_destination->query_count());
  EXPECT_EQ(0, CountStreamsDuplicatedTo(duplicated_destination));

  ExpectNoLongerManagingAnything();
}

}  // namespace content
