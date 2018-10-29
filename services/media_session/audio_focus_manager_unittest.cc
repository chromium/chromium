// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/audio_focus_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/audio_focus_manager_metrics_helper.h"
#include "services/media_session/media_session_service.h"
#include "services/media_session/mock_media_session.h"
#include "services/media_session/public/cpp/switches.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

namespace {

const char kExampleSourceName[] = "test";
const char kExampleSourceName2[] = "test2";

}  // anonymous namespace

// This tests the Audio Focus Manager API. The parameter determines whether
// audio focus is enabled or not. If it is not enabled it should track the media
// sessions but not enforce single session focus.
class AudioFocusManagerTest : public testing::TestWithParam<bool> {
 public:
  AudioFocusManagerTest() = default;

  void SetUp() override {
    if (!GetParam()) {
      command_line_.GetProcessCommandLine()->AppendSwitchASCII(
          switches::kEnableAudioFocus, switches::kEnableAudioFocusNoEnforce);
    }

    ASSERT_EQ(GetParam(), IsAudioFocusEnforcementEnabled());

    // Create an instance of the MediaSessionService.
    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            MediaSessionService::Create());
    connector_ = connector_factory_->CreateConnector();

    // Bind |audio_focus_ptr_| to AudioFocusManager.
    connector_->BindInterface("test", mojo::MakeRequest(&audio_focus_ptr_));

    // Bind |audio_focus_debug_ptr_| to AudioFocusManagerDebug.
    connector_->BindInterface("test",
                              mojo::MakeRequest(&audio_focus_debug_ptr_));
  }

  void TearDown() override {
    // Run pending tasks.
    base::RunLoop().RunUntilIdle();
  }

  AudioFocusManager::RequestId GetAudioFocusedSession() {
    const auto audio_focus_requests = GetRequests();
    for (auto iter = audio_focus_requests.rbegin();
         iter != audio_focus_requests.rend(); ++iter) {
      if ((*iter)->audio_focus_type == mojom::AudioFocusType::kGain)
        return (*iter)->request_id.value();
    }
    return base::UnguessableToken::Null();
  }

  int GetTransientCount() {
    return GetCountForType(mojom::AudioFocusType::kGainTransient);
  }

  int GetTransientMaybeDuckCount() {
    return GetCountForType(mojom::AudioFocusType::kGainTransientMayDuck);
  }

  void AbandonAudioFocusNoReset(test::MockMediaSession* session) {
    session->audio_focus_request()->AbandonAudioFocus();
    session->FlushForTesting();
    audio_focus_ptr_.FlushForTesting();
  }

  AudioFocusManager::RequestId RequestAudioFocus(
      test::MockMediaSession* session,
      mojom::AudioFocusType audio_focus_type) {
    return session->RequestAudioFocusFromService(audio_focus_ptr_,
                                                 audio_focus_type);
  }

  mojom::MediaSessionDebugInfoPtr GetDebugInfo(
      AudioFocusManager::RequestId request_id) {
    mojom::MediaSessionDebugInfoPtr result;
    base::OnceCallback<void(mojom::MediaSessionDebugInfoPtr)> callback =
        base::BindOnce(
            [](mojom::MediaSessionDebugInfoPtr* out_result,
               mojom::MediaSessionDebugInfoPtr result) {
              *out_result = std::move(result);
            },
            &result);

    GetDebugService()->GetDebugInfoForRequest(request_id, std::move(callback));

    audio_focus_ptr_.FlushForTesting();
    audio_focus_debug_ptr_.FlushForTesting();

    return result;
  }

  mojom::MediaSessionInfo::SessionState GetState(
      test::MockMediaSession* session) {
    mojom::MediaSessionInfo::SessionState state = session->GetState();

    if (!GetParam()) {
      // If audio focus enforcement is disabled then we should never see these
      // states in the tests.
      EXPECT_NE(mojom::MediaSessionInfo::SessionState::kSuspended, state);
      EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking, state);
    }

    return state;
  }

  std::unique_ptr<test::TestAudioFocusObserver> CreateObserver() {
    std::unique_ptr<test::TestAudioFocusObserver> observer =
        std::make_unique<test::TestAudioFocusObserver>();

    mojom::AudioFocusObserverPtr observer_ptr;
    observer->BindToMojoRequest(mojo::MakeRequest(&observer_ptr));
    GetService()->AddObserver(std::move(observer_ptr));

    audio_focus_ptr_.FlushForTesting();
    return observer;
  }

  mojom::MediaSessionInfo::SessionState GetStateFromParam(
      mojom::MediaSessionInfo::SessionState state) {
    // If enforcement is enabled then returns the provided state, otherwise
    // returns kActive because without enforcement we did not change state.
    if (GetParam())
      return state;
    return mojom::MediaSessionInfo::SessionState::kActive;
  }

  void SetSourceName(const std::string& name) {
    GetService()->SetSourceName(name);
    audio_focus_ptr_.FlushForTesting();
  }

  mojom::AudioFocusManagerPtr CreateAudioFocusManagerPtr() {
    mojom::AudioFocusManagerPtr ptr;
    connector_->BindInterface("test", mojo::MakeRequest(&ptr));
    return ptr;
  }

  const std::string GetSourceNameForLastRequest() {
    std::vector<mojom::AudioFocusRequestStatePtr> requests = GetRequests();
    EXPECT_TRUE(requests.back());
    return requests.back()->source_name.value();
  }

  std::unique_ptr<base::HistogramSamples> GetHistogramSamplesSinceTestStart(
      const std::string& name) {
    return histogram_tester_.GetHistogramSamplesSinceCreation(name);
  }

  int GetAudioFocusHistogramCount() {
    return histogram_tester_
        .GetTotalCountsForPrefix("Media.Session.AudioFocus.")
        .size();
  }

 private:
  int GetCountForType(mojom::AudioFocusType type) {
    const auto audio_focus_requests = GetRequests();
    return std::count_if(audio_focus_requests.begin(),
                         audio_focus_requests.end(),
                         [type](const auto& session) {
                           return session->audio_focus_type == type;
                         });
  }

  std::vector<mojom::AudioFocusRequestStatePtr> GetRequests() {
    std::vector<mojom::AudioFocusRequestStatePtr> result;

    GetService()->GetFocusRequests(base::BindOnce(
        [](std::vector<mojom::AudioFocusRequestStatePtr>* out,
           std::vector<mojom::AudioFocusRequestStatePtr> requests) {
          for (auto& request : requests)
            out->push_back(request.Clone());
        },
        &result));

    audio_focus_ptr_.FlushForTesting();
    return result;
  }

  mojom::AudioFocusManager* GetService() const {
    return audio_focus_ptr_.get();
  }

  mojom::AudioFocusManagerDebug* GetDebugService() const {
    return audio_focus_debug_ptr_.get();
  }

  void FlushForTestingIfEnabled() {
    if (!GetParam())
      return;

    audio_focus_ptr_.FlushForTesting();
  }

  base::test::ScopedCommandLine command_line_;
  base::test::ScopedTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;

  mojom::AudioFocusManagerPtr audio_focus_ptr_;
  mojom::AudioFocusManagerDebugPtr audio_focus_debug_ptr_;

  DISALLOW_COPY_AND_ASSIGN(AudioFocusManagerTest);
};

