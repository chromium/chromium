// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/animation_frame_timing_monitor.h"

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/lazy_source_location.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

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

    WebScriptSource source = WebScriptSource(WebString::FromUtf8(kPollScript));
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

TEST_F(AnimationFrameTimingMonitorTest,
       DetachChildLocalRootDuringFrameRelatedTask) {
  InitializeRemote();
  LocalFrame* frame = LocalFrameRoot().GetFrame();
  CoreProbeSink* probe_sink = frame->GetProbeSink();
  ASSERT_TRUE(probe_sink->HasAnimationFrameTimingMonitors());

  {
    probe::FrameRelatedTask probe(frame->DomWindow());
    constexpr char kLongScript[] = R"JS(
      const start = performance.now();
      while (performance.now() - start < 60) {}
    )JS";
    LocalFrameRoot().ExecuteScript(
        WebScriptSource(WebString::FromUtf8(kLongScript)));

    LocalFrameRoot().Detach();
    EXPECT_FALSE(probe_sink->HasAnimationFrameTimingMonitors());
  }
}

// TODO(crbug.com/587574574): Re-enable once flaky LoAF script leak is fixed.
TEST_F(AnimationFrameTimingMonitorTest,
       DISABLED_CrossOriginLivePromiseSettlementNotExposed) {
  SimRequest::Params params;
  params.response_http_headers.insert("Origin-Agent-Cluster", "?0");

  SimRequest main_resource("https://a.example.com/main.html", "text/html",
                           params);
  SimRequest child_resource("https://b.example.com/child.html", "text/html",
                            params);

  LoadURL("https://a.example.com/main.html");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <script>
      window.loafObserved = false;
      window.leakedScripts = '';
      new PerformanceObserver((list) => {
        for (const entry of list.getEntries()) {
          window.loafObserved = true;
          for (const s of entry.scripts) {
            window.leakedScripts += s.sourceURL + '|' + s.invoker + ';';
          }
        }
      }).observe({type: 'long-animation-frame'});

      function tick() {
        requestAnimationFrame(tick);
      }
      requestAnimationFrame(tick);
    </script>
    <iframe src="https://b.example.com/child.html"></iframe>
  )HTML");
  child_resource.Complete("<!doctype html><p>cross-origin child</p>");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  LocalFrame* main_frame = MainFrame().GetFrame();
  auto* child_frame = DynamicTo<LocalFrame>(main_frame->Tree().FirstChild());
  ASSERT_TRUE(child_frame);
  ASSERT_EQ(main_frame->GetProbeSink(), child_frame->GetProbeSink());

  // Settle a promise belonging to the cross-origin child inside a long task
  // attributed to the main frame.
  {
    probe::FrameRelatedTask probe(main_frame->DomWindow());
    probe::WillHandlePromise(child_frame->DomWindow(),
                             ToScriptStateForMainWorld(child_frame),
                             /*resolving=*/false, "Window", "fetch",
                             MakeGarbageCollected<LazySourceLocation>(
                                 "https://b.example.com/victim.js"));
    const base::TimeTicks start = base::TimeTicks::Now();
    while (base::TimeTicks::Now() - start < base::Milliseconds(60)) {
    }
  }

  for (int i = 0; i < 2; i++) {
    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  v8::Isolate* isolate = Window().GetIsolate();
  auto eval_string = [&](const char* script) -> String {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Value> result = MainFrame().ExecuteScriptAndReturnValue(
        WebScriptSource(WebString::FromUtf8(script)));
    return ToCoreString(isolate, result.As<v8::String>());
  };

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return eval_string("String(window.loafObserved)") == "true"; }));
  EXPECT_EQ(eval_string("window.leakedScripts"), "");
}

