// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/classic_pending_script.h"

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/script/pending_script.h"
#include "third_party/blink/renderer/core/script/script_runner.h"
#include "third_party/blink/renderer/core/script/script_scheduling_type.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "base/time/time.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/platform/loader/fetch/ad_tagging_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace blink {
namespace {

class ClassicPendingScriptTest : public SimTest {
 public:
  ClassicPendingScriptTest()
      : SimTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    SimTest::SetUp();
#if !BUILDFLAG(IS_CHROMEOS)
    if (!ClassicPendingScript::
            IsDeviceEligibleForLowPriorityAsyncScriptExecutionForTesting()) {
      GTEST_SKIP()
          << "Low-priority async script execution requires a non-low-end "
             "device with at least 3 GiB of physical memory.";
    }
#endif
  }

 protected:
  void ExpectEligible(Document& document, const char* id) {
    ExpectEligibility(document, id, true);
  }

  void ExpectIneligible(Document& document, const char* id) {
    ExpectEligibility(document, id, false);
  }

  // Returns the pending async script for `id`, and verifies that it is a
  // classic async script.
  PendingScript* GetPendingClassicAsyncScript(Document& document,
                                              const char* id) {
    auto* element =
        DynamicTo<HTMLScriptElement>(document.getElementById(AtomicString(id)));
    if (!element || !document.GetScriptRunner()) {
      return nullptr;
    }
    PendingScript* pending_script =
        document.GetScriptRunner()->FindPendingAsyncScriptForTesting(element);
    if (pending_script) {
      EXPECT_EQ(pending_script->GetScriptType(),
                mojom::blink::ScriptType::kClassic);
      EXPECT_EQ(pending_script->GetSchedulingType(),
                ScriptSchedulingType::kAsync);
    }
    return pending_script;
  }

 private:
  void ExpectEligibility(Document& document, const char* id, bool expected) {
    SCOPED_TRACE(id);
    PendingScript* pending_script = GetPendingClassicAsyncScript(document, id);
    if (!pending_script) {
      ADD_FAILURE() << "Pending classic async script not found.";
      return;
    }
    EXPECT_EQ(pending_script->IsEligibleForLowPriorityAsyncScriptExecution(),
              expected);
  }
};

#if BUILDFLAG(IS_CHROMEOS)

TEST_F(ClassicPendingScriptTest, DisabledOnChromeOS) {
  SimRequest main_resource("https://example.test/test.html", "text/html");
  SimSubresourceRequest script_resource("https://example.com/script.js",
                                        "application/javascript");

  LoadURL("https://example.test/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script id="target" async src="https://example.com/script.js"></script>
  )HTML");

  ExpectIneligible(GetDocument(), "target");

  script_resource.Complete("");
}

#else

TEST_F(ClassicPendingScriptTest, CrossSiteMainFrameScriptIsEligible) {
  SimRequest main_resource("https://example.test/test.html", "text/html");
  SimSubresourceRequest script_resource("https://example.com/script.js",
                                        "application/javascript");

  LoadURL("https://example.test/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script id="target" async src="https://example.com/script.js"></script>
  )HTML");

  ExpectEligible(GetDocument(), "target");

  script_resource.Complete("");
}

TEST_F(ClassicPendingScriptTest, SameSiteScriptIsNotEligible) {
  SimRequest main_resource("https://example.test/test.html", "text/html");
  SimSubresourceRequest same_origin_script(
      "https://example.test/same_origin.js", "application/javascript");
  SimSubresourceRequest same_site_script(
      "https://sub.example.test/same_site.js", "application/javascript");

  LoadURL("https://example.test/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script id="same_origin" async
            src="https://example.test/same_origin.js"></script>
    <script id="same_site" async
            src="https://sub.example.test/same_site.js"></script>
  )HTML");

  ExpectIneligible(GetDocument(), "same_origin");
  ExpectIneligible(GetDocument(), "same_site");

  same_origin_script.Complete("");
  same_site_script.Complete("");
}

TEST_F(ClassicPendingScriptTest, SubframeScriptIsNotEligible) {
  SimRequest main_resource("https://example.test/test.html", "text/html");
  SimRequest subframe_resource("https://example.test/subframe.html",
                               "text/html");
  SimSubresourceRequest script_resource("https://example.com/script.js",
                                        "application/javascript");

  LoadURL("https://example.test/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe id="subframe" src="https://example.test/subframe.html"></iframe>
  )HTML");
  subframe_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script id="target" async src="https://example.com/script.js"></script>
  )HTML");

  auto* iframe = DynamicTo<HTMLIFrameElement>(GetElementById("subframe"));
  ASSERT_TRUE(iframe);
  Document* subframe_document = iframe->contentDocument();
  ASSERT_TRUE(subframe_document);
  ASSERT_FALSE(subframe_document->IsInOutermostMainFrame());

  ExpectIneligible(*subframe_document, "target");

  script_resource.Complete("");
}

TEST_F(ClassicPendingScriptTest, DocumentOlderThanMaxAgeIsNotEligible) {
  SimRequest main_resource("https://example.test/test.html", "text/html");
  SimSubresourceRequest early_script("https://example.com/early.js",
                                     "application/javascript");
  SimSubresourceRequest late_script("https://example.com/late.js",
                                    "application/javascript");

  LoadURL("https://example.test/test.html");
  main_resource.Write(R"HTML(
    <!DOCTYPE html>
    <script id="early" async src="https://example.com/early.js"></script>
  )HTML");

  PendingScript* early_pending =
      GetPendingClassicAsyncScript(GetDocument(), "early");
  ASSERT_TRUE(early_pending);

  // At exactly 3 seconds, the document has not exceeded the max age limit.
  task_environment().FastForwardBy(base::Seconds(3));
  EXPECT_TRUE(early_pending->IsEligibleForLowPriorityAsyncScriptExecution());

  task_environment().FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(early_pending->IsEligibleForLowPriorityAsyncScriptExecution());

  main_resource.Write(R"HTML(
    <script id="late" async src="https://example.com/late.js"></script>
  )HTML");
  main_resource.Finish();

  ExpectIneligible(GetDocument(), "late");

  early_script.Complete("");
  late_script.Complete("");
}

