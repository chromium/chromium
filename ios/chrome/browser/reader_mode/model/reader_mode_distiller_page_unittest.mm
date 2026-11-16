// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_distiller_page.h"

#import <memory>
#import <string>
#import <tuple>
#import <utility>

#import "base/strings/utf_string_conversions.h"
#import "base/test/test_future.h"
#import "components/web_distiller/core/distiller_options.h"
#import "components/web_distiller/core/extraction_utils.h"
#import "components/web_distiller/core/proto/distilled_page.pb.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/test/ios_chrome_test_with_web_state.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
using ::testing::HasSubstr;
using ::testing::Not;

// Distilled page and parse result reported by a completed distillation.
struct DistillationOutcome {
  std::unique_ptr<web_distiller::DistilledPageProto> page;
  web_distiller::DistillationParseResult parse_result =
      web_distiller::DistillationParseResult::kParseFailure;
};

// Runs `DistillPage` on `distiller_page` for `url`, waits for the completion
// callback, and returns the distillation outcome.
DistillationOutcome RunDistillation(ReaderModeDistillerPage& distiller_page,
                                    const GURL& url) {
  base::test::TestFuture<std::unique_ptr<web_distiller::DistilledPageProto>,
                         web_distiller::DistillationParseResult>
      future;
  distiller_page.DistillPage(url, web_distiller::DistillerOptions(),
                             future.GetCallback());
  auto [page, parse_result] = future.Take();
  return {std::move(page), parse_result};
}
}  // namespace

class ReaderModeDistillerPageTest : public PlatformTest {
 public:
  ReaderModeDistillerPageTest() : valid_url_(GURL("https://test.url")) {
    distiller_page_ = std::make_unique<ReaderModeDistillerPage>(&web_state_);

    distiller_script_ =
        web_distiller::GetDistillerScript(web_distiller::DistillerOptions());

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = frames_manager.get();
    web_state_.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                   std::move(frames_manager));

    // Use a fake web frame to return a custom result after JS execution.
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame(valid_url());
    web_frame_ = main_frame.get();
    web_frames_manager_->AddWebFrame(std::move(main_frame));
  }

  const GURL& valid_url() { return valid_url_; }

  const std::u16string distiller_script_utf16() {
    return base::UTF8ToUTF16(distiller_script_);
  }

  web::FakeWebFrame* web_frame() { return web_frame_; }

  void RemoveMainWebFrame() {
    std::string frame_id = web_frame_->GetFrameId();
    web_frame_ = nullptr;
    web_frames_manager_->RemoveWebFrame(frame_id);
  }

  // Runs `DistillPage` on `url` and returns the parse result.
  web_distiller::DistillationParseResult DistillPage(const GURL& url) {
    return RunDistillation(*distiller_page_, url).parse_result;
  }

 private:
  web::WebTaskEnvironment task_environment_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_;
  raw_ptr<web::FakeWebFrame> web_frame_;

  GURL valid_url_;
  std::string distiller_script_;
  std::unique_ptr<ReaderModeDistillerPage> distiller_page_;
};

// Tests that an invalid URL fails immediately with `kNoData` without executing
// JavaScript.
TEST_F(ReaderModeDistillerPageTest, DistillPageInvalidUrl) {
  GURL invalid_url("invalid_url");
  EXPECT_EQ(web_distiller::DistillationParseResult::kNoData,
            DistillPage(invalid_url));
  EXPECT_EQ(0u, web_frame()->GetJavaScriptCallHistory().size());
}

// Tests that a missing main frame fails immediately with `kNoData` instead of
// hanging until the distillation timeout.
TEST_F(ReaderModeDistillerPageTest, DistillPageMissingMainFrame) {
  RemoveMainWebFrame();
  EXPECT_EQ(web_distiller::DistillationParseResult::kNoData,
            DistillPage(valid_url()));
}

// Tests that a JavaScript execution error (null result + `NSError`) invokes the
// completion callback with `kNoData` instead of dropping the callback.
TEST_F(ReaderModeDistillerPageTest, DistillPageInvalidJavaScriptResult) {
  web_frame()->AddResultForExecutedJs(nullptr, distiller_script_utf16());

  EXPECT_EQ(web_distiller::DistillationParseResult::kNoData,
            DistillPage(valid_url()));
  EXPECT_EQ(1u, web_frame()->GetJavaScriptCallHistory().size());
  EXPECT_EQ(distiller_script_utf16(), web_frame()->GetLastJavaScriptCall());
}

