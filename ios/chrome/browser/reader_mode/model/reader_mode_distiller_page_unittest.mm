// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_distiller_page.h"

#import <memory>

#import "base/strings/utf_string_conversions.h"
#import "components/dom_distiller/core/extraction_utils.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/dom_distiller_js/dom_distiller.pb.h"
#import "url/gurl.h"

class ReaderModeDistillerPageTest : public PlatformTest {
 public:
  ReaderModeDistillerPageTest() : valid_url_(GURL("https://test.url")) {
    distiller_page_ = std::make_unique<ReaderModeDistillerPage>(&web_state_);

    dom_distiller::proto::DomDistillerOptions options;
    dom_distiller_script_ =
        dom_distiller::GetDistillerScriptWithOptions(options);

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

  ReaderModeDistillerPage* distiller_page() { return distiller_page_.get(); }

  const std::string& dom_distiller_script() { return dom_distiller_script_; }

  const std::u16string dom_distiller_script_utf16() {
    return base::UTF8ToUTF16(dom_distiller_script_);
  }

  web::FakeWebFrame* web_frame() { return web_frame_; }

 private:
  web::WebTaskEnvironment task_environment_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_;
  raw_ptr<web::FakeWebFrame> web_frame_;

  GURL valid_url_;
  std::string dom_distiller_script_;
  std::unique_ptr<ReaderModeDistillerPage> distiller_page_;
};

TEST_F(ReaderModeDistillerPageTest, DistillPageInvalidUrl) {
  GURL invalid_url("invalid_url");
  distiller_page()->DistillPageImpl(invalid_url, dom_distiller_script());
  EXPECT_EQ(0u, web_frame()->GetJavaScriptCallHistory().size());
}

TEST_F(ReaderModeDistillerPageTest, DistillPageInvalidScript) {
  distiller_page()->DistillPageImpl(valid_url(), "");
  EXPECT_EQ(0u, web_frame()->GetJavaScriptCallHistory().size());
}

TEST_F(ReaderModeDistillerPageTest, DistillPageInvalidJavaScriptResult) {
  web_frame()->AddResultForExecutedJs(nullptr, dom_distiller_script_utf16());

  distiller_page()->DistillPageImpl(valid_url(), dom_distiller_script());
  EXPECT_EQ(1u, web_frame()->GetJavaScriptCallHistory().size());
  EXPECT_EQ(dom_distiller_script_utf16(), web_frame()->GetLastJavaScriptCall());
}

TEST_F(ReaderModeDistillerPageTest, DistillPageValid) {
  base::Value empty_value;
  web_frame()->AddResultForExecutedJs(&empty_value,
                                      dom_distiller_script_utf16());

  distiller_page()->DistillPageImpl(valid_url(), dom_distiller_script());
  EXPECT_EQ(1u, web_frame()->GetJavaScriptCallHistory().size());
  EXPECT_EQ(dom_distiller_script_utf16(), web_frame()->GetLastJavaScriptCall());
}
