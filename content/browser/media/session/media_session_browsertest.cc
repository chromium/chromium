// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_session.h"

#include <optional>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/public/test/test_media_session_client.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/media_session/public/cpp/features.h"
#include "services/media_session/public/cpp/test/audio_focus_test_util.h"
#include "services/media_session/public/cpp/test/mock_media_session.h"

namespace {
int hidden_metadata_placeholder_thumbnail_size = 42;
}  // namespace

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

  MediaImageGetterHelper(const MediaImageGetterHelper&) = delete;
  MediaImageGetterHelper& operator=(const MediaImageGetterHelper&) = delete;

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
  std::optional<SkBitmap> bitmap_;
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

  MediaSessionBrowserTestBase(const MediaSessionBrowserTestBase&) = delete;
  MediaSessionBrowserTestBase& operator=(const MediaSessionBrowserTestBase&) =
      delete;

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
    shell->web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"document.querySelector('#" + base::ASCIIToUTF16(id) + u"').play();",
        base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
    WaitForStart(shell);
  }

  void StopPlaybackAndWait(Shell* shell, const std::string& id) {
    shell->web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"document.querySelector('#" + base::ASCIIToUTF16(id) + u"').pause();",
        base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
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
    return EvalJs(shell->web_contents(),
                  "!document.querySelector('#" + id + "').paused;")
        .ExtractBool();
  }

  bool WasURLVisited(const GURL& url) {
    base::AutoLock lock(visited_urls_lock_);
    return base::Contains(visited_urls_, url);
  }

  MediaSession* SetupMediaImageTest(bool expect_media_image = true) {
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(kMediaSessionImageTestURL)));
    StartPlaybackAndWait(shell(), kMediaSessionImageTestPageVideoElement);

    MediaSession* media_session = MediaSession::Get(shell()->web_contents());

    if (expect_media_image) {
      std::vector<media_session::MediaImage> expected_images;

      expected_images.push_back(CreateTestImageWithSize(1));
      expected_images.push_back(CreateTestImageWithSize(10));
      media_session::test::MockMediaSessionMojoObserver observer(
          *media_session);
      observer.WaitForExpectedImagesOfType(
          media_session::mojom::MediaSessionImageType::kArtwork,
          expected_images);
    }

    return media_session;
  }

  media_session::MediaImage CreateTestImageWithSize(int size) const {
    media_session::MediaImage image;
    image.src = GetTestImageURL();
    image.type = u"image/jpeg";
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
};

class MediaSessionBrowserTest : public MediaSessionBrowserTestBase {
 public:
  MediaSessionBrowserTest() {
    feature_list_.InitAndEnableFeature(media::kInternalMediaSession);
  }

  void SetUp() override {
    SetupMediaSessionClient();

    MediaSessionBrowserTestBase::SetUp();
  }

 protected:
  void SetupMediaSessionClient() {
    SkBitmap placeholder_bitmap;
    placeholder_bitmap.allocN32Pixels(
        hidden_metadata_placeholder_thumbnail_size,
        hidden_metadata_placeholder_thumbnail_size);
    client_.SetThumbnailPlaceholder(placeholder_bitmap);
  }

  TestMediaSessionClient client_;

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
#if BUILDFLAG(IS_MAC)
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

#if !BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, GetMediaImageBitmap) {
  ASSERT_TRUE(embedded_test_server()->Start());

  MediaSession* media_session = SetupMediaImageTest();
  ASSERT_NE(nullptr, media_session);

  MediaImageGetterHelper helper(media_session, CreateTestImageWithSize(1), 0,
                                10);
  helper.Wait();

  // The test image is a 1x1 test image.
  EXPECT_EQ(1, helper.bitmap().width());
  EXPECT_EQ(1, helper.bitmap().height());
  EXPECT_EQ(kRGBA_8888_SkColorType, helper.bitmap().colorType());

  EXPECT_TRUE(WasURLVisited(GetTestImageURL()));
}

// We hide the media image from CrOS' media controls by replacing the image in
// the MediaSessionImpl with a placeholder image. These changes are gated to
// only affect ChromeOS, hence why the testing for this is also ChromeOS only.
#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, HideMediaMetadataImageInCrOS) {
  client_.SetShouldHideMetadata(true);

  ASSERT_TRUE(embedded_test_server()->Start());

  // We don't expect a media image because of the way the image will be replaced
  // with its placeholder in MediaSessionImpl.
  MediaSession* media_session =
      SetupMediaImageTest(/*expect_media_image=*/false);
  ASSERT_NE(nullptr, media_session);

  MediaImageGetterHelper helper(media_session, CreateTestImageWithSize(1), 0,
                                10);

  helper.Wait();

  EXPECT_EQ(hidden_metadata_placeholder_thumbnail_size,
            helper.bitmap().width());
  EXPECT_EQ(hidden_metadata_placeholder_thumbnail_size,
            helper.bitmap().height());

  // As we are replacing the image, we should not visit the original's URL.
  EXPECT_FALSE(WasURLVisited(GetTestImageURL()));
}
#else  // !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest,
                       DontHideMediaMetadataImageInNonCrOS) {
  client_.SetShouldHideMetadata(true);

  ASSERT_TRUE(embedded_test_server()->Start());

  MediaSession* media_session = SetupMediaImageTest();
  ASSERT_NE(nullptr, media_session);

  MediaImageGetterHelper helper(media_session, CreateTestImageWithSize(1), 0,
                                10);
  helper.Wait();

  // The test image is a 1x1 test image.
  EXPECT_EQ(1, helper.bitmap().width());
  EXPECT_EQ(1, helper.bitmap().height());
  EXPECT_EQ(kRGBA_8888_SkColorType, helper.bitmap().colorType());

  EXPECT_TRUE(WasURLVisited(GetTestImageURL()));
}
#endif

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

// Regression test of crbug.com/1195769.
IN_PROC_BROWSER_TEST_F(MediaSessionBrowserTest, ChangeMediaElementDocument) {
  ASSERT_TRUE(NavigateToURL(
      shell(), GetTestUrl("media/session", "change_document.html")));
  ASSERT_TRUE(ExecJs(shell()->web_contents(), "moveAudioToSubframe();"));

  ASSERT_EQ(true, EvalJs(shell(), "play();"));
  MediaSession* const media_session =
      MediaSession::Get(shell()->web_contents());
  media_session->Suspend(MediaSession::SuspendType::kUI);
  WaitForStop(shell());
}

}  // namespace content