// Tests that a valid dictionary result from JavaScript succeeds.
TEST_F(ReaderModeDistillerPageTest, DistillPageValid) {
  base::DictValue dict_value;
  dict_value.Set(web_distiller::kDistilledTitleKey, "Article Title");
  dict_value.Set(web_distiller::kDistilledContentKey, "<p>Article body</p>");
  base::Value valid_value(std::move(dict_value));
  web_frame()->AddResultForExecutedJs(&valid_value, distiller_script_utf16());

  EXPECT_EQ(web_distiller::DistillationParseResult::kSuccess,
            DistillPage(valid_url()));
  EXPECT_EQ(1u, web_frame()->GetJavaScriptCallHistory().size());
  EXPECT_EQ(distiller_script_utf16(), web_frame()->GetLastJavaScriptCall());
}

// Tests that a main frame with a different origin than the target URL fails
// immediately with `kNoData` without executing JavaScript.
TEST_F(ReaderModeDistillerPageTest, DifferentOriginIgnored) {
  base::Value empty_value;
  web_frame()->AddResultForExecutedJs(&empty_value, distiller_script_utf16());

  GURL different_origin("https://notthesame.url");
  EXPECT_EQ(web_distiller::DistillationParseResult::kNoData,
            DistillPage(different_origin));
  EXPECT_EQ(0u, web_frame()->GetJavaScriptCallHistory().size());
}

// Tests that `DistillPage` fails immediately with `kNoData` when the observed
// `WebState` has already been destroyed.
TEST_F(ReaderModeDistillerPageTest, DistillPageDestroyedWebState) {
  auto transient_web_state = std::make_unique<web::FakeWebState>();
  ReaderModeDistillerPage distiller_page(transient_web_state.get());
  transient_web_state.reset();

  EXPECT_EQ(web_distiller::DistillationParseResult::kNoData,
            RunDistillation(distiller_page, valid_url()).parse_result);
}

class ReaderModeDistillerPageWebStateTest : public IOSChromeTestWithWebState {
 public:
  ReaderModeDistillerPageWebStateTest()
      : IOSChromeTestWithWebState(WebClientMode::kChromeWebClient) {}

  // Runs `ReaderModeDistillerPage::DistillPage` against `web_state()` for `url`
  // and returns the distillation outcome.
  DistillationOutcome DistillPage(const GURL& url) {
    ReaderModeDistillerPage distiller_page(web_state());
    return RunDistillation(distiller_page, url);
  }
};

// Tests that `ReaderModeDistillerPage` distills HTML5 documents (`<!DOCTYPE
// html>`) containing both declarative and imperative open shadow roots in
// `WKWebView` without throwing `NotSupportedError` on `DocumentType` nodes.
TEST_F(ReaderModeDistillerPageWebStateTest, DistillsPagesWithOpenShadowRoots) {
  const GURL article_url("https://example.com/article");
  NSString* html =
      @"<!-- Leading comment -->"
       "<!DOCTYPE html>"
       "<html><head><title>Shadow DOM Article</title></head><body>"
       "<site-header></site-header>"
       "<custom-article>"
       "  <template shadowrootmode='open'>"
       "    <article>"
       "      <slot name='lead'></slot>"
       "      <p>The city botanical garden was established in the late "
       "      nineteenth century as a public sanctuary for plant conservation, "
       "      scientific research, and horticulture education. Over the "
       "      decades, the grounds have expanded to include several distinct "
       "      greenhouse pavilions and walking trails.</p>"
       "      <slot name='unassigned-fallback'>"
       "        <p>Seasonal exhibitions and educational tours are offered "
       "        throughout the spring and autumn growing months.</p>"
       "      </slot>"
       "      <inner-widget>"
       "        <template shadowrootmode='open'>"
       "          <div class='widget-body'><slot></slot></div>"
       "        </template>"
       "        <p>The newly renovated alpine pavilion houses a wide variety "
       "        of high-altitude plants, mosses, and hardy perennials.</p>"
       "      </inner-widget>"
       "      <template><slot name='lead'></slot></template>"
       "      <slot name='body-extra'></slot>"
       "    </article>"
       "  </template>"
       "  <p slot='lead'>The central glass conservatory on an autumn day.</p>"
       "  <slot slot='body-extra'>"
       "    <p>Researchers, educators, and visiting students regularly consult "
       "    archives to study long-term shifts in regional plant distributions "
       "    and seasonal flowering schedules across temperate ecosystems.</p>"
       "  </slot>"
       "  <div class='unslotted'>"
       "    <p>Unslotted sidebar navigation links that must be excluded.</p>"
       "  </div>"
       "</custom-article>"
       "<script>"
       "class SiteHeader extends HTMLElement {"
       "  constructor() {"
       "    super();"
       "    const root = this.attachShadow({mode: 'open'});"
       "    root.innerHTML = '<nav>Navigation Banner</nav>';"
       "  }"
       "}"
       "customElements.define('site-header', SiteHeader);"
       "</script>"
       "</body></html>";

  web::test::LoadHtml(html, article_url, web_state());
  ASSERT_TRUE(WaitForMainFrame(ReaderModeJavaScriptFeature::GetInstance()));

  DistillationOutcome outcome = DistillPage(article_url);

  EXPECT_EQ(web_distiller::DistillationParseResult::kSuccess,
            outcome.parse_result);
  ASSERT_TRUE(outcome.page);
  const web_distiller::DistilledPageProto& distilled_page = *outcome.page;
  EXPECT_EQ("Shadow DOM Article", distilled_page.title());
  EXPECT_THAT(distilled_page.html(),
              HasSubstr("city botanical garden was established"));
  EXPECT_THAT(distilled_page.html(),
              HasSubstr("central glass conservatory on an autumn day"));
  EXPECT_THAT(distilled_page.html(),
              HasSubstr("Seasonal exhibitions and educational tours"));
  EXPECT_THAT(distilled_page.html(),
              HasSubstr("newly renovated alpine pavilion"));
  EXPECT_THAT(distilled_page.html(), HasSubstr("Researchers, educators"));
  EXPECT_THAT(distilled_page.html(),
              HasSubstr("<template><slot name=\"lead\"></slot></template>"));
  EXPECT_THAT(distilled_page.html(),
              Not(HasSubstr("Unslotted sidebar navigation links")));
}

