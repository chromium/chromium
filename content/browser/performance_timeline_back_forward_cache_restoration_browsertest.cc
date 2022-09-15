// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"

namespace content {
class PerformanceTimelineBackForwardCacheRestorationBrowserTest
    : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkTestFeatures, "NavigationId");
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  EvalJsResult GetBackForwardCacheRestorationEntriesByObserver() const {
    std::string script = R"(
      (
        async ()=>Promise.all([entryTypesPromise, typePromise])
      )();
    )";
    return EvalJs(shell(), script);
  }

  EvalJsResult GetDroppedEntriesCount() const {
    std::string script = R"(
      (
        async ()=> {
          let promise =  new Promise(resolve=>{
                new PerformanceObserver((list, observer, options) => {
                  resolve(options['droppedEntriesCount']);
                }).observe({ type: 'back-forward-cache-restoration',
                buffered: true });
              });
          return await promise;
        }
      )();
    )";
    return EvalJs(shell(), script);
  }

  EvalJsResult SetBackForwardCacheRestorationBufferSize(int size) const {
    std::string script = R"(
        internals.setBackForwardCacheRestorationBufferSize($1);
    )";
    script = content::JsReplace(script, size);
    return EvalJs(shell(), script);
  }

  EvalJsResult RegisterPerformanceObservers(int max_size) const {
    std::string script = R"(
            let entryTypesEntries = [];
            var entryTypesPromise =  new Promise(resolve=>{
              new PerformanceObserver((list) => {
                const entries = list.getEntries().filter(
                  e => e.entryType == 'back-forward-cache-restoration').map(
                    e=>e.toJSON());;
                if (entries.length > 0) {
                  entryTypesEntries = entryTypesEntries.concat(entries);
                }
                if(entryTypesEntries.length>=$1){
                  resolve(entryTypesEntries);
                }
              }).observe({ entryTypes: ['back-forward-cache-restoration'] });
            });

            let typeEntries = [];
            var typePromise =  new Promise(resolve=>{
              new PerformanceObserver((list) => {
                const entries = list.getEntries().filter(
                  e => e.entryType == 'back-forward-cache-restoration').map(
                    e=>e.toJSON());
                if (entries.length > 0) {
                  typeEntries = typeEntries.concat(entries);
                }
                if(typeEntries.length>=$1){
                  resolve(typeEntries);
                }
              }).observe({type: 'back-forward-cache-restoration'});
            });
    )";
    script = content::JsReplace(script, max_size);
    return EvalJs(shell(), script);
  }

  void CheckEntry(const base::Value::List lst, int num_of_loops) const {
    for (int i = 0; i < num_of_loops; i++) {
      auto* dict = lst[i].GetIfDict();
      EXPECT_TRUE(dict);
      EXPECT_EQ("", *dict->FindString("name"));
      EXPECT_EQ("back-forward-cache-restoration",
                *dict->FindString("entryType"));
      int expected_navigation_id =
          i + 2;  // Navigation id starts from 1. It get incremented before a
                  // BackForwardCacheRestoration instance is created.
      EXPECT_EQ(expected_navigation_id, dict->FindInt("navigationId").value());
      EXPECT_LE(dict->FindDouble("pageshowEventStart").value(),
                dict->FindDouble("pageshowEventEnd").value());
    }
  }
};

IN_PROC_BROWSER_TEST_F(
    PerformanceTimelineBackForwardCacheRestorationBrowserTest,
    Create) {
  if (!IsBackForwardCacheEnabled())
    return;
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  const GURL url2(embedded_test_server()->GetURL("b.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url1));

  RenderFrameHostImplWrapper rfh(current_frame_host());

  int buffer_size = 10;
  int num_of_loops = 12;

  SetBackForwardCacheRestorationBufferSize(buffer_size);
  RegisterPerformanceObservers(num_of_loops);

  for (int i = 0; i < num_of_loops; i++) {
    // Navigate away
    ASSERT_TRUE(NavigateToURL(shell(), url2));

    // Verify `rfh` is stored in back/forward cache.
    ASSERT_TRUE(rfh->IsInBackForwardCache());

    // Navigate back.
    ASSERT_TRUE(HistoryGoBack(web_contents()));
  }
  auto result = std::move(GetBackForwardCacheRestorationEntriesByObserver()
                              .ExtractList()
                              .GetList());
  CheckEntry(std::move(result[0].GetList()), num_of_loops);
  CheckEntry(std::move(result[1].GetList()), num_of_loops);

  // Size of back forward restoration buffer is smaller than the number of back
  // forward restoration instances expected by 2. Therefore the
  // droppedEntriesCount is expected to be 2.
  EXPECT_EQ(2, GetDroppedEntriesCount().ExtractInt());
}
}  // namespace content
