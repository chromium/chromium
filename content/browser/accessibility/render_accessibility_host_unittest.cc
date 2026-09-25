// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/render_accessibility_host.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_updates_and_events.h"

namespace content {

class RenderAccessibilityHostTest : public testing::Test {
 public:
  RenderAccessibilityHostTest() = default;

 protected:
  BrowserTaskEnvironment task_environment_;
};

TEST_F(RenderAccessibilityHostTest, HandleAXEvents_ValidIds) {
  mojo::Remote<blink::mojom::RenderAccessibilityHost> remote;
  RenderAccessibilityHost host(nullptr, ui::AXTreeID::CreateNewAXTreeID());
  host.Bind(remote.BindNewPipeAndPassReceiver());

  mojo::test::BadMessageObserver bad_message_observer;
  ui::AXUpdatesAndEvents updates_and_events;
  ui::AXTreeUpdate update;
  update.root_id = 1;
  updates_and_events.updates.push_back(update);
  ui::AXLocationAndScrollUpdates location_and_scroll_updates;

  base::RunLoop run_loop;
  remote->HandleAXEvents(updates_and_events, location_and_scroll_updates, 0,
                         run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(RenderAccessibilityHostTest, HandleAXEvents_InvalidEventId) {
  mojo::Remote<blink::mojom::RenderAccessibilityHost> remote;
  RenderAccessibilityHost host(nullptr, ui::AXTreeID::CreateNewAXTreeID());
  host.Bind(remote.BindNewPipeAndPassReceiver());

  mojo::test::BadMessageObserver bad_message_observer;
  ui::AXUpdatesAndEvents updates_and_events;
  ui::AXEvent event;
  event.id = ui::kFirstGeneratedBrowserNodeID;
  updates_and_events.events.push_back(event);
  ui::AXLocationAndScrollUpdates location_and_scroll_updates;

  remote->HandleAXEvents(updates_and_events, location_and_scroll_updates, 0,
                         base::DoNothing());
  EXPECT_EQ("Invalid AXNodeID from renderer.",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(RenderAccessibilityHostTest, HandleAXEvents_InvalidUpdateId) {
  mojo::Remote<blink::mojom::RenderAccessibilityHost> remote;
  RenderAccessibilityHost host(nullptr, ui::AXTreeID::CreateNewAXTreeID());
  host.Bind(remote.BindNewPipeAndPassReceiver());

  mojo::test::BadMessageObserver bad_message_observer;
  ui::AXUpdatesAndEvents updates_and_events;
  ui::AXTreeUpdate update;
  update.root_id = ui::kFirstGeneratedBrowserNodeID;
  updates_and_events.updates.push_back(update);
  ui::AXLocationAndScrollUpdates location_and_scroll_updates;

  remote->HandleAXEvents(updates_and_events, location_and_scroll_updates, 0,
                         base::DoNothing());
  EXPECT_EQ("Invalid AXNodeID from renderer.",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(RenderAccessibilityHostTest, HandleAXEvents_InvalidLocationChangeId) {
  mojo::Remote<blink::mojom::RenderAccessibilityHost> remote;
  RenderAccessibilityHost host(nullptr, ui::AXTreeID::CreateNewAXTreeID());
  host.Bind(remote.BindNewPipeAndPassReceiver());

  mojo::test::BadMessageObserver bad_message_observer;
  ui::AXUpdatesAndEvents updates_and_events;
  ui::AXLocationAndScrollUpdates location_and_scroll_updates;
  ui::AXLocationChange loc;
  loc.id = ui::kFirstGeneratedBrowserNodeID;
  location_and_scroll_updates.location_changes.push_back(loc);

  remote->HandleAXEvents(updates_and_events, location_and_scroll_updates, 0,
                         base::DoNothing());
  EXPECT_EQ("Invalid AXNodeID from renderer.",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(RenderAccessibilityHostTest, HandleAXLocationChanges_ValidId) {
  mojo::Remote<blink::mojom::RenderAccessibilityHost> remote;
  RenderAccessibilityHost host(nullptr, ui::AXTreeID::CreateNewAXTreeID());
  host.Bind(remote.BindNewPipeAndPassReceiver());

  mojo::test::BadMessageObserver bad_message_observer;
  ui::AXLocationAndScrollUpdates changes;
  ui::AXLocationChange loc;
  loc.id = 1;
  changes.location_changes.push_back(loc);

  remote->HandleAXLocationChanges(changes, 0);
  remote.FlushForTesting();

  EXPECT_FALSE(bad_message_observer.got_bad_message());
}

TEST_F(RenderAccessibilityHostTest, HandleAXLocationChanges_InvalidId) {
  mojo::Remote<blink::mojom::RenderAccessibilityHost> remote;
  RenderAccessibilityHost host(nullptr, ui::AXTreeID::CreateNewAXTreeID());
  host.Bind(remote.BindNewPipeAndPassReceiver());

  mojo::test::BadMessageObserver bad_message_observer;
  ui::AXLocationAndScrollUpdates changes;
  ui::AXLocationChange loc;
  loc.id = ui::kFirstGeneratedBrowserNodeID;
  changes.location_changes.push_back(loc);

  remote->HandleAXLocationChanges(changes, 0);
  EXPECT_EQ("Invalid AXNodeID from renderer.",
            bad_message_observer.WaitForBadMessage());
}

}  // namespace content
