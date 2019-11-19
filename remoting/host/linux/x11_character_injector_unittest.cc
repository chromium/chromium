// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/x11_character_injector.h"

#include <unordered_map>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/host/linux/x11_keyboard.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  constexpr base::TimeDelta kKeycodeReuseDuration =
      base::TimeDelta::FromMilliseconds(100);
}

namespace remoting {

// This class acts as a simulated keyboard interface for testing that
// * Maintains a changeable simulated keyboard layout.
// * Verifies the correct sequence of characters are being typed.
// * Ensures that a key mapping can't be changed if the time elapsed since
//   last used is less than |kKeycodeReuseDuration|.
class FakeX11Keyboard : public X11Keyboard {
 public:
  struct MappingInfo {
    uint32_t code_point;
    base::TimeTicks reusable_at;
  };

  explicit FakeX11Keyboard(const std::vector<uint32_t>& available_keycodes);
  ~FakeX11Keyboard() override;

  // X11Keyboard overrides.
  std::vector<uint32_t> GetUnusedKeycodes() override;

  void PressKey(uint32_t keycode, uint32_t modifiers) override;
  bool FindKeycode(uint32_t code_point,
                   uint32_t* keycode,
                   uint32_t* modifiers) override;
  bool ChangeKeyMapping(uint32_t keycode, uint32_t code_point) override;
  void Flush() override;
  void Sync() override;

  void ExpectEnterCodePoints(const std::vector<uint32_t>& sequence);

  // Sets a callback to be called when the keypress expectation queue becomes
  // empty.
  void SetKeyPressFinishedCallback(const base::Closure& callback) {
    keypress_finished_callback_ = callback;
  }

 private:
  std::unordered_map<uint32_t, MappingInfo> keycode_mapping_;
  base::circular_deque<uint32_t> expected_code_point_sequence_;
  base::Closure keypress_finished_callback_;
};

FakeX11Keyboard::FakeX11Keyboard(
    const std::vector<uint32_t>& available_keycodes) {
  for (uint32_t keycode : available_keycodes) {
    keycode_mapping_.insert({keycode, {0, base::TimeTicks()}});
  }
}

FakeX11Keyboard::~FakeX11Keyboard() {
  EXPECT_TRUE(expected_code_point_sequence_.empty());
  for (const auto& pair : keycode_mapping_) {
    EXPECT_EQ(0u, pair.second.code_point);
  }
}

std::vector<uint32_t> FakeX11Keyboard::GetUnusedKeycodes() {
  std::vector<uint32_t> keycodes;
  for (const auto& pair : keycode_mapping_) {
    if (!pair.second.code_point) {
      keycodes.push_back(pair.first);
    }
  }
  return keycodes;
}

void FakeX11Keyboard::PressKey(uint32_t keycode, uint32_t modifiers) {
  ASSERT_FALSE(expected_code_point_sequence_.empty());
  uint32_t expected_code_point = expected_code_point_sequence_.front();
  auto position = keycode_mapping_.find(keycode);
  ASSERT_NE(position, keycode_mapping_.end());
  MappingInfo& info = position->second;
  EXPECT_EQ(expected_code_point, info.code_point);
  info.reusable_at = base::TimeTicks::Now() + kKeycodeReuseDuration;
  expected_code_point_sequence_.pop_front();
  if (expected_code_point_sequence_.empty() && keypress_finished_callback_) {
    keypress_finished_callback_.Run();
  }
}

bool FakeX11Keyboard::FindKeycode(uint32_t code_point,
                                  uint32_t* keycode,
                                  uint32_t* modifiers) {
  auto position = std::find_if(keycode_mapping_.begin(), keycode_mapping_.end(),
               [code_point](const std::pair<uint32_t, MappingInfo>& pair) {
    return pair.second.code_point == code_point;
  });
  if (position == keycode_mapping_.end()) {
    return false;
  }
  *keycode = position->first;
  *modifiers = 0;
  return true;
}

bool FakeX11Keyboard::ChangeKeyMapping(uint32_t keycode, uint32_t code_point) {
  MappingInfo& info = keycode_mapping_[keycode];
  info.code_point = code_point;
  if (code_point) {
    base::TimeTicks now = base::TimeTicks::Now();
    EXPECT_LE(info.reusable_at, now)
        << "Attempted to reuse a keycode in less than "
        << kKeycodeReuseDuration;
    info.reusable_at = now + kKeycodeReuseDuration;
  } else {
    info.reusable_at = base::TimeTicks();
  }
  return true;
}

void FakeX11Keyboard::Flush() {}

void FakeX11Keyboard::Sync() {}

void FakeX11Keyboard::ExpectEnterCodePoints(
    const std::vector<uint32_t>& sequence) {
  expected_code_point_sequence_.insert(expected_code_point_sequence_.end(),
              sequence.begin(), sequence.end());
}

class X11CharacterInjectorTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  // Injects the characters sequentially, verifies the sequence of characters
  // being injected are correct, and runs the message loop until all
  // characters are injected.
  void InjectAndRun(const std::vector<uint32_t>& code_points);

  std::unique_ptr<X11CharacterInjector> injector_;
  FakeX11Keyboard* keyboard_;  // Owned by |injector_|.

  base::test::SingleThreadTaskEnvironment task_environment_;
};

void X11CharacterInjectorTest::SetUp() {
  keyboard_ = new FakeX11Keyboard({55, 54, 53, 52, 51});
  injector_.reset(new X11CharacterInjector(base::WrapUnique(keyboard_)));
}

void X11CharacterInjectorTest::TearDown() {
  injector_.reset();
}

void X11CharacterInjectorTest::InjectAndRun(
    const std::vector<uint32_t>& code_points) {
  base::RunLoop run_loop;
  keyboard_->SetKeyPressFinishedCallback(run_loop.QuitClosure());
  for (uint32_t code_point : code_points)
    injector_->Inject(code_point);
  keyboard_->ExpectEnterCodePoints(code_points);
  run_loop.Run();
}

TEST_F(X11CharacterInjectorTest, TestNoMappingNoExpectation) {
}

TEST_F(X11CharacterInjectorTest, TestTypeOneCharacter) {
  InjectAndRun({123});
}

TEST_F(X11CharacterInjectorTest, TestMapCharactersUntilFull) {
  InjectAndRun({1, 2, 3, 4, 5});
}

TEST_F(X11CharacterInjectorTest, TestMapOneCharacterWhenFull) {
  InjectAndRun({1, 2, 3, 4, 5, 6});
}

TEST_F(X11CharacterInjectorTest, TestReuseMappedCharacterOnce) {
  InjectAndRun({1, 2, 3, 4, 5});
  InjectAndRun({1, 6});
}

TEST_F(X11CharacterInjectorTest, TestReuseAllMappedCharactersInChangedOrder) {
  InjectAndRun({1, 2, 3, 4, 5});
  InjectAndRun({2, 4, 5, 1, 3});
  InjectAndRun({31, 32, 33, 34, 35});
}

}  // namespace remoting
