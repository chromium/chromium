// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_session.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"

namespace content {

namespace {

const char kMediaSessionImageTestURL[] = "/media/session/image_test_page.html";
const char kMediaSessionImageTestPageVideoElement[] = "video";

const char kMediaSessionTestImagePath[] = "/media/session/test_image.jpg";

class MediaImageGetterHelper {
 public:
  MediaImageGetterHelper(content::MediaSession* media_session,
                         const media_session::MediaImage& image,
                         int min_size,
                         int desired_size) {
    media_session->GetMediaImageBitmap(
        image, min_size, desired_size,
        base::BindOnce(&MediaImageGetterHelper::OnComplete,
                       base::Unretained(this)));
  }

  void Wait() {
    if (bitmap_.has_value())
      return;

    run_loop_.Run();
  }

  const SkBitmap& bitmap() { return *bitmap_; }

 private:
  void OnComplete(const SkBitmap& bitmap) {
    bitmap_ = bitmap;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  base::Optional<SkBitmap> bitmap_;

  DISALLOW_COPY_AND_ASSIGN(MediaImageGetterHelper);
};

// Integration tests for content::MediaSession that do not take into
// consideration the implementation details contrary to
// MediaSessionImplBrowserTest.
class MediaSessionBrowserTestBase : public ContentBrowserTest {
 public:
  MediaSessionBrowserTestBase() {
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &MediaSessionBrowserTestBase::OnServerRequest, base::Unretained(this)));
  }

  void SetUp() override {
    ContentBrowserTest::SetUp();
    visited_urls_.clear();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void StartPlaybackAndWait(Shell* shell, const std::string& id) {
    shell->web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("document.querySelector('#" + id + "').play();"),
        base::NullCallback());
    WaitForStart(shell);
  }

  void StopPlaybackAndWait(Shell* shell, const std::string& id) {
    shell->web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("document.querySelector('#" + id + "').pause();"),
        base::NullCallback());
    WaitForStop(shell);
  }

  void WaitForStart(Shell* shell) {
    MediaStartStopObserver observer(shell->web_contents(),
                                    MediaStartStopObserver::Type::kStart);
    observer.Wait();
  }

  void WaitForStop(Shell* shell) {
    MediaStartStopObserver observer(shell->web_contents(),
                                    MediaStartStopObserver::Type::kStop);
    observer.Wait();
  }

  bool IsPlaying(Shell* shell, const std::string& id) {
    bool result;
    EXPECT_TRUE(
        ExecuteScriptAndExtractBool(shell->web_contents(),
                                    "window.domAutomationController.send("
                                    "!document.querySelector('#" +
                                        id + "').paused);",
                                    &result));
    return result;
  }

  bool WasURLVisited(const GURL& url) {
    base::AutoLock lock(visited_urls_lock_);
    return base::Contains(visited_urls_, url);
  }

  MediaSession* SetupMediaImageTest() {
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(kMediaSessionImageTestURL)));
    StartPlaybackAndWait(shell(), kMediaSessionImageTestPageVideoElement);

    MediaSession* media_session = MediaSession::Get(shell()->web_contents());

    std::vector<media_session::MediaImage> expected_images;
    expected_images.push_back(CreateTestImageWithSize(1));
    expected_images.push_back(CreateTestImageWithSize(10));

    media_session::test::MockMediaSessionMojoObserver observer(*media_session);
    observer.WaitForExpectedImagesOfType(
        media_session::mojom::MediaSessionImageType::kArtwork, expected_images);

    return media_session;
  }

  media_session::MediaImage CreateTestImageWithSize(int size) const {
    media_session::MediaImage image;
    image.src = GetTestImageURL();
    image.type = base::ASCIIToUTF16("image/jpeg");
    image.sizes.push_back(gfx::Size(size, size));
    return image;
  }

  GURL GetTestImageURL() const {
    return embedded_test_server()->GetURL(kMediaSessionTestImagePath);
  }

 private:
  void OnServerRequest(const net::test_server::HttpRequest& request) {
    // Note this method is called on the EmbeddedTestServer's background thread.
    base::AutoLock lock(visited_urls_lock_);
    visited_urls_.insert(request.GetURL());
  }

  // visited_urls_ is accessed both on the main thread and on the
  // EmbeddedTestServer's background thread via OnServerRequest(), so it must be
  // locked.
  base::Lock visited_urls_lock_;
  std::set<GURL> visited_urls_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionBrowserTestBase);
};