// Tests that a standard light-DOM HTML5 article still distills cleanly when an
// unrelated custom element on the page attaches an open shadow root.
TEST_F(ReaderModeDistillerPageWebStateTest,
       DistillsLightDomArticleWithShadowHostWidget) {
  const GURL article_url("https://example.com/news-story");
  // `ConsentBanner` throws if its constructor runs twice, because flattening
  // must never re-run page-defined custom element constructors (which would
  // execute page script during distillation).
  NSString* html =
      @"<!DOCTYPE html>"
       "<html><head><title>Light DOM News Story</title></head><body>"
       "<consent-banner></consent-banner>"
       "<article>"
       "  <h1>Regional Transit Expansion Opens Ahead of Schedule</h1>"
       "  <p>Commuters across the metropolitan corridor gained access to three "
       "  new rail stations on Monday morning following years of tunneling, "
       "  signal upgrades, and station accessibility improvements.</p>"
       "  <p>Transit officials reported steady ridership throughout the "
       "  inaugural morning rush, noting that automated train control reduced "
       "  headways and improved platform transfers across connecting lines.</p>"
       "  <p>Local businesses near the newly opened terminals welcomed the "
       "  additional foot traffic while urban planners highlighted long-term "
       "  reductions in regional highway congestion and emissions.</p>"
       "</article>"
       "<script>"
       "let constructed = false;"
       "class ConsentBanner extends HTMLElement {"
       "  constructor() {"
       "    super();"
       "    if (constructed) {"
       "      throw new Error('Unexpected constructor re-entry');"
       "    }"
       "    constructed = true;"
       "    this.attachShadow({mode: 'open'}).innerHTML = '<div>Cookie</div>';"
       "  }"
       "}"
       "customElements.define('consent-banner', ConsentBanner);"
       "</script>"
       "</body></html>";

  web::test::LoadHtml(html, article_url, web_state());
  ASSERT_TRUE(WaitForMainFrame(ReaderModeJavaScriptFeature::GetInstance()));

  DistillationOutcome outcome = DistillPage(article_url);

  EXPECT_EQ(web_distiller::DistillationParseResult::kSuccess,
            outcome.parse_result);
  ASSERT_TRUE(outcome.page);
  const web_distiller::DistilledPageProto& distilled_page = *outcome.page;
  EXPECT_EQ("Light DOM News Story", distilled_page.title());
  EXPECT_THAT(distilled_page.html(),
              HasSubstr("Commuters across the metropolitan corridor"));
  EXPECT_THAT(distilled_page.html(),
              HasSubstr("Transit officials reported steady ridership"));
}
