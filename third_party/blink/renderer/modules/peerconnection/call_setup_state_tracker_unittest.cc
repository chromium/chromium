// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/call_setup_state_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

template <typename StateType>
Vector<StateType> GetAllCallSetupStates();

template <>
Vector<OffererState> GetAllCallSetupStates() {
  Vector<OffererState> states = {OffererState::kNotStarted,
                                 OffererState::kCreateOfferPending,
                                 OffererState::kCreateOfferRejected,
                                 OffererState::kCreateOfferResolved,
                                 OffererState::kSetLocalOfferPending,
                                 OffererState::kSetLocalOfferRejected,
                                 OffererState::kSetLocalOfferResolved,
                                 OffererState::kSetRemoteAnswerPending,
                                 OffererState::kSetRemoteAnswerRejected,
                                 OffererState::kSetRemoteAnswerResolved};
  EXPECT_EQ(static_cast<size_t>(OffererState::kMaxValue) + 1u, states.size());
  return states;
}

template <>
Vector<AnswererState> GetAllCallSetupStates() {
  Vector<AnswererState> states = {AnswererState::kNotStarted,
                                  AnswererState::kSetRemoteOfferPending,
                                  AnswererState::kSetRemoteOfferRejected,
                                  AnswererState::kSetRemoteOfferResolved,
                                  AnswererState::kCreateAnswerPending,
                                  AnswererState::kCreateAnswerRejected,
                                  AnswererState::kCreateAnswerResolved,
                                  AnswererState::kSetLocalAnswerPending,
                                  AnswererState::kSetLocalAnswerRejected,
                                  AnswererState::kSetLocalAnswerResolved};
  EXPECT_EQ(static_cast<size_t>(AnswererState::kMaxValue) + 1u, states.size());
  return states;
}

}  // namespace

class CallSetupStateTrackerTest : public testing::Test {
 public:
  enum class Reachability {
    kReachable,
    kUnreachable,
  };

  template <typename StateType>
  StateType current_state() const;

  bool NoteStateEvent(CallSetupStateTracker* tracker,
                      OffererState event) const {
    return tracker->NoteOffererStateEvent(event, false);
  }

  bool NoteStateEvent(CallSetupStateTracker* tracker,
                      AnswererState event) const {
    return tracker->NoteAnswererStateEvent(event, false);
  }

  template <typename StateType>
  bool VerifyReachability(Reachability reachability,
                          Vector<StateType> states) const {
    bool expected_state_reached = (reachability == Reachability::kReachable);
    for (const auto& state : states) {
      bool did_reach_state;
      if (state == current_state<StateType>()) {
        // The current state always counts as reachable.
        did_reach_state = true;
      } else {
        // Perform the test on a copy to avoid mutating |tracker_|.
        CallSetupStateTracker tracker_copy = tracker_;
        did_reach_state = NoteStateEvent(&tracker_copy, state);
      }
      if (did_reach_state != expected_state_reached)
        return false;
    }
    return true;
  }

  template <typename StateType>
  bool VerifyOnlyReachableStates(Vector<StateType> reachable_states,
                                 bool include_current = true) const {
    if (include_current)
      reachable_states.push_back(current_state<StateType>());
    Vector<StateType> unreachable_states = GetAllCallSetupStates<StateType>();
    for (const auto& reachable_state : reachable_states) {
      unreachable_states.erase(std::find(unreachable_states.begin(),
                                         unreachable_states.end(),
                                         reachable_state));
    }
    return VerifyReachability<StateType>(Reachability::kReachable,
                                         reachable_states) &&
           VerifyReachability<StateType>(Reachability::kUnreachable,
                                         unreachable_states);
  }

 protected:
  CallSetupStateTracker tracker_;
};

// The following two template specializations can be moved to the class
// declaration once we officially switch to C++17 (we need C++ DR727 to be
// implemented).
template <>
OffererState CallSetupStateTrackerTest::current_state() const {
  return tracker_.offerer_state();
}

template <>
AnswererState CallSetupStateTrackerTest::current_state() const {
  return tracker_.answerer_state();
}