class MediaSessionBrowserTest : public MediaSessionBrowserTestBase {
 public:
  MediaSessionBrowserTest() {
    feature_list_.InitAndEnableFeature(media::kInternalMediaSession);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class MediaSessionBrowserTestWithoutInternalMediaSession
    : public MediaSessionBrowserTestBase {
 public:
  MediaSessionBrowserTestWithoutInternalMediaSession() {
    disabled_feature_list_.InitWithFeatures(
        {}, {media::kInternalMediaSession,
             media_session::features::kMediaSessionService});
  }

 private:
  base::test::ScopedFeatureList disabled_feature_list_;
};

// A MediaSessionBrowserTest with BackForwardCache enabled.
class MediaSessionBrowserTestWithBackForwardCache
    : public MediaSessionBrowserTestBase {
 public:
  MediaSessionBrowserTestWithBackForwardCache() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          {{"TimeToLiveInBackForwardCacheInSeconds", "3600"}}},
         {media::kInternalMediaSession, {}}},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    MediaSessionBrowserTestBase::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // anonymous namespace

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTestWithoutInternalMediaSession,
                       MediaSessionNoOpWhenDisabled) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            GetTestUrl("media/session", "media-session.html")));

  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_NE(nullptr, media_session);

  StartPlaybackAndWait(shell(), "long-video");
  StartPlaybackAndWait(shell(), "long-audio");

  media_session->Suspend(MediaSession::SuspendType::kSystem);
  StopPlaybackAndWait(shell(), "long-audio");

  // At that point, only "long-audio" is paused.
  EXPECT_FALSE(IsPlaying(shell(), "long-audio"));
  EXPECT_TRUE(IsPlaying(shell(), "long-video"));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, SimplePlayPause) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            GetTestUrl("media/session", "media-session.html")));

  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_NE(nullptr, media_session);

  StartPlaybackAndWait(shell(), "long-video");

  media_session->Suspend(MediaSession::SuspendType::kSystem);
  WaitForStop(shell());
  EXPECT_FALSE(IsPlaying(shell(), "long-video"));

  media_session->Resume(MediaSession::SuspendType::kSystem);
  WaitForStart(shell());
  EXPECT_TRUE(IsPlaying(shell(), "long-video"));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, MultiplePlayersPlayPause) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            GetTestUrl("media/session", "media-session.html")));

  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_NE(nullptr, media_session);

  StartPlaybackAndWait(shell(), "long-video");
  StartPlaybackAndWait(shell(), "long-audio");

  media_session->Suspend(MediaSession::SuspendType::kSystem);
  WaitForStop(shell());
  EXPECT_FALSE(IsPlaying(shell(), "long-video"));
  EXPECT_FALSE(IsPlaying(shell(), "long-audio"));

  media_session->Resume(MediaSession::SuspendType::kSystem);
  WaitForStart(shell());
  EXPECT_TRUE(IsPlaying(shell(), "long-video"));
  EXPECT_TRUE(IsPlaying(shell(), "long-audio"));
}

// Flaky on Mac. See https://crbug.com/980663
#if defined(OS_MACOSX)
#define MAYBE_WebContents_Muted DISABLED_WebContents_Muted
#else
#define MAYBE_WebContents_Muted WebContents_Muted
#endif
IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, MAYBE_WebContents_Muted) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            GetTestUrl("media/session", "media-session.html")));

  shell()->web_contents()->SetAudioMuted(true);
  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  ASSERT_NE(nullptr, media_session);

  StartPlaybackAndWait(shell(), "long-video");
  EXPECT_FALSE(media_session::test::GetMediaSessionInfoSync(media_session)
                   ->is_controllable);

  // Unmute the web contents and the player should be created.
  shell()->web_contents()->SetAudioMuted(false);
  EXPECT_TRUE(media_session::test::GetMediaSessionInfoSync(media_session)
                  ->is_controllable);

  // Now mute it again and the player should be removed.
  shell()->web_contents()->SetAudioMuted(true);
  EXPECT_FALSE(media_session::test::GetMediaSessionInfoSync(media_session)
                   ->is_controllable);
}

