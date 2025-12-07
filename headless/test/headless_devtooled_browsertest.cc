// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_devtooled_browsertest.h"

#include <memory>

#include "base/check_deref.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/headless_browser_context.h"
#include "headless/test/headless_browser_test_utils.h"

namespace headless {

HeadlessDevTooledBrowserTest::HeadlessDevTooledBrowserTest() = default;
HeadlessDevTooledBrowserTest::~HeadlessDevTooledBrowserTest() = default;

void HeadlessDevTooledBrowserTest::RunTest() {
  HeadlessBrowserContext::Builder builder =
      browser()->CreateBrowserContextBuilder();
  CustomizeHeadlessBrowserContext(builder);
  browser_context_ = builder.Build();
  browser()->SetDefaultBrowserContext(browser_context_);

  browser_devtools_client_.AttachToBrowser();

  HeadlessWebContents::Builder web_contents_builder =
      browser_context_->CreateWebContentsBuilder();
  web_contents_builder.SetEnableBeginFrameControl(GetEnableBeginFrameControl());
  CustomizeHeadlessWebContents(web_contents_builder);
  web_contents_ = web_contents_builder.Build();
  Observe(HeadlessWebContentsImpl::From(web_contents_)->web_contents());

  PreRunAsynchronousTest();
  RunAsynchronousTest();
  PostRunAsynchronousTest();

  devtools_client_.DetachClient();
  Observe(nullptr);
  {
    // Keep raw_ptr<> happy and clear it before WC dies.
    HeadlessWebContents& wc = *web_contents_;
    web_contents_ = nullptr;
    wc.Close();
  }
  {
    HeadlessBrowserContext& bc = *browser_context_;
    browser_context_ = nullptr;
    browser_devtools_client_.DetachClient();
    bc.Close();
  }

  // Let the tasks that might have beein scheduled during web contents
  // being closed run (see https://crbug.com/1036627 for details).
  base::RunLoop().RunUntilIdle();
}

void HeadlessDevTooledBrowserTest::RenderViewReady() {
  if (had_render_view_ready_) {
    return;
  }
  had_render_view_ready_ = true;

  CHECK(HeadlessWebContentsImpl::From(web_contents_)
            ->web_contents()
            ->GetPrimaryMainFrame()
            ->IsRenderFrameLive());

  DevToolsTargetReady();

  devtools_client_.AttachToWebContents(
      HeadlessWebContentsImpl::From(web_contents_)->web_contents());

  RunDevTooledTest();
}

void HeadlessDevTooledBrowserTest::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION)
    return;

  FinishAsynchronousTest();
  FAIL() << "Abnormal renderer termination (status=" << status << ")";
}

bool HeadlessDevTooledBrowserTest::GetEnableBeginFrameControl() {
  return false;
}

void HeadlessDevTooledBrowserTest::CustomizeHeadlessBrowserContext(
    HeadlessBrowserContext::Builder& builder) {}

void HeadlessDevTooledBrowserTest::CustomizeHeadlessWebContents(
    HeadlessWebContents::Builder& builder) {}

// DevTooled browser tests ---------------------------------------------------

// This test was implicitly disabled on Fuchsia while being part of headless
// protocol tests before it was moved here.
// TODO(crbug.com/40222911): Enable on Fuchsia when no longer flakily timeout.
#if !BUILDFLAG(IS_FUCHSIA)

class HeadlessAllowedVideoCodecsTest
    : public HeadlessDevTooledBrowserTest,
      public testing::WithParamInterface<
          std::tuple<std::string, std::string, bool>> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessDevTooledBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("allow-video-codecs", allowlist());
  }

  void RunDevTooledTest() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    SendCommandSync(devtools_client_, "Page.enable");
    devtools_client_.AddEventHandler(
        "Page.loadEventFired",
        base::BindRepeating(&HeadlessAllowedVideoCodecsTest::OnLoadEventFired,
                            base::Unretained(this)));
    devtools_client_.SendCommand(
        "Page.navigate",
        Param("url", embedded_test_server()->GetURL("/hello.html").spec()));
  }

  void OnLoadEventFired(const base::Value::Dict& params) {
    base::Value::Dict eval_params;
    eval_params.Set("returnByValue", true);
    eval_params.Set("awaitPromise", true);
    eval_params.Set("expression", base::StringPrintf(R"(
      VideoDecoder.isConfigSupported({codec: "%s"})
          .then(result => result.supported)
    )",
                                                     codec_name().c_str()));
    base::Value::Dict result = SendCommandSync(
        devtools_client_, "Runtime.evaluate", std::move(eval_params));
    EXPECT_THAT(result.FindBoolByDottedPath("result.result.value"),
                testing::Optional(is_codec_enabled()));
    FinishAsynchronousTest();
  }

  const std::string& allowlist() const { return std::get<0>(GetParam()); }
  const std::string& codec_name() const { return std::get<1>(GetParam()); }
  bool is_codec_enabled() const { return std::get<2>(GetParam()); }
};

constexpr bool have_proprietary_codecs =
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    true;
#else
    false;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HeadlessAllowedVideoCodecsTest,
    testing::Values(
        std::make_tuple("av1,-*", "av01.0.04M.08", true),
        std::make_tuple("-av1,*", "av01.0.04M.08", false),
        std::make_tuple("*", "avc1.64000b", have_proprietary_codecs)));

HEADLESS_DEVTOOLED_TEST_P(HeadlessAllowedVideoCodecsTest);

#endif  // #if !BUILDFLAG(IS_FUCHSIA)

}  // namespace headless