TEST_F(CallSetupStateTrackerTest, InitialState) {
  EXPECT_EQ(OffererState::kNotStarted, tracker_.offerer_state());
  EXPECT_EQ(AnswererState::kNotStarted, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kNotStarted, tracker_.GetCallSetupState());
  EXPECT_FALSE(tracker_.document_uses_media());
}

TEST_F(CallSetupStateTrackerTest, OffererSuccessfulNegotiation) {
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kCreateOfferPending}));
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending, false));
  EXPECT_EQ(OffererState::kCreateOfferPending, tracker_.offerer_state());
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kCreateOfferResolved,
       OffererState::kCreateOfferRejected}));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(OffererState::kCreateOfferResolved,
                                             false));
  EXPECT_EQ(OffererState::kCreateOfferResolved, tracker_.offerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetLocalOfferPending}));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetLocalOfferPending, false));
  EXPECT_EQ(OffererState::kSetLocalOfferPending, tracker_.offerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetLocalOfferResolved,
       OffererState::kSetLocalOfferRejected}));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetLocalOfferResolved, false));
  EXPECT_EQ(OffererState::kSetLocalOfferResolved, tracker_.offerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetRemoteAnswerPending}));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetRemoteAnswerPending, false));
  EXPECT_EQ(OffererState::kSetRemoteAnswerPending, tracker_.offerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetRemoteAnswerResolved,
       OffererState::kSetRemoteAnswerRejected}));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetRemoteAnswerResolved, false));
  EXPECT_EQ(OffererState::kSetRemoteAnswerResolved, tracker_.offerer_state());
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>({}));
}

TEST_F(CallSetupStateTrackerTest, OffererCreateOfferRejected) {
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending, false));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(OffererState::kCreateOfferRejected,
                                             false));
  EXPECT_EQ(OffererState::kCreateOfferRejected, tracker_.offerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kCreateOfferResolved}));
}

TEST_F(CallSetupStateTrackerTest, OffererSetLocalOfferRejected) {
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending, false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(OffererState::kCreateOfferResolved,
                                             false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetLocalOfferPending, false));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetLocalOfferRejected, false));
  EXPECT_EQ(OffererState::kSetLocalOfferRejected, tracker_.offerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetLocalOfferResolved}));
}

TEST_F(CallSetupStateTrackerTest, OffererSetRemoteAnswerRejected) {
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending, false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(OffererState::kCreateOfferResolved,
                                             false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetLocalOfferPending, false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetLocalOfferResolved, false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetRemoteAnswerPending, false));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetRemoteAnswerRejected, false));
  EXPECT_EQ(OffererState::kSetRemoteAnswerRejected, tracker_.offerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<OffererState>(
      {OffererState::kSetRemoteAnswerResolved}));
}

TEST_F(CallSetupStateTrackerTest, OffererRejectThenSucceed) {
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending, false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(OffererState::kCreateOfferResolved,
                                             false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetLocalOfferPending, false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetLocalOfferResolved, false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetRemoteAnswerPending, false));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetRemoteAnswerRejected, false));
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  // Pending another operation should not revert the states to "pending" or
  // "started".
  EXPECT_FALSE(tracker_.NoteOffererStateEvent(
      OffererState::kSetRemoteAnswerPending, false));
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetRemoteAnswerResolved, false));
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
}

TEST_F(CallSetupStateTrackerTest, AnswererSuccessfulNegotiation) {
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetRemoteOfferPending}));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferPending, false));
  EXPECT_EQ(AnswererState::kSetRemoteOfferPending, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetRemoteOfferResolved,
       AnswererState::kSetRemoteOfferRejected}));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferResolved, false));
  EXPECT_EQ(AnswererState::kSetRemoteOfferResolved, tracker_.answerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kCreateAnswerPending}));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kCreateAnswerPending, false));
  EXPECT_EQ(AnswererState::kCreateAnswerPending, tracker_.answerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kCreateAnswerResolved,
       AnswererState::kCreateAnswerRejected}));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kCreateAnswerResolved, false));
  EXPECT_EQ(AnswererState::kCreateAnswerResolved, tracker_.answerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetLocalAnswerPending}));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetLocalAnswerPending, false));
  EXPECT_EQ(AnswererState::kSetLocalAnswerPending, tracker_.answerer_state());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetLocalAnswerResolved,
       AnswererState::kSetLocalAnswerRejected}));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetLocalAnswerResolved, false));
  EXPECT_EQ(AnswererState::kSetLocalAnswerResolved, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>({}));
}