TEST_F(AnimationFrameTimingMonitorTest,
       CrossOriginPromiseRejectOnParserDetachNotExposed) {
  SimRequest::Params params;
  params.response_http_headers.insert("Origin-Agent-Cluster", "?0");

  SimRequest main_resource("https://a.example.com/main.html", "text/html",
                           params);
  SimRequest child_resource("https://b.example.com/victim.html", "text/html",
                            params);
  KURL pending_fetch_url("https://b.example.com/longpoll");
  url_test_helpers::RegisterMockedURLLoad(pending_fetch_url, "");

  LoadURL("https://a.example.com/main.html");
  main_resource.Write(R"HTML(
    <!doctype html>
    <body>
    <script>
      window.detachLoafObserved = false;
      window.detachReferenceTime = -1;
      window.leakedScripts = '';
      new PerformanceObserver((list) => {
        for (const entry of list.getEntries()) {
          if (window.detachReferenceTime >= 0 &&
              entry.startTime <= window.detachReferenceTime &&
              window.detachReferenceTime <= entry.startTime + entry.duration) {
            window.detachLoafObserved = true;
            for (const s of entry.scripts) {
              if (s.sourceURL.includes('b.example.com')) {
                window.leakedScripts += s.sourceURL + '|' + s.invoker + ';';
              }
            }
          }
        }
      }).observe({type: 'long-animation-frame'});

      function tick() {
        requestAnimationFrame(tick);
      }
      requestAnimationFrame(tick);
    </script>
    <b><p><iframe src="https://b.example.com/victim.html"></iframe>
  )HTML");

  child_resource.Complete(R"HTML(
    <!doctype html>
    <script>
      fetch('https://b.example.com/longpoll');
    </script>
  )HTML");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Stream chunk 2 inside a FrameRelatedTask attributed to the main frame so
  // that the adoption-agency reparent detaches the cross-origin iframe at
  // script depth 0 and OnMainThreadTaskCompleted runs for the main frame.
  {
    probe::FrameRelatedTask probe(MainFrame().GetFrame()->DomWindow());
    main_resource.Write(R"HTML(</b><script>
      const t0 = performance.now();
      while (performance.now() - t0 < 30) {}
      window.detachReferenceTime = performance.now();
      while (performance.now() - t0 < 65) {}
    </script>)HTML");
    main_resource.Finish();
  }
  test::RunPendingTasks();

  for (int i = 0; i < 2; i++) {
    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  v8::Isolate* isolate = Window().GetIsolate();
  auto eval_string = [&](const char* script) -> String {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Value> result = MainFrame().ExecuteScriptAndReturnValue(
        WebScriptSource(WebString::FromUtf8(script)));
    return ToCoreString(isolate, result.As<v8::String>());
  };

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return eval_string("String(window.detachLoafObserved)") == "true";
  }));
  EXPECT_EQ(eval_string("window.leakedScripts"), "");

  url_test_helpers::RegisterMockedURLUnregister(pending_fetch_url);
}

TEST_F(AnimationFrameTimingMonitorTest,
       SameOriginChildPromiseAttributedToChildWindow) {
  SimRequest main_resource("https://a.example.com/main.html", "text/html");
  SimRequest child_resource("https://a.example.com/child.html", "text/html");

  LoadURL("https://a.example.com/main.html");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <iframe id="child" src="https://a.example.com/child.html"></iframe>
    <script>
      window.promiseAttribution = '';
      new PerformanceObserver((list) => {
        for (const entry of list.getEntries()) {
          for (const s of entry.scripts) {
            if (s.invokerType === 'resolve-promise') {
              const isChild =
                  s.window === document.getElementById('child').contentWindow;
              window.promiseAttribution =
                  s.windowAttribution + '|' + isChild + '|' + s.sourceURL;
            }
          }
        }
      }).observe({type: 'long-animation-frame'});

      function tick() {
        requestAnimationFrame(tick);
      }
      requestAnimationFrame(tick);
    </script>
  )HTML");
  child_resource.Complete("<!doctype html><p>same-origin child</p>");

  Compositor().BeginFrame();
  test::RunPendingTasks();

  LocalFrame* main_frame = MainFrame().GetFrame();
  auto* child_frame = DynamicTo<LocalFrame>(main_frame->Tree().FirstChild());
  ASSERT_TRUE(child_frame);

  {
    probe::FrameRelatedTask probe(main_frame->DomWindow());
    probe::WillHandlePromise(child_frame->DomWindow(),
                             ToScriptStateForMainWorld(child_frame),
                             /*resolving=*/true, "Window", "fetch",
                             MakeGarbageCollected<LazySourceLocation>(
                                 "https://a.example.com/child.js"));
    const base::TimeTicks start = base::TimeTicks::Now();
    while (base::TimeTicks::Now() - start < base::Milliseconds(60)) {
    }
  }

  for (int i = 0; i < 2; i++) {
    Compositor().BeginFrame();
    test::RunPendingTasks();
  }

  v8::Isolate* isolate = Window().GetIsolate();
  auto eval_string = [&](const char* script) -> String {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Value> result = MainFrame().ExecuteScriptAndReturnValue(
        WebScriptSource(WebString::FromUtf8(script)));
    return ToCoreString(isolate, result.As<v8::String>());
  };

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return eval_string("window.promiseAttribution") != ""; }));
  EXPECT_EQ(eval_string("window.promiseAttribution"),
            "descendant|true|https://a.example.com/child.js");
}

}  // namespace blink