INSTANTIATE_TEST_CASE_P(, AudioFocusManagerTest, testing::Bool());

TEST_P(AudioFocusManagerTest, RequestAudioFocusGain_ReplaceFocusedEntry) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;
  test::MockMediaSession media_session_3;

  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusedSession());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kInactive,
            GetState(&media_session_1));
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kInactive,
            GetState(&media_session_2));
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kInactive,
            GetState(&media_session_3));

  AudioFocusManager::RequestId request_id_1 =
      RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id_1, GetAudioFocusedSession());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));

  AudioFocusManager::RequestId request_id_2 =
      RequestAudioFocus(&media_session_2, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id_2, GetAudioFocusedSession());
  EXPECT_EQ(
      GetStateFromParam(mojom::MediaSessionInfo::SessionState::kSuspended),
      GetState(&media_session_1));

  AudioFocusManager::RequestId request_id_3 =
      RequestAudioFocus(&media_session_3, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id_3, GetAudioFocusedSession());
  EXPECT_EQ(
      GetStateFromParam(mojom::MediaSessionInfo::SessionState::kSuspended),
      GetState(&media_session_2));
}

TEST_P(AudioFocusManagerTest, RequestAudioFocusGain_Duplicate) {
  test::MockMediaSession media_session;

  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusedSession());

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());

  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());
}

