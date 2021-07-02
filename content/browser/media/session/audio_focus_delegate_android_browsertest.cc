// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/media/session/mock_media_session_player_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_content_type.h"

namespace content {

class AudioFocusDelegateAndroidBrowserTest : public ContentBrowserTest {};

// MAYBE_OnAudioFocusChangeAfterDtorCrash will hit a DCHECK before the crash, it
// is the only way found to actually reproduce the crash so as a result, the
// test will only run on builds without DCHECK's.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// TODO(crbug.com/602787) The test is flaky, disabling it everywhere.
#define MAYBE_OnAudioFocusChangeAfterDtorCrash \
  DISABLED_OnAudioFocusChangeAfterDtorCrash
#else
#define MAYBE_OnAudioFocusChangeAfterDtorCrash \
  DISABLED_OnAudioFocusChangeAfterDtorCrash
#endif

IN_PROC_BROWSER_TEST_F(AudioFocusDelegateAndroidBrowserTest,
                       MAYBE_OnAudioFocusChangeAfterDtorCrash) {
  std::unique_ptr<MockMediaSessionPlayerObserver> player_observer(
      new MockMediaSessionPlayerObserver);

  MediaSessionImpl* media_session =
      MediaSessionImpl::Get(shell()->web_contents());
  ASSERT_TRUE(media_session);

  WebContents* other_web_contents = CreateBrowser()->web_contents();
  MediaSessionImpl* other_media_session =
      MediaSessionImpl::Get(other_web_contents);
  ASSERT_TRUE(other_media_session);

  player_observer->StartNewPlayer();
  media_session->AddPlayer(player_observer.get(), 0,
                           media::MediaContentType::Persistent);
  EXPECT_TRUE(media_session->IsActive());
  EXPECT_FALSE(other_media_session->IsActive());

  player_observer->StartNewPlayer();
  other_media_session->AddPlayer(player_observer.get(), 1,
                                 media::MediaContentType::Persistent);
  EXPECT_TRUE(media_session->IsActive());
  EXPECT_TRUE(other_media_session->IsActive());

  shell()->CloseAllWindows();

  // Give some time to the AudioFocusManager to send an audioFocusChange message
  // to the listeners. If the bug is still present, it will crash.
  {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(1));
    run_loop.Run();
  }
}

}  // namespace content