TEST_F(CallSetupStateTrackerTest, AnswererSetRemoteOfferRejected) {
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferPending, false));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferRejected, false));
  EXPECT_EQ(AnswererState::kSetRemoteOfferRejected, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetRemoteOfferResolved}));
}

TEST_F(CallSetupStateTrackerTest, AnswererCreateAnswerRejected) {
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferPending, false));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferResolved, false));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kCreateAnswerPending, false));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kCreateAnswerRejected, false));
  EXPECT_EQ(AnswererState::kCreateAnswerRejected, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kCreateAnswerResolved}));
}

TEST_F(CallSetupStateTrackerTest, AnswererSetLocalAnswerRejected) {
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferPending, false));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferResolved, false));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kCreateAnswerPending, false));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kCreateAnswerResolved, false));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetLocalAnswerPending, false));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetLocalAnswerRejected, false));
  EXPECT_EQ(AnswererState::kSetLocalAnswerRejected, tracker_.answerer_state());
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(VerifyOnlyReachableStates<AnswererState>(
      {AnswererState::kSetLocalAnswerResolved}));
}

TEST_F(CallSetupStateTrackerTest, AnswererRejectThenSucceed) {
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferPending, false));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferResolved, false));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kCreateAnswerPending, false));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kCreateAnswerResolved, false));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetLocalAnswerPending, false));
  EXPECT_EQ(CallSetupState::kStarted, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetLocalAnswerRejected, false));
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  // Pending another operation should not revert the states to "pending" or
  // "started".
  EXPECT_FALSE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetLocalAnswerPending, false));
  EXPECT_EQ(CallSetupState::kFailed, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetLocalAnswerResolved, false));
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
}

// Succeeding in one role and subsequently failing in another should not revert
// the call setup state from kSucceeded; the most succeessful attempt would
// still have been successful.
TEST_F(CallSetupStateTrackerTest, OffererSucceedAnswererFail) {
  EXPECT_TRUE(
      tracker_.NoteOffererStateEvent(OffererState::kCreateOfferPending, false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(OffererState::kCreateOfferResolved,
                                             false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetLocalOfferPending, false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetLocalOfferResolved, false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetRemoteAnswerPending, false));
  EXPECT_TRUE(tracker_.NoteOffererStateEvent(
      OffererState::kSetRemoteAnswerResolved, false));
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferPending, false));
  EXPECT_TRUE(tracker_.NoteAnswererStateEvent(
      AnswererState::kSetRemoteOfferRejected, false));
  // Still succeeded.
  EXPECT_EQ(CallSetupState::kSucceeded, tracker_.GetCallSetupState());
}

TEST_F(CallSetupStateTrackerTest, SetDocumentMedia) {
  // Any offerer state event can set document_uses_media().
  for (auto offerer_state : GetAllCallSetupStates<OffererState>()) {
    CallSetupStateTracker tracker;
    EXPECT_FALSE(tracker.document_uses_media());
    tracker.NoteOffererStateEvent(offerer_state, true);
    EXPECT_TRUE(tracker.document_uses_media());
  }
  // Any answerer state event can set document_uses_media().
  for (auto answerer_state : GetAllCallSetupStates<AnswererState>()) {
    CallSetupStateTracker tracker;
    EXPECT_FALSE(tracker.document_uses_media());
    tracker.NoteAnswererStateEvent(answerer_state, true);
    EXPECT_TRUE(tracker.document_uses_media());
  }
}

TEST_F(CallSetupStateTrackerTest, DocumentMediaCannotBeUnset) {
  tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferPending, true);
  EXPECT_TRUE(tracker_.document_uses_media());
  tracker_.NoteOffererStateEvent(OffererState::kSetLocalOfferResolved, false);
  // If it has ever been true, it stays true.
  EXPECT_TRUE(tracker_.document_uses_media());
}

}  // namespace blink