TEST_P(AudioFocusManagerTest, RequestAudioFocusGain_FromTransient) {
  test::MockMediaSession media_session;

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGainTransient);
  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusedSession());
  EXPECT_EQ(1, GetTransientCount());

  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());
  EXPECT_EQ(0, GetTransientCount());
}

TEST_P(AudioFocusManagerTest, RequestAudioFocusGain_FromTransientMayDuck) {
  test::MockMediaSession media_session;

  AudioFocusManager::RequestId request_id = RequestAudioFocus(
      &media_session, mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusedSession());
  EXPECT_EQ(1, GetTransientMaybeDuckCount());

  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
}

TEST_P(AudioFocusManagerTest, RequestAudioFocusTransient_FromGain) {
  test::MockMediaSession media_session;

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);

  EXPECT_EQ(request_id, GetAudioFocusedSession());
  EXPECT_EQ(0, GetTransientCount());

  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGainTransient);
  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusedSession());
  EXPECT_EQ(1, GetTransientCount());
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kSuspended,
            GetState(&media_session));
}

TEST_P(AudioFocusManagerTest, RequestAudioFocusTransientMayDuck_FromGain) {
  test::MockMediaSession media_session;

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);

  EXPECT_EQ(request_id, GetAudioFocusedSession());
  EXPECT_EQ(0, GetTransientMaybeDuckCount());

  RequestAudioFocus(&media_session,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusedSession());
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session));
}

TEST_P(AudioFocusManagerTest, RequestAudioFocusTransient_FromGainWhileDucking) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(0, GetTransientCount());
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGainTransient);
  EXPECT_EQ(1, GetTransientCount());
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));
}

TEST_P(AudioFocusManagerTest,
       RequestAudioFocusTransientMayDuck_FromGainWhileDucking) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_1,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(2, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));
}

TEST_P(AudioFocusManagerTest, AbandonAudioFocus_RemovesFocusedEntry) {
  test::MockMediaSession media_session;

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());

  media_session.AbandonAudioFocusFromClient();
  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusedSession());
}

TEST_P(AudioFocusManagerTest, AbandonAudioFocus_MultipleCalls) {
  test::MockMediaSession media_session;

  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id, GetAudioFocusedSession());

  AbandonAudioFocusNoReset(&media_session);

  std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
  media_session.AbandonAudioFocusFromClient();

  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusedSession());
  EXPECT_TRUE(observer->focus_lost_session_.is_null());
}

TEST_P(AudioFocusManagerTest, AbandonAudioFocus_RemovesTransientMayDuckEntry) {
  test::MockMediaSession media_session;

  RequestAudioFocus(&media_session,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    media_session.AbandonAudioFocusFromClient();

    EXPECT_EQ(0, GetTransientMaybeDuckCount());
    EXPECT_TRUE(observer->focus_lost_session_.Equals(
        test::GetMediaSessionInfoSync(&media_session)));
  }
}

TEST_P(AudioFocusManagerTest, AbandonAudioFocus_RemovesTransientEntry) {
  test::MockMediaSession media_session;

  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGainTransient);
  EXPECT_EQ(1, GetTransientCount());

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    media_session.AbandonAudioFocusFromClient();

    EXPECT_EQ(0, GetTransientCount());
    EXPECT_TRUE(observer->focus_lost_session_.Equals(
        test::GetMediaSessionInfoSync(&media_session)));
  }
}

TEST_P(AudioFocusManagerTest, AbandonAudioFocus_WhileDuckingThenResume) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_1));

  media_session_1.AbandonAudioFocusFromClient();
  EXPECT_EQ(1, GetTransientMaybeDuckCount());

  media_session_2.AbandonAudioFocusFromClient();
  EXPECT_EQ(0, GetTransientMaybeDuckCount());

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));
}

TEST_P(AudioFocusManagerTest, AbandonAudioFocus_StopsDucking) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_1));

  media_session_2.AbandonAudioFocusFromClient();
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));
}

TEST_P(AudioFocusManagerTest, AbandonAudioFocus_ResumesPlayback) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2, mojom::AudioFocusType::kGainTransient);
  EXPECT_EQ(1, GetTransientCount());
  EXPECT_EQ(
      GetStateFromParam(mojom::MediaSessionInfo::SessionState::kSuspended),
      GetState(&media_session_1));

  media_session_2.AbandonAudioFocusFromClient();
  EXPECT_EQ(0, GetTransientCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));
}

TEST_P(AudioFocusManagerTest, DuckWhilePlaying) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_1));
}

