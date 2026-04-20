// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_session_creation_observer_win.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/win/core_audio_util_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioSessionCreationObserverWinTest : public testing::Test {
 public:
  AudioSessionCreationObserverWinTest() = default;
  ~AudioSessionCreationObserverWinTest() override = default;

 protected:
  base::win::ScopedCOMInitializer com_init_;
};

TEST_F(AudioSessionCreationObserverWinTest, NotifiesCallback) {
  base::MockRepeatingClosure session_created_callback;
  auto observer = Microsoft::WRL::Make<AudioSessionCreationObserverWin>(
      session_created_callback.Get());

  EXPECT_CALL(session_created_callback, Run()).Times(1);
  observer->OnSessionCreated(nullptr);
}

}  // namespace media
