// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/listener_registration_phase_map.h"

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace extensions {

namespace {

using ListenerSet = ListenerRegistrationPhaseMap::ListenerSet;
using State = ListenerRegistrationPhaseMap::State;
using FinishedPhase = std::pair<ExtensionId, State>;

constexpr char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";

class ListenerRegistrationPhaseMapTest : public testing::Test {
 protected:
  content::BrowserContext& browser_context() { return browser_context_; }
  content::BrowserContext& other_browser_context() {
    return other_browser_context_;
  }
  ListenerRegistrationPhaseMap& phases() { return phases_; }

  const std::vector<FinishedPhase>& finished_phases() const {
    return finished_phases_;
  }

 private:
  void OnFinished(const ExtensionId& extension_id,
                  content::BrowserContext&,
                  State state) {
    finished_phases_.emplace_back(extension_id, state);
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  content::TestBrowserContext other_browser_context_;
  std::vector<FinishedPhase> finished_phases_;
  ListenerRegistrationPhaseMap phases_{
      base::BindRepeating(&ListenerRegistrationPhaseMapTest::OnFinished,
                          base::Unretained(this))};
};

// Tests that Commit() requires a started phase for the matching worker
// instance, succeeds at most once, and runs the finished callback only when it
// succeeds.
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
  EXPECT_EQ(finished_phases(),
            (std::vector<FinishedPhase>{{kExtensionId, State::kCommitted}}));
}

// Tests that AbortForInstance() ignores stop signals from other worker
// instances, and that committed phases cannot be aborted. No-op aborts don't
// run the finished callback.
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
  EXPECT_EQ(finished_phases(),
            (std::vector<FinishedPhase>{{kExtensionId, State::kAborted},
                                        {kExtensionId, State::kCommitted}}));
}

// Tests adding, querying, merging, and removing filtered and unfiltered
// entries in `ListenerSet`.
TEST_F(ListenerRegistrationPhaseMapTest, ListenerSet) {
  const base::DictValue filter =
      base::DictValue().Set("hostSuffix", "example.com");

  // Adding the same filter twice keeps a single entry.
  ListenerSet listeners;
  listeners.Add("test.onEvent", nullptr);
  listeners.Add("test.onFiltered", &filter);
  listeners.Add("test.onFiltered", &filter);
  EXPECT_EQ(listeners.events, std::set<std::string>{"test.onEvent"});
  EXPECT_EQ(listeners.filtered_events,
            base::DictValue().Set("test.onFiltered",
                                  base::ListValue().Append(filter.Clone())));
  EXPECT_TRUE(listeners.Has("test.onEvent", nullptr));
  EXPECT_TRUE(listeners.Has("test.onFiltered", &filter));
  EXPECT_FALSE(listeners.Has("test.onEvent", &filter));
  EXPECT_FALSE(listeners.Has("test.onFiltered", nullptr));
  EXPECT_EQ(listeners.Clone(), listeners);

  // `AddAll()` merges new entries without duplicating existing filters.
  ListenerSet other;
  other.Add("test.onOther", nullptr);
  other.Add("test.onFiltered", &filter);
  other.Add("test.onOtherFiltered", &filter);
  listeners.AddAll(other);
  EXPECT_EQ(listeners.events,
            (std::set<std::string>{"test.onEvent", "test.onOther"}));
  EXPECT_EQ(
      listeners.filtered_events,
      base::DictValue()
          .Set("test.onFiltered", base::ListValue().Append(filter.Clone()))
          .Set("test.onOtherFiltered",
               base::ListValue().Append(filter.Clone())));

  // Removing an event's last filter also removes its key from
  // `filtered_events`.
  listeners.Remove("test.onFiltered", &filter);
  listeners.Remove("test.onEvent", nullptr);
  EXPECT_EQ(listeners.events, std::set<std::string>{"test.onOther"});
  EXPECT_EQ(listeners.filtered_events,
            base::DictValue().Set("test.onOtherFiltered",
                                  base::ListValue().Append(filter.Clone())));
}

// Tests that a phase's listeners remain accessible after `Commit()`, and that
// `Start()` for a new worker instance resets both the phase state and its
// listeners.
TEST_F(ListenerRegistrationPhaseMapTest, StartReplacesFinishedPhase) {
  const blink::ServiceWorkerToken kCommittedWorkerToken;
  const blink::ServiceWorkerToken kNewWorkerToken;

  EXPECT_FALSE(phases().GetListeners(kExtensionId, browser_context()));
  phases().Start(kExtensionId, browser_context(), kCommittedWorkerToken);
  ListenerSet* listeners =
      phases().GetListeners(kExtensionId, browser_context());
  ASSERT_TRUE(listeners);
  listeners->Add("test.onEvent", nullptr);
  EXPECT_TRUE(
      phases().Commit(kExtensionId, browser_context(), kCommittedWorkerToken));
  listeners = phases().GetListeners(kExtensionId, browser_context());
  ASSERT_TRUE(listeners);
  EXPECT_EQ(listeners->events, std::set<std::string>{"test.onEvent"});

  // Starting a new instance resets the phase (and its listener set) and
  // invalidates the previous worker token.
  phases().Start(kExtensionId, browser_context(), kNewWorkerToken);
  EXPECT_EQ(State::kStarted,
            phases().GetState(kExtensionId, browser_context()));
  listeners = phases().GetListeners(kExtensionId, browser_context());
  ASSERT_TRUE(listeners);
  EXPECT_EQ(*listeners, ListenerSet());
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