TEST_P(AudioFocusManagerTest, GainSuspendsTransient) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_2, mojom::AudioFocusType::kGainTransient);

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(
      GetStateFromParam(mojom::MediaSessionInfo::SessionState::kSuspended),
      GetState(&media_session_2));
}

TEST_P(AudioFocusManagerTest, GainSuspendsTransientMayDuck) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(
      GetStateFromParam(mojom::MediaSessionInfo::SessionState::kSuspended),
      GetState(&media_session_2));
}

TEST_P(AudioFocusManagerTest, DuckWithMultipleTransientMayDucks) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;
  test::MockMediaSession media_session_3;
  test::MockMediaSession media_session_4;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2, mojom::AudioFocusType::kGainTransient);
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_2));

  RequestAudioFocus(&media_session_3,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_1));
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_2));

  RequestAudioFocus(&media_session_4,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_1));
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_2));

  media_session_3.AbandonAudioFocusFromClient();
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_1));
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_2));

  media_session_4.AbandonAudioFocusFromClient();
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_1));
  EXPECT_NE(mojom::MediaSessionInfo::SessionState::kDucking,
            GetState(&media_session_2));
}

TEST_P(AudioFocusManagerTest, MediaSessionDestroyed_ReleasesFocus) {
  {
    test::MockMediaSession media_session;

    AudioFocusManager::RequestId request_id =
        RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
    EXPECT_EQ(request_id, GetAudioFocusedSession());
  }

  // If the media session is destroyed without abandoning audio focus we do not
  // know until we next interact with the manager.
  test::MockMediaSession media_session;
  RequestAudioFocus(&media_session,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(base::UnguessableToken::Null(), GetAudioFocusedSession());
}

TEST_P(AudioFocusManagerTest, MediaSessionDestroyed_ReleasesTransient) {
  {
    test::MockMediaSession media_session;
    RequestAudioFocus(&media_session, mojom::AudioFocusType::kGainTransient);
    EXPECT_EQ(1, GetTransientCount());
  }

  // If the media session is destroyed without abandoning audio focus we do not
  // know until we next interact with the manager.
  test::MockMediaSession media_session;
  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientCount());
}

TEST_P(AudioFocusManagerTest, MediaSessionDestroyed_ReleasesTransientMayDucks) {
  {
    test::MockMediaSession media_session;
    RequestAudioFocus(&media_session,
                      mojom::AudioFocusType::kGainTransientMayDuck);
    EXPECT_EQ(1, GetTransientMaybeDuckCount());
  }

  // If the media session is destroyed without abandoning audio focus we do not
  // know until we next interact with the manager.
  test::MockMediaSession media_session;
  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
}

TEST_P(AudioFocusManagerTest, GainDucksForceDuck) {
  test::MockMediaSession media_session_1(true /* force_duck */);
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);

  AudioFocusManager::RequestId request_id_2 =
      RequestAudioFocus(&media_session_2, mojom::AudioFocusType::kGain);

  EXPECT_EQ(request_id_2, GetAudioFocusedSession());
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_1));
}

TEST_P(AudioFocusManagerTest,
       AbandoningGainFocusRevokesTopMostForceDuckSession) {
  test::MockMediaSession media_session_1(true /* force_duck */);
  test::MockMediaSession media_session_2;
  test::MockMediaSession media_session_3;

  AudioFocusManager::RequestId request_id_1 =
      RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  AudioFocusManager::RequestId request_id_2 =
      RequestAudioFocus(&media_session_2, mojom::AudioFocusType::kGain);

  AudioFocusManager::RequestId request_id_3 =
      RequestAudioFocus(&media_session_3, mojom::AudioFocusType::kGain);
  EXPECT_EQ(request_id_3, GetAudioFocusedSession());

  EXPECT_EQ(
      GetStateFromParam(mojom::MediaSessionInfo::SessionState::kSuspended),
      GetState(&media_session_2));
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_1));

  media_session_3.AbandonAudioFocusFromClient();
  EXPECT_EQ(GetParam() ? request_id_1 : request_id_2, GetAudioFocusedSession());
}

TEST_P(AudioFocusManagerTest, AudioFocusObserver_RequestNoop) {
  test::MockMediaSession media_session;
  AudioFocusManager::RequestId request_id;

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    request_id =
        RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);

    EXPECT_EQ(request_id, GetAudioFocusedSession());
    EXPECT_EQ(mojom::AudioFocusType::kGain, observer->focus_gained_type());
    EXPECT_FALSE(observer->focus_gained_session_.is_null());
  }

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);

    EXPECT_EQ(request_id, GetAudioFocusedSession());
    EXPECT_TRUE(observer->focus_gained_session_.is_null());
  }
}