#if !defined(OS_ANDROID)
// On Android, System Audio Focus would break this test.
IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, MultipleTabsPlayPause) {
  Shell* other_shell = CreateBrowser();

  EXPECT_TRUE(NavigateToURL(shell(),
                            GetTestUrl("media/session", "media-session.html")));
  EXPECT_TRUE(NavigateToURL(other_shell,
                            GetTestUrl("media/session", "media-session.html")));

  MediaSession* media_session = MediaSession::Get(shell()->web_contents());
  MediaSession* other_media_session =
      MediaSession::Get(other_shell->web_contents());
  ASSERT_NE(nullptr, media_session);
  ASSERT_NE(nullptr, other_media_session);

  StartPlaybackAndWait(shell(), "long-video");
  StartPlaybackAndWait(other_shell, "long-video");

  media_session->Suspend(MediaSession::SuspendType::kSystem);
  WaitForStop(shell());
  EXPECT_FALSE(IsPlaying(shell(), "long-video"));
  EXPECT_TRUE(IsPlaying(other_shell, "long-video"));

  other_media_session->Suspend(MediaSession::SuspendType::kSystem);
  WaitForStop(other_shell);
  EXPECT_FALSE(IsPlaying(shell(), "long-video"));
  EXPECT_FALSE(IsPlaying(other_shell, "long-video"));

  media_session->Resume(MediaSession::SuspendType::kSystem);
  WaitForStart(shell());
  EXPECT_TRUE(IsPlaying(shell(), "long-video"));
  EXPECT_FALSE(IsPlaying(other_shell, "long-video"));

  other_media_session->Resume(MediaSession::SuspendType::kSystem);
  WaitForStart(other_shell);
  EXPECT_TRUE(IsPlaying(shell(), "long-video"));
  EXPECT_TRUE(IsPlaying(other_shell, "long-video"));
}
#endif  // defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, GetMediaImageBitmap) {
  ASSERT_TRUE(embedded_test_server()->Start());

  MediaSession* media_session = SetupMediaImageTest();
  ASSERT_NE(nullptr, media_session);

  media_session::MediaImage image;
  image.src = embedded_test_server()->GetURL("/media/session/test_image.jpg");
  image.type = base::ASCIIToUTF16("image/jpeg");
  image.sizes.push_back(gfx::Size(1, 1));

  MediaImageGetterHelper helper(media_session, CreateTestImageWithSize(1), 0,
                                10);
  helper.Wait();

  // The test image is a 1x1 test image.
  EXPECT_EQ(1, helper.bitmap().width());
  EXPECT_EQ(1, helper.bitmap().height());
  EXPECT_EQ(kRGBA_8888_SkColorType, helper.bitmap().colorType());

  EXPECT_TRUE(WasURLVisited(GetTestImageURL()));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       GetMediaImageBitmap_ImageTooSmall) {
  ASSERT_TRUE(embedded_test_server()->Start());

  MediaSession* media_session = SetupMediaImageTest();
  ASSERT_NE(nullptr, media_session);

  MediaImageGetterHelper helper(media_session, CreateTestImageWithSize(10), 10,
                                10);
  helper.Wait();

  // The |image| is too small but we do not know that until after we have
  // downloaded it. We should still receive a null image though.
  EXPECT_TRUE(helper.bitmap().isNull());
  EXPECT_TRUE(WasURLVisited(GetTestImageURL()));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       GetMediaImageBitmap_ImageTooSmall_BeforeDownload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  MediaSession* media_session = SetupMediaImageTest();
  ASSERT_NE(nullptr, media_session);

  MediaImageGetterHelper helper(media_session, CreateTestImageWithSize(1), 10,
                                10);
  helper.Wait();

  // Since |image| is too small but we know this in advance we should not
  // download it and instead we should receive a null image.
  EXPECT_TRUE(helper.bitmap().isNull());
  EXPECT_FALSE(WasURLVisited(GetTestImageURL()));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       GetMediaImageBitmap_InvalidImage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  MediaSession* media_session = SetupMediaImageTest();
  ASSERT_NE(nullptr, media_session);

  media_session::MediaImage image = CreateTestImageWithSize(1);
  image.src = embedded_test_server()->GetURL("/blank.jpg");

  MediaImageGetterHelper helper(media_session, image, 0, 10);
  helper.Wait();

  // Since |image| is not an image that is associated with the test page we
  // should not download it and instead we should receive a null image.
  EXPECT_TRUE(helper.bitmap().isNull());
  EXPECT_FALSE(WasURLVisited(image.src));
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTestWithBackForwardCache,
                       DoNotCacheIfMediaSessionExists) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page containing media.
  EXPECT_TRUE(NavigateToURL(shell(),
                            GetTestUrl("media/session", "media-session.html")));

  RenderFrameHost* rfh_a = shell()->web_contents()->GetMainFrame();
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page should not have been cached in the back forward cache.
  delete_observer_rfh_a.WaitUntilDeleted();
}

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTestWithBackForwardCache,
                       CachesPageWithoutMedia) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Navigate to a page not containing any media.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  RenderFrameHostImpl* rfh_a = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate away.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title1.html")));

  // The page should be cached in the back forward cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_TRUE(rfh_a->is_in_back_forward_cache());
}
}  // namespace content
