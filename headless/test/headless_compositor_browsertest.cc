// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_protocol_browsertest.h"

#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/switches.h"

namespace headless {

class HeadlessCompositorBrowserTest : public HeadlessProtocolBrowserTest {
 public:
  HeadlessCompositorBrowserTest() = default;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessProtocolBrowserTest::SetUpCommandLine(command_line);
    // The following switches are recommended for BeginFrameControl required by
    // compositor tests. See https://goo.gle/chrome-headless-rendering for
    // details.
    static const char* const compositor_switches[] = {
        // We control BeginFrames ourselves and need all compositing stages to
        // run.
        ::switches::kRunAllCompositorStagesBeforeDraw,
        ::switches::kDisableNewContentRenderingTimeout,

        // Animation-only BeginFrames are only supported when updates from the
        // impl-thread are disabled. See
        // https://goo.gle/chrome-headless-rendering.
        switches::kDisableThreadedAnimation,
        switches::kDisableCheckerImaging,

        // Ensure that image animations don't resync their animation timestamps
        // when looping back around.
        blink::switches::kDisableImageAnimationResync,
    };

    for (auto* compositor_switch : compositor_switches) {
      command_line->AppendSwitch(compositor_switch);
    }
  }
};

// BeginFrameControl is not supported on MacOS yet, see: https://cs.chromium.org
// chromium/src/headless/lib/browser/protocol/target_handler.cc?
// rcl=5811aa08e60ba5ac7622f029163213cfbdb682f7&l=32
// TODO(crbug.com/40656275): Suite is flaky on TSan Linux.
#if BUILDFLAG(IS_MAC) || ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
                          defined(THREAD_SANITIZER))