TEST_P(AudioFocusManagerTest, AudioFocusObserver_TransientMayDuck) {
  test::MockMediaSession media_session;

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    RequestAudioFocus(&media_session,
                      mojom::AudioFocusType::kGainTransientMayDuck);

    EXPECT_EQ(1, GetTransientMaybeDuckCount());
    EXPECT_EQ(mojom::AudioFocusType::kGainTransientMayDuck,
              observer->focus_gained_type());
    EXPECT_FALSE(observer->focus_gained_session_.is_null());
  }

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    media_session.AbandonAudioFocusFromClient();

    EXPECT_EQ(0, GetTransientMaybeDuckCount());
    EXPECT_TRUE(observer->focus_lost_session_.Equals(
        test::GetMediaSessionInfoSync(&media_session)));
  }
}

TEST_P(AudioFocusManagerTest, GetDebugInfo) {
  test::MockMediaSession media_session;
  AudioFocusManager::RequestId request_id =
      RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);

  mojom::MediaSessionDebugInfoPtr debug_info = GetDebugInfo(request_id);
  EXPECT_FALSE(debug_info->name.empty());
  EXPECT_FALSE(debug_info->owner.empty());
  EXPECT_FALSE(debug_info->state.empty());
}

TEST_P(AudioFocusManagerTest, GetDebugInfo_BadRequestId) {
  mojom::MediaSessionDebugInfoPtr debug_info =
      GetDebugInfo(base::UnguessableToken::Create());
  EXPECT_TRUE(debug_info->name.empty());
}

TEST_P(AudioFocusManagerTest,
       RequestAudioFocusTransient_FromGainWhileSuspended) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2, mojom::AudioFocusType::kGainTransient);
  EXPECT_EQ(1, GetTransientCount());
  EXPECT_EQ(
      GetStateFromParam(mojom::MediaSessionInfo::SessionState::kSuspended),
      GetState(&media_session_1));

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGainTransient);
  EXPECT_EQ(2, GetTransientCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));
}

TEST_P(AudioFocusManagerTest,
       RequestAudioFocusTransientMayDuck_FromGainWhileSuspended) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientCount());
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2, mojom::AudioFocusType::kGainTransient);
  EXPECT_EQ(1, GetTransientCount());
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_EQ(
      GetStateFromParam(mojom::MediaSessionInfo::SessionState::kSuspended),
      GetState(&media_session_1));

  RequestAudioFocus(&media_session_1,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientCount());
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));
}

TEST_P(AudioFocusManagerTest, SourceName_AssociatedWithBinding) {
  SetSourceName(kExampleSourceName);

  mojom::AudioFocusManagerPtr new_ptr = CreateAudioFocusManagerPtr();
  new_ptr->SetSourceName(kExampleSourceName2);
  new_ptr.FlushForTesting();

  test::MockMediaSession media_session;
  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(kExampleSourceName, GetSourceNameForLastRequest());
}

TEST_P(AudioFocusManagerTest, SourceName_Empty) {
  test::MockMediaSession media_session;
  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_TRUE(GetSourceNameForLastRequest().empty());
}

TEST_P(AudioFocusManagerTest, SourceName_Updated) {
  SetSourceName(kExampleSourceName);

  test::MockMediaSession media_session;
  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  EXPECT_EQ(kExampleSourceName, GetSourceNameForLastRequest());

  SetSourceName(kExampleSourceName2);
  EXPECT_EQ(kExampleSourceName, GetSourceNameForLastRequest());
}

