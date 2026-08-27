// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/listener_registration_phase_map.h"

#include <optional>

#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace extensions {

namespace {

using State = ListenerRegistrationPhaseMap::State;

constexpr char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";

class ListenerRegistrationPhaseMapTest : public testing::Test {
 protected:
  content::BrowserContext& browser_context() { return browser_context_; }
  content::BrowserContext& other_browser_context() {
    return other_browser_context_;
  }
  ListenerRegistrationPhaseMap& phases() { return phases_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  content::TestBrowserContext other_browser_context_;
  ListenerRegistrationPhaseMap phases_;
};

// Tests that Commit() requires a started phase for the matching worker
// instance and only succeeds once.
TEST_F(ListenerRegistrationPhaseMapTest, Commit) {
  const blink::ServiceWorkerToken kWorkerToken;
  const blink::ServiceWorkerToken kOtherWorkerToken;

  phases().Start(kExtensionId, browser_context(), kWorkerToken);

  // A completion from another worker instance does not commit.
  EXPECT_FALSE(
      phases().Commit(kExtensionId, browser_context(), kOtherWorkerToken));
  EXPECT_EQ(State::kStarted,
            phases().GetState(kExtensionId, browser_context()));

  // The matching instance commits, but only once.
  EXPECT_TRUE(phases().Commit(kExtensionId, browser_context(), kWorkerToken));
  EXPECT_FALSE(phases().Commit(kExtensionId, browser_context(), kWorkerToken));
  EXPECT_EQ(State::kCommitted,
            phases().GetState(kExtensionId, browser_context()));
}

// Tests that AbortForInstance() ignores stop signals from other worker
// instances, and that committed phases cannot be aborted.
TEST_F(ListenerRegistrationPhaseMapTest, Abort) {
  const blink::ServiceWorkerToken kWorkerToken;
  const blink::ServiceWorkerToken kStaleWorkerToken;
  const blink::ServiceWorkerToken kNextWorkerToken;

  phases().Start(kExtensionId, browser_context(), kWorkerToken);

  // Stop signals from other worker instances do not abort the phase.
  phases().AbortForInstance(kExtensionId, browser_context(), kStaleWorkerToken);
  EXPECT_EQ(State::kStarted,
            phases().GetState(kExtensionId, browser_context()));

  // A matching stop signal aborts the phase, preventing subsequent commits.
  phases().AbortForInstance(kExtensionId, browser_context(), kWorkerToken);
  EXPECT_FALSE(phases().Commit(kExtensionId, browser_context(), kWorkerToken));
  EXPECT_EQ(State::kAborted,
            phases().GetState(kExtensionId, browser_context()));

  // A committed phase cannot be aborted.
  phases().Start(kExtensionId, browser_context(), kNextWorkerToken);
  EXPECT_TRUE(
      phases().Commit(kExtensionId, browser_context(), kNextWorkerToken));
  phases().Abort(kExtensionId, browser_context());
  phases().AbortForInstance(kExtensionId, browser_context(), kNextWorkerToken);
  EXPECT_EQ(State::kCommitted,
            phases().GetState(kExtensionId, browser_context()));
}

// Tests that Start() replaces a finished phase for a new worker instance.
TEST_F(ListenerRegistrationPhaseMapTest, StartReplacesFinishedPhase) {
  const blink::ServiceWorkerToken kCommittedWorkerToken;
  const blink::ServiceWorkerToken kNewWorkerToken;

  phases().Start(kExtensionId, browser_context(), kCommittedWorkerToken);
  EXPECT_TRUE(
      phases().Commit(kExtensionId, browser_context(), kCommittedWorkerToken));

  // Starting a new instance replaces the committed phase and invalidates the
  // previous worker token.
  phases().Start(kExtensionId, browser_context(), kNewWorkerToken);
  EXPECT_EQ(State::kStarted,
            phases().GetState(kExtensionId, browser_context()));
  EXPECT_FALSE(
      phases().Commit(kExtensionId, browser_context(), kCommittedWorkerToken));
  EXPECT_TRUE(
      phases().Commit(kExtensionId, browser_context(), kNewWorkerToken));
}

// Tests that RemoveAllForContext() clears the phases in one context, and
// RemoveAllForExtension() clears an extension's phases across all contexts.
TEST_F(ListenerRegistrationPhaseMapTest, RemoveAll) {
  const blink::ServiceWorkerToken kWorkerToken;
  const blink::ServiceWorkerToken kOtherContextWorkerToken;

  phases().Start(kExtensionId, browser_context(), kWorkerToken);
  phases().Start(kExtensionId, other_browser_context(),
                 kOtherContextWorkerToken);
  EXPECT_TRUE(phases().Commit(kExtensionId, other_browser_context(),
                              kOtherContextWorkerToken));

  phases().RemoveAllForContext(browser_context());
  EXPECT_EQ(std::nullopt, phases().GetState(kExtensionId, browser_context()));
  EXPECT_EQ(State::kCommitted,
            phases().GetState(kExtensionId, other_browser_context()));

  phases().RemoveAllForExtension(kExtensionId);
  EXPECT_EQ(std::nullopt,
            phases().GetState(kExtensionId, other_browser_context()));
}

}  // namespace

}  // namespace extensions