#define HEADLESS_COMPOSITOR_TEST(TEST_NAME, SCRIPT_NAME)  \
  HEADLESS_PROTOCOL_TEST_F(HeadlessCompositorBrowserTest, \
                           DISABLED_##TEST_NAME, SCRIPT_NAME)
#else
#define HEADLESS_COMPOSITOR_TEST(TEST_NAME, SCRIPT_NAME)             \
  HEADLESS_PROTOCOL_TEST_F(HeadlessCompositorBrowserTest, TEST_NAME, \
                           SCRIPT_NAME)
#endif

HEADLESS_COMPOSITOR_TEST(VirtualTimeControllerTest,
                         "helpers/virtual-time-controller-test.js")

HEADLESS_COMPOSITOR_TEST(CompositorBasicRaf,
                         "virtual-time/compositor-basic-raf.js")
HEADLESS_COMPOSITOR_TEST(CompositorImageAnimation,
                         "virtual-time/compositor-image-animation-test.js")

// Flaky on all platforms. TODO(crbug.com/41471823): Re-enable.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA)
#define MAYBE_CompositorCssAnimation DISABLED_CompositorCssAnimation
#else
#define MAYBE_CompositorCssAnimation CompositorCssAnimation
#endif
HEADLESS_COMPOSITOR_TEST(MAYBE_CompositorCssAnimation,
                         "virtual-time/compositor-css-animation-test.js")
HEADLESS_COMPOSITOR_TEST(VirtualTimeCancelClientRedirect,
                         "virtual-time/virtual-time-cancel-client-redirect.js")
HEADLESS_COMPOSITOR_TEST(DoubleBeginFrame, "virtual-time/double-begin-frame.js")
HEADLESS_COMPOSITOR_TEST(RendererHelloWorld,
                         "virtual-time/renderer-hello-world.js")
HEADLESS_COMPOSITOR_TEST(RendererOverrideTitleJsEnabled,
                         "virtual-time/renderer-override-title-js-enabled.js")
HEADLESS_COMPOSITOR_TEST(RendererOverrideTitleJsDisabled,
                         "virtual-time/renderer-override-title-js-disabled.js")
HEADLESS_COMPOSITOR_TEST(RendererJavaScriptConsoleErrors,
                         "virtual-time/renderer-javascript-console-errors.js")
HEADLESS_COMPOSITOR_TEST(RendererDelayedCompletion,
                         "virtual-time/renderer-delayed-completion.js")
HEADLESS_COMPOSITOR_TEST(RendererClientRedirectChain,
                         "virtual-time/renderer-client-redirect-chain.js")
HEADLESS_COMPOSITOR_TEST(RendererClientRedirectChainNoJs,
                         "virtual-time/renderer-client-redirect-chain-no-js.js")
HEADLESS_COMPOSITOR_TEST(RendererServerRedirectChain,
                         "virtual-time/renderer-server-redirect-chain.js")
HEADLESS_COMPOSITOR_TEST(RendererServerRedirectToFailure,
                         "virtual-time/renderer-server-redirect-to-failure.js")
HEADLESS_COMPOSITOR_TEST(
    RendererServerRedirectRelativeChain,
    "virtual-time/renderer-server-redirect-relative-chain.js")
HEADLESS_COMPOSITOR_TEST(RendererMixedRedirectChain,
                         "virtual-time/renderer-mixed-redirect-chain.js")
HEADLESS_COMPOSITOR_TEST(RendererFramesRedirectChain,
                         "virtual-time/renderer-frames-redirect-chain.js")
HEADLESS_COMPOSITOR_TEST(RendererDoubleRedirect,
                         "virtual-time/renderer-double-redirect.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirectAfterCompletion,
                         "virtual-time/renderer-redirect-after-completion.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirect307PostMethod,
                         "virtual-time/renderer-redirect-307-post-method.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirectPostChain,
                         "virtual-time/renderer-redirect-post-chain.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirect307PutMethod,
                         "virtual-time/renderer-redirect-307-put-method.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirect303PutGet,
                         "virtual-time/renderer-redirect-303-put-get.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirectBaseUrl,
                         "virtual-time/renderer-redirect-base-url.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirectNonAsciiUrl,
                         "virtual-time/renderer-redirect-non-ascii-url.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirectEmptyUrl,
                         "virtual-time/renderer-redirect-empty-url.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirectInvalidUrl,
                         "virtual-time/renderer-redirect-invalid-url.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirectKeepsFragment,
                         "virtual-time/renderer-redirect-keeps-fragment.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirectReplacesFragment,
                         "virtual-time/renderer-redirect-replaces-fragment.js")
HEADLESS_COMPOSITOR_TEST(RendererRedirectNewFragment,
                         "virtual-time/renderer-redirect-new-fragment.js")
HEADLESS_COMPOSITOR_TEST(RendererWindowLocationFragments,
                         "virtual-time/renderer-window-location-fragments.js")
HEADLESS_COMPOSITOR_TEST(RendererCookieSetFromJs,
                         "virtual-time/renderer-cookie-set-from-js.js")
HEADLESS_COMPOSITOR_TEST(
    RendererCookieSetFromJsNoCookies,
    "virtual-time/renderer-cookie-set-from-js-no-cookies.js")
HEADLESS_COMPOSITOR_TEST(RendererCookieUpdatedFromJs,
                         "virtual-time/renderer-cookie-updated-from-js.js")
HEADLESS_COMPOSITOR_TEST(RendererInCrossOriginObject,
                         "virtual-time/renderer-in-cross-origin-object.js")

HEADLESS_COMPOSITOR_TEST(RendererContentSecurityPolicy,
                         "virtual-time/renderer-content-security-policy.js")

HEADLESS_COMPOSITOR_TEST(RendererFrameLoadEvents,
                         "virtual-time/renderer-frame-load-events.js")
HEADLESS_COMPOSITOR_TEST(RendererCssUrlFilter,
                         "virtual-time/renderer-css-url-filter.js")
HEADLESS_COMPOSITOR_TEST(RendererCanvas, "virtual-time/renderer-canvas.js")

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
// Flaky on at least Linux and Windows: crbug.com/1294751.
#define MAYBE_RendererOpacityAnimation DISABLED_RendererOpacityAnimation
#else
#define MAYBE_RendererOpacityAnimation RendererOpacityAnimation
#endif
HEADLESS_COMPOSITOR_TEST(MAYBE_RendererOpacityAnimation,
                         "virtual-time/renderer-opacity-animation.js")

HEADLESS_COMPOSITOR_TEST(ScreenshotWebp, "virtual-time/screenshot-webp.js")
HEADLESS_COMPOSITOR_TEST(ScreenshotOptimizeForSpeed,
                         "virtual-time/screenshot-optimize-for-speed.js")
HEADLESS_COMPOSITOR_TEST(ScreenshotAfterMetricsOverride,
                         "virtual-time/screenshot-after-metrics-override.js")
HEADLESS_COMPOSITOR_TEST(ScreenshotDeviceScaleFactor,
                         "virtual-time/screenshot-device-scale-factor.js")
HEADLESS_COMPOSITOR_TEST(VirtualTimeIntersectionObserverWithViewport,
                         "virtual-time/intersection-observer-with-viewport.js")
HEADLESS_COMPOSITOR_TEST(VeryLargeViewportCrash,
                         "virtual-time/very-large-viewport-crash.js")

}  // namespace headless