TEST_P(AudioFocusManagerTest, RecordUmaMetrics) {
  EXPECT_EQ(0, GetAudioFocusHistogramCount());

  SetSourceName(kExampleSourceName);
  test::MockMediaSession media_session;
  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGainTransient);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            "Media.Session.AudioFocus.Request.Test"));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
                     AudioFocusManagerMetricsHelper::AudioFocusRequestSource::
                         kInitial)));
  }

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            "Media.Session.AudioFocus.Type.Test"));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(
        1,
        samples->GetCount(static_cast<base::HistogramBase::Sample>(
            AudioFocusManagerMetricsHelper::AudioFocusType::kGainTransient)));
  }

  EXPECT_EQ(2, GetAudioFocusHistogramCount());

  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            "Media.Session.AudioFocus.Request.Test"));
    EXPECT_EQ(2, samples->TotalCount());
    EXPECT_EQ(
        1,
        samples->GetCount(static_cast<base::HistogramBase::Sample>(
            AudioFocusManagerMetricsHelper::AudioFocusRequestSource::kUpdate)));
  }

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            "Media.Session.AudioFocus.Type.Test"));
    EXPECT_EQ(2, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
                     AudioFocusManagerMetricsHelper::AudioFocusType::kGain)));
  }

  EXPECT_EQ(2, GetAudioFocusHistogramCount());

  media_session.AbandonAudioFocusFromClient();

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            "Media.Session.AudioFocus.Abandon.Test"));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(
        1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
               AudioFocusManagerMetricsHelper::AudioFocusAbandonSource::kAPI)));
  }

  EXPECT_EQ(3, GetAudioFocusHistogramCount());
}

TEST_P(AudioFocusManagerTest, RecordUmaMetrics_ConnectionError) {
  SetSourceName(kExampleSourceName);

  {
    test::MockMediaSession media_session;
    RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);
  }

  test::MockMediaSession media_session;
  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            "Media.Session.AudioFocus.Abandon.Test"));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(static_cast<base::HistogramBase::Sample>(
                     AudioFocusManagerMetricsHelper::AudioFocusAbandonSource::
                         kConnectionError)));
  }
}

TEST_P(AudioFocusManagerTest, RecordUmaMetrics_NoSourceName) {
  test::MockMediaSession media_session;
  RequestAudioFocus(&media_session, mojom::AudioFocusType::kGain);

  EXPECT_EQ(0, GetAudioFocusHistogramCount());
}

TEST_P(AudioFocusManagerTest,
       AbandonAudioFocus_ObserverFocusGain_NoTopSession) {
  test::MockMediaSession media_session;

  RequestAudioFocus(&media_session,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session));

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    media_session.AbandonAudioFocusFromClient();

    EXPECT_EQ(0, GetTransientMaybeDuckCount());
    EXPECT_TRUE(observer->focus_lost_session_.Equals(
        test::GetMediaSessionInfoSync(&media_session)));
    EXPECT_TRUE(observer->focus_gained_session_.is_null());

    auto notifications = observer->notifications();
    EXPECT_EQ(1u, notifications.size());
    EXPECT_EQ(test::TestAudioFocusObserver::NotificationType::kFocusLost,
              notifications[0]);
  }
}

TEST_P(AudioFocusManagerTest,
       AbandonAudioFocus_ObserverFocusGain_NewTopSession) {
  test::MockMediaSession media_session_1;
  test::MockMediaSession media_session_2;

  RequestAudioFocus(&media_session_1, mojom::AudioFocusType::kGain);
  EXPECT_EQ(0, GetTransientMaybeDuckCount());
  EXPECT_EQ(mojom::MediaSessionInfo::SessionState::kActive,
            GetState(&media_session_1));

  RequestAudioFocus(&media_session_2,
                    mojom::AudioFocusType::kGainTransientMayDuck);
  EXPECT_EQ(1, GetTransientMaybeDuckCount());
  EXPECT_EQ(GetStateFromParam(mojom::MediaSessionInfo::SessionState::kDucking),
            GetState(&media_session_1));

  mojom::MediaSessionInfoPtr media_session_1_info =
      test::GetMediaSessionInfoSync(&media_session_1);

  {
    std::unique_ptr<test::TestAudioFocusObserver> observer = CreateObserver();
    media_session_2.AbandonAudioFocusFromClient();

    EXPECT_EQ(0, GetTransientMaybeDuckCount());
    EXPECT_TRUE(observer->focus_lost_session_.Equals(
        test::GetMediaSessionInfoSync(&media_session_2)));
    EXPECT_TRUE(observer->focus_gained_session_.Equals(media_session_1_info));

    // FocusLost should always come before FocusGained so observers always know
    // the current session that has focus.
    auto notifications = observer->notifications();
    EXPECT_EQ(2u, notifications.size());
    EXPECT_EQ(test::TestAudioFocusObserver::NotificationType::kFocusLost,
              notifications[0]);
    EXPECT_EQ(test::TestAudioFocusObserver::NotificationType::kFocusGained,
              notifications[1]);
  }
}

}  // namespace media_session
