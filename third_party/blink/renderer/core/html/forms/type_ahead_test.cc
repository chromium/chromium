// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/type_ahead.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

class TestTypeAheadDataSource final
    : public GarbageCollected<TestTypeAheadDataSource>,
      public TypeAheadDataSource {
 public:
  void set_selected_index(int index) { selected_index_ = index; }

  void Trace(Visitor* visitor) const override {
    TypeAheadDataSource::Trace(visitor);
  }

  // TypeAheadDataSource overrides:
  int IndexOfSelectedOption() const override { return selected_index_; }
  int OptionCount() const override { return 4; }
  String OptionAtIndex(int index) const override {
    switch (index) {
      case 0:
        return "aa";
      case 1:
        return "ab";
      case 2:
        return "ba";
      case 3:
        return "bb";
    }
    NOTREACHED();
  }

 private:
  int selected_index_ = -1;
};

class TypeAheadTest : public ::testing::Test {
 protected:
  TypeAheadTest()
      : test_source_(MakeGarbageCollected<TestTypeAheadDataSource>()),
        type_ahead_(MakeGarbageCollected<DisallowNewWrapper<TypeAhead>>(
            test_source_.Get())) {}

  TypeAhead& type_ahead() { return type_ahead_->Value(); }

  test::TaskEnvironment task_environment_;
  Persistent<TestTypeAheadDataSource> test_source_;
  Persistent<DisallowNewWrapper<TypeAhead>> type_ahead_;
};

TEST_F(TypeAheadTest, HasActiveSessionAtStart) {
  WebKeyboardEvent web_event(WebInputEvent::Type::kChar, 0,
                             base::TimeTicks() + base::Milliseconds(500));
  web_event.text[0] = ' ';
  auto& event = *KeyboardEvent::Create(web_event, nullptr);

  EXPECT_FALSE(type_ahead().HasActiveSession(event));
}

TEST_F(TypeAheadTest, HasActiveSessionAfterHandleEvent) {
  {
    WebKeyboardEvent web_event(WebInputEvent::Type::kChar, 0,
                               base::TimeTicks() + base::Milliseconds(500));
    web_event.text[0] = ' ';
    auto& event = *KeyboardEvent::Create(web_event, nullptr);
    type_ahead().HandleEvent(
        event, event.charCode(),
        TypeAhead::kMatchPrefix | TypeAhead::kCycleFirstChar);

    // A session should now be in progress.
    EXPECT_TRUE(type_ahead().HasActiveSession(event));
  }

  {
    // Should still be active after 1 second elapses.
    WebKeyboardEvent web_event(WebInputEvent::Type::kChar, 0,
                               base::TimeTicks() + base::Milliseconds(1500));
    web_event.text[0] = ' ';
    auto& event = *KeyboardEvent::Create(web_event, nullptr);
    EXPECT_TRUE(type_ahead().HasActiveSession(event));
  }

  {
    // But more than 1 second should be considered inactive.
    WebKeyboardEvent web_event(WebInputEvent::Type::kChar, 0,
                               base::TimeTicks() + base::Milliseconds(1501));
    web_event.text[0] = ' ';
    auto& event = *KeyboardEvent::Create(web_event, nullptr);
    EXPECT_FALSE(type_ahead().HasActiveSession(event));
  }
}

TEST_F(TypeAheadTest, HasActiveSessionAfterResetSession) {
  WebKeyboardEvent web_event(WebInputEvent::Type::kChar, 0,
                             base::TimeTicks() + base::Milliseconds(500));
  web_event.text[0] = ' ';
  auto& event = *KeyboardEvent::Create(web_event, nullptr);
  type_ahead().HandleEvent(
      event, event.charCode(),
      TypeAhead::kMatchPrefix | TypeAhead::kCycleFirstChar);

  // A session should now be in progress.
  EXPECT_TRUE(type_ahead().HasActiveSession(event));

  // But resetting it should make it go back to false.
  type_ahead().ResetSession();
  EXPECT_FALSE(type_ahead().HasActiveSession(event));
}

}  // namespace
}  // namespace blink
