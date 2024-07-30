// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspected_frames.h"

#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class InspectedFramesTest : public testing::Test {
 public:
  InspectedFramesTest() = default;
  ~InspectedFramesTest() override = default;

 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(InspectedFramesTest, FindsFrameForGivenStorageKey) {
  auto security_origin =
      SecurityOrigin::CreateFromString("http://example.site");
  auto nonce = base::UnguessableToken::Create();
  auto blink_storage_key =
      BlinkStorageKey::CreateWithNonce(security_origin, nonce);

  auto page_holder = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), nullptr, nullptr, base::NullCallback());
  LocalDOMWindow* dom_window = page_holder->GetFrame().DomWindow();
  dom_window->SetStorageKey(blink_storage_key);

  InspectedFrames* inspected_frames =
      MakeGarbageCollected<InspectedFrames>(&page_holder->GetFrame());
  std::string storage_key =
      static_cast<StorageKey>(blink_storage_key).Serialize();

  EXPECT_EQ(page_holder->GetFrame(),
            inspected_frames->FrameWithStorageKey(WTF::String(storage_key)));
}

}  // namespace blink