TEST_F(ClassicPendingScriptTest, ReloadScriptIsNotEligible) {
  SimRequest initial_resource("https://example.test/test.html", "text/html");
  LoadURL("https://example.test/test.html");
  initial_resource.Complete("<!DOCTYPE html>");

  SimRequest reload_resource("https://example.test/test.html", "text/html");
  SimSubresourceRequest script_resource("https://example.com/script.js",
                                        "application/javascript");
  MainFrame().StartReload(WebFrameLoadType::kReload);
  reload_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script id="target" async src="https://example.com/script.js"></script>
  )HTML");

  ASSERT_TRUE(GetDocument().Loader());
  ASSERT_TRUE(GetDocument().Loader()->IsReloadedOrFormSubmitted());

  ExpectIneligible(GetDocument(), "target");

  script_resource.Complete("");
}

TEST_F(ClassicPendingScriptTest, PotentiallyRenderBlockingScriptIsNotEligible) {
  SimRequest main_resource("https://example.test/test.html", "text/html");
  SimSubresourceRequest script_resource("https://example.com/script.js",
                                        "application/javascript");

  LoadURL("https://example.test/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <head>
      <script id="target" async blocking="render"
              src="https://example.com/script.js"></script>
    </head>
  )HTML");

  PendingScript* pending_script =
      GetPendingClassicAsyncScript(GetDocument(), "target");
  ASSERT_TRUE(pending_script);
  ASSERT_TRUE(pending_script->GetElement());
  ASSERT_TRUE(pending_script->GetElement()->IsPotentiallyRenderBlocking());
  EXPECT_FALSE(pending_script->IsEligibleForLowPriorityAsyncScriptExecution());

  script_resource.Complete("");
}

TEST_F(ClassicPendingScriptTest, LinkPreloadedScriptIsNotEligible) {
  SimRequest main_resource("https://example.test/test.html", "text/html");
  SimSubresourceRequest script_resource("https://example.com/script.js",
                                        "application/javascript");

  LoadURL("https://example.test/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <link rel="preload" as="script" href="https://example.com/script.js">
    <script id="target" async src="https://example.com/script.js"></script>
  )HTML");

  Resource* resource = GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/script.js"));
  ASSERT_TRUE(resource);
  ASSERT_TRUE(resource->IsLinkPreload());

  ExpectIneligible(GetDocument(), "target");

  script_resource.Complete("");
}

TEST_F(ClassicPendingScriptTest, AdScriptIsNotEligible) {
  SimRequest main_resource("https://example.test/test.html", "text/html");
  SimSubresourceRequest script_resource("https://example.com/script.js",
                                        "application/javascript");

  LoadURL("https://example.test/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script id="target" async src="https://example.com/script.js"></script>
  )HTML");

  Resource* resource = GetDocument().Fetcher()->CachedResource(
      KURL("https://example.com/script.js"));
  ASSERT_TRUE(resource);
  resource->SetIsAdResource(NoProvenance{});
  ASSERT_TRUE(resource->GetResourceRequest().IsAdResource());

  ExpectIneligible(GetDocument(), "target");

  script_resource.Complete("");
}

TEST_F(ClassicPendingScriptTest, DocumentWrittenScriptIsNotEligible) {
  SimRequest main_resource("https://example.test/test.html", "text/html");
  SimSubresourceRequest script_resource("https://example.com/script.js",
                                        "application/javascript");

  LoadURL("https://example.test/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script>
      document.write(
          '<script id="target" async ' +
          'src="https://example.com/script.js"><\/script>');
    </script>
  )HTML");

  PendingScript* pending_script =
      GetPendingClassicAsyncScript(GetDocument(), "target");
  ASSERT_TRUE(pending_script);
  ASSERT_TRUE(pending_script->is_in_document_write());
  EXPECT_FALSE(pending_script->IsEligibleForLowPriorityAsyncScriptExecution());

  script_resource.Complete("");
}

TEST_F(ClassicPendingScriptTest, FetchPriorityHighIsNotEligible) {
  SimRequest main_resource("https://example.test/test.html", "text/html");
  SimSubresourceRequest script_resource("https://example.com/script.js",
                                        "application/javascript");

  LoadURL("https://example.test/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script id="target" async fetchpriority="high"
            src="https://example.com/script.js"></script>
  )HTML");

  ExpectIneligible(GetDocument(), "target");

  script_resource.Complete("");
}

TEST_F(ClassicPendingScriptTest, FetchPriorityLowAndAutoAreEligible) {
  SimRequest main_resource("https://example.test/test.html", "text/html");
  SimSubresourceRequest low_script("https://example.com/low.js",
                                   "application/javascript");
  SimSubresourceRequest auto_script("https://example.com/auto.js",
                                    "application/javascript");

  LoadURL("https://example.test/test.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <script id="low" async fetchpriority="low"
            src="https://example.com/low.js"></script>
    <script id="auto" async fetchpriority="auto"
            src="https://example.com/auto.js"></script>
  )HTML");

  ExpectEligible(GetDocument(), "low");
  ExpectEligible(GetDocument(), "auto");

  low_script.Complete("");
  auto_script.Complete("");
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace blink
