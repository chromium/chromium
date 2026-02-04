// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/animation_frame_timing_monitor.h"

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class AnimationFrameTimingMonitorTest : public SimTest {
 protected:
  void TestAttribution(const String& url, const bool expected_presence) {
    constexpr char kHTMLString[] = R"HTML(
        <div id='update'></div>
        <script>
          let result = '';

          new PerformanceObserver((list) => {
            const entries = list.getEntries();
            const has_attribution =
                entries.length > 0 &&
                entries.every(({ scripts }) =>
                  scripts.length > 0 &&
                  scripts.every(({ sourceURL }) => sourceURL === location.href)
                );
            result = has_attribution ? 'YES' : 'NO';
          }).observe({entryTypes: ['long-animation-frame']});

          // Produce a long animation frame
          requestAnimationFrame(() => {
            const before = performance.now();
            document.querySelector('#update').innerHTML = 'Update';
            while (performance.now() < (before + 60)) {}
            requestAnimationFrame(() => {});
          });
        </script>
      )HTML";

    constexpr char kPollScript[] = R"JS(
        result;
      )JS";

    SimRequest main_resource(url, "text/html");
    LoadURL(url);
    main_resource.Complete(kHTMLString);

    // Produce two frames
    for (int i = 0; i < 2; i++) {
      Compositor().BeginFrame();
      test::RunPendingTasks();
    }

    WebScriptSource source = WebScriptSource(WebString::FromUTF8(kPollScript));
    v8::Isolate* isolate = Window().GetIsolate();

    auto get_result = [&]() -> String {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Value> result =
          MainFrame().ExecuteScriptAndReturnValue(source);
      return ToCoreString(isolate, result.As<v8::String>());
    };

    // The long-animation-frame event is emitted asynchronously
    EXPECT_TRUE(base::test::RunUntil([&]() { return get_result() != ""; }));

    EXPECT_EQ(get_result(), expected_presence ? "YES" : "NO");
  }

  void TestAttributionWithFeatureFlag(const String& url,
                                      const bool expected_presence) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(kAlwaysLogLOAFURL);

    TestAttribution(url, expected_presence);
  }
};

TEST_F(AnimationFrameTimingMonitorTest,
       HttpHasScriptAttributionWithoutFeatureFlag) {
  TestAttribution("http://example.com", true);
}

TEST_F(AnimationFrameTimingMonitorTest,
       HttpHasScriptAttributionWithFeatureFlag) {
  TestAttributionWithFeatureFlag("http://example.com", true);
}

TEST_F(AnimationFrameTimingMonitorTest,
       HttpsHasScriptAttributionWithoutFeatureFlag) {
  TestAttribution("https://example.com", true);
}

TEST_F(AnimationFrameTimingMonitorTest,
       HttpsHasScriptAttributionWithFeatureFlag) {
  TestAttributionWithFeatureFlag("https://example.com", true);
}

TEST_F(AnimationFrameTimingMonitorTest,
       FileDoesNotHaveScriptAttributionWithoutFeatureFlag) {
  TestAttribution("file:///path/to/file.html", false);
}

TEST_F(AnimationFrameTimingMonitorTest,
       FileHasScriptAttributionWithFeatureFlag) {
  TestAttributionWithFeatureFlag("file:///path/to/file.html", true);
}

TEST_F(AnimationFrameTimingMonitorTest,
       CustomProtocolDoesNotHaveScriptAttributionWithoutFeatureFlag) {
  TestAttribution("custom://example.com", false);
}

TEST_F(AnimationFrameTimingMonitorTest,
       CustomProtocolHasScriptAttributionWithFeatureFlag) {
  TestAttributionWithFeatureFlag("custom://example.com", true);
}

}  // namespace blink
