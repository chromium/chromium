// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/ad_tracker.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class TestAdTracker : public AdTracker {
 public:
  explicit TestAdTracker(LocalFrame* frame) : AdTracker(frame) {}
  void SetScriptAtTopOfStack(const String& url) { script_at_top_ = url; }
  void SetExecutionContext(ExecutionContext* execution_context) {
    execution_context_ = execution_context;
  }

  void SetAdSuffix(const String& ad_suffix) { ad_suffix_ = ad_suffix; }
  ~TestAdTracker() override {}

  void Trace(Visitor* visitor) override {
    visitor->Trace(execution_context_);
    AdTracker::Trace(visitor);
  }

  bool RequestWithUrlTaggedAsAd(const String& url) const {
    DCHECK(is_ad_.Contains(url));
    return is_ad_.at(url);
  }

 protected:
  String ScriptAtTopOfStack(ExecutionContext* execution_context) override {
    if (script_at_top_.IsEmpty())
      return AdTracker::ScriptAtTopOfStack(execution_context);

    return script_at_top_;
  }

  ExecutionContext* GetCurrentExecutionContext() override {
    if (!execution_context_)
      return AdTracker::GetCurrentExecutionContext();

    return execution_context_;
  }

  bool CalculateIfAdSubresource(ExecutionContext* execution_context,
                                const ResourceRequest& resource_request,
                                ResourceType resource_type,
                                bool ad_request) override {
    if (!ad_suffix_.IsEmpty() &&
        resource_request.Url().GetString().EndsWith(ad_suffix_)) {
      ad_request = true;
    }

    ad_request = AdTracker::CalculateIfAdSubresource(
        execution_context, resource_request, resource_type, ad_request);

    is_ad_.insert(resource_request.Url().GetString(), ad_request);
    return ad_request;
  }

 private:
  HashMap<String, bool> is_ad_;
  String script_at_top_;
  Member<ExecutionContext> execution_context_;
  String ad_suffix_;
};

}  // namespace

class AdTrackerTest : public testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
  LocalFrame* GetFrame() const {
    return page_holder_->GetDocument().GetFrame();
  }

  void CreateAdTracker() {
    if (ad_tracker_)
      ad_tracker_->Shutdown();
    ad_tracker_ = MakeGarbageCollected<TestAdTracker>(GetFrame());
    ad_tracker_->SetExecutionContext(&page_holder_->GetDocument());
  }

  void WillExecuteScript(const String& script_url) {
    ad_tracker_->WillExecuteScript(&page_holder_->GetDocument(),
                                   String(script_url));
  }

  void DidExecuteScript() { ad_tracker_->DidExecuteScript(); }

  bool AnyExecutingScriptsTaggedAsAdResource() {
    return ad_tracker_->IsAdScriptInStack();
  }

  void AppendToKnownAdScripts(const String& url) {
    ad_tracker_->AppendToKnownAdScripts(page_holder_->GetDocument(), url);
  }

  Persistent<TestAdTracker> ad_tracker_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

void AdTrackerTest::SetUp() {
  page_holder_ = std::make_unique<DummyPageHolder>(IntSize(800, 600));
  page_holder_->GetDocument().SetURL(KURL("https://example.com/foo"));
  CreateAdTracker();
}

void AdTrackerTest::TearDown() {
  ad_tracker_->Shutdown();
}

TEST_F(AdTrackerTest, AnyExecutingScriptsTaggedAsAdResource) {
  String ad_script_url("https://example.com/bar.js");
  AppendToKnownAdScripts(ad_script_url);

  WillExecuteScript("https://example.com/foo.js");
  WillExecuteScript("https://example.com/bar.js");
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
}

TEST_F(AdTrackerTest, TopOfStackOnly_NoAdsOnTop) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTopOfStackAdTagging);
  CreateAdTracker();

  String ad_script_url("https://example.com/bar.js");
  AppendToKnownAdScripts(ad_script_url);

  WillExecuteScript(ad_script_url);
  WillExecuteScript("https://example.com/foo.js");
  ad_tracker_->SetScriptAtTopOfStack("https://www.example.com/baz.js");

  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());
}

TEST_F(AdTrackerTest, TopOfStackOnly_AdsOnTop) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kTopOfStackAdTagging);
  CreateAdTracker();

  String ad_script_url("https://example.com/bar.js");
  AppendToKnownAdScripts(ad_script_url);

  WillExecuteScript(ad_script_url);
  WillExecuteScript("https://example.com/foo.js");
  ad_tracker_->SetScriptAtTopOfStack(ad_script_url);

  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
}

// Tests that if neither script in the stack is an ad,
// AnyExecutingScriptsTaggedAsAdResource should return false.
TEST_F(AdTrackerTest, AnyExecutingScriptsTaggedAsAdResource_False) {
  WillExecuteScript("https://example.com/foo.js");
  WillExecuteScript("https://example.com/bar.js");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());
}

TEST_F(AdTrackerTest, TopOfStackIncluded) {
  String ad_script_url("https://example.com/ad.js");
  AppendToKnownAdScripts(ad_script_url);

  WillExecuteScript("https://example.com/foo.js");
  WillExecuteScript("https://example.com/bar.js");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  ad_tracker_->SetScriptAtTopOfStack("https://www.example.com/baz.js");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  ad_tracker_->SetScriptAtTopOfStack(ad_script_url);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());

  ad_tracker_->SetScriptAtTopOfStack("https://www.example.com/baz.js");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  ad_tracker_->SetScriptAtTopOfStack("");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  ad_tracker_->SetScriptAtTopOfStack(String());
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  WillExecuteScript(ad_script_url);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
}

TEST_F(AdTrackerTest, AdStackFrameCounting) {
  AppendToKnownAdScripts("https://example.com/ad.js");

  WillExecuteScript("https://example.com/vanilla.js");
  WillExecuteScript("https://example.com/vanilla.js");
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  WillExecuteScript("https://example.com/ad.js");
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());

  DidExecuteScript();
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  WillExecuteScript("https://example.com/ad.js");
  WillExecuteScript("https://example.com/ad.js");
  WillExecuteScript("https://example.com/vanilla.js");
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());

  DidExecuteScript();
  DidExecuteScript();
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());

  DidExecuteScript();
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  DidExecuteScript();
  DidExecuteScript();
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  WillExecuteScript("https://example.com/ad.js");
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
}

TEST_F(AdTrackerTest, AsyncTagging) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAsyncStackAdTagging);
  CreateAdTracker();

  // Put an ad script on the stack.
  AppendToKnownAdScripts("https://example.com/ad.js");
  WillExecuteScript("https://example.com/ad.js");
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());

  // Create a fake task void*.
  probe::AsyncTaskId async_task;

  // Create an async task while ad script is running.
  ad_tracker_->DidCreateAsyncTask(&async_task);

  // Finish executing the ad script.
  DidExecuteScript();
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  // Start and stop the async task created by the ad script.
  ad_tracker_->DidStartAsyncTask(&async_task);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
  ad_tracker_->DidFinishAsyncTask(&async_task);
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  // Do it again.
  ad_tracker_->DidStartAsyncTask(&async_task);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
  ad_tracker_->DidFinishAsyncTask(&async_task);
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  // Call the task recursively.
  ad_tracker_->DidStartAsyncTask(&async_task);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
  ad_tracker_->DidStartAsyncTask(&async_task);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
  ad_tracker_->DidFinishAsyncTask(&async_task);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
  ad_tracker_->DidFinishAsyncTask(&async_task);
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());
}

class AdTrackerSimTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    main_resource_ = std::make_unique<SimRequest>(
        "https://example.com/test.html", "text/html");

    LoadURL("https://example.com/test.html");
    ad_tracker_ = MakeGarbageCollected<TestAdTracker>(GetDocument().GetFrame());
    GetDocument().GetFrame()->SetAdTrackerForTesting(ad_tracker_);
  }

  void TearDown() override {
    ad_tracker_->Shutdown();
    SimTest::TearDown();
  }

  bool IsKnownAdScript(ExecutionContext* execution_context, const String& url) {
    return ad_tracker_->IsKnownAdScript(execution_context, url);
  }

  std::unique_ptr<SimRequest> main_resource_;
  Persistent<TestAdTracker> ad_tracker_;
};

// Script loaded by ad script is tagged as ad.
TEST_F(AdTrackerSimTest, ScriptLoadedWhileExecutingAdScript) {
  const char kAdUrl[] = "https://example.com/ad_script.js";
  const char kVanillaUrl[] = "https://example.com/vanilla_script.js";
  SimSubresourceRequest ad_resource(kAdUrl, "text/javascript");
  SimSubresourceRequest vanilla_script(kVanillaUrl, "text/javascript");

  ad_tracker_->SetAdSuffix("ad_script.js");

  main_resource_->Complete("<body></body><script src=ad_script.js></script>");

  ad_resource.Complete(R"SCRIPT(
    script = document.createElement("script");
    script.src = "vanilla_script.js";
    document.body.appendChild(script);
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  vanilla_script.Complete("");

  EXPECT_TRUE(IsKnownAdScript(&GetDocument(), kAdUrl));
  EXPECT_TRUE(IsKnownAdScript(&GetDocument(), kVanillaUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kVanillaUrl));
}

// Unknown script running in an ad context should be labeled as ad script.
TEST_F(AdTrackerSimTest, ScriptDetectedByContext) {
  const char kAdScriptUrl[] = "https://example.com/ad_script.js";
  SimSubresourceRequest ad_script(kAdScriptUrl, "text/javascript");

  ad_tracker_->SetAdSuffix("ad_script.js");

  // Create an iframe that's considered an ad.
  main_resource_->Complete("<body><script src='ad_script.js'></script></body>");
  ad_script.Complete(R"SCRIPT(
    frame = document.createElement("iframe");
    document.body.appendChild(frame);
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  // The child frame should be an ad subframe.
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame->IsAdSubframe());

  // Now run unknown script in the child's context. It should be considered an
  // ad based on context alone.
  ad_tracker_->SetExecutionContext(child_frame->GetDocument());
  ad_tracker_->SetScriptAtTopOfStack("foo.js");
  EXPECT_TRUE(ad_tracker_->IsAdScriptInStack());
}

TEST_F(AdTrackerSimTest, RedirectToAdUrl) {
  SimRequest::Params params;
  params.redirect_url = "https://example.com/ad_script.js";
  SimSubresourceRequest redirect_script(
      "https://example.com/redirect_script.js", "text/javascript", params);
  SimSubresourceRequest ad_script("https://example.com/ad_script.js",
                                  "text/javascript");

  ad_tracker_->SetAdSuffix("ad_script.js");

  main_resource_->Complete(
      "<body><script src='redirect_script.js'></script></body>");

  ad_script.Complete("");

  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(
      "https://example.com/redirect_script.js"));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(
      "https://example.com/ad_script.js"));
}

TEST_F(AdTrackerSimTest, AdResourceDetectedByContext) {
  SimSubresourceRequest ad_script("https://example.com/ad_script.js",
                                  "text/javascript");
  SimRequest ad_frame("https://example.com/ad_frame.html", "text/html");
  SimSubresourceRequest foo_css("https://example.com/foo.css", "text/style");
  ad_tracker_->SetAdSuffix("ad_script.js");

  // Create an iframe that's considered an ad.
  main_resource_->Complete("<body><script src='ad_script.js'></script></body>");
  ad_script.Complete(R"SCRIPT(
    frame = document.createElement("iframe");
    frame.src="ad_frame.html";
    document.body.appendChild(frame);
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  // The child frame should be an ad subframe.
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame->IsAdSubframe());

  // Load a resource from the frame. It should be detected as an ad resource due
  // to its context.
  ad_frame.Complete(R"HTML(
    <link rel="stylesheet" href="foo.css">
    )HTML");

  foo_css.Complete("");

  EXPECT_TRUE(
      ad_tracker_->RequestWithUrlTaggedAsAd("https://example.com/foo.css"));
}

// When inline script in an ad frame inserts an iframe into a non-ad frame, the
// new frame should be considered an ad.
TEST_F(AdTrackerSimTest, InlineAdScriptRunningInNonAdContext) {
  SimSubresourceRequest ad_script("https://example.com/ad_script.js",
                                  "text/javascript");
  SimRequest ad_iframe("https://example.com/ad_frame.html", "text/html");
  ad_tracker_->SetAdSuffix("ad_script.js");

  main_resource_->Complete("<body><script src='ad_script.js'></script></body>");
  ad_script.Complete(R"SCRIPT(
    frame = document.createElement("iframe");
    frame.src = "ad_frame.html";
    document.body.appendChild(frame);
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  // Verify that the new frame is an ad frame.
  EXPECT_TRUE(To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild())
                  ->IsAdSubframe());

  // Create a new sibling frame to the ad frame. The ad context calls the non-ad
  // context's (top frame) appendChild.
  ad_iframe.Complete(R"HTML(
    <script>
      frame = document.createElement("iframe");
      frame.name = "ad_sibling";
      parent.document.body.appendChild(frame);
    </script>
    )HTML");

  // The new sibling frame should also be identified as an ad.
  EXPECT_TRUE(
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild("ad_sibling"))
          ->IsAdSubframe());
}

// Image loaded by ad script is tagged as ad.
TEST_F(AdTrackerSimTest, ImageLoadedWhileExecutingAdScriptAsyncEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAsyncStackAdTagging);

  // Reset the AdTracker so that it gets the latest base::Feature value on
  // construction.
  ad_tracker_ = MakeGarbageCollected<TestAdTracker>(GetDocument().GetFrame());
  GetDocument().GetFrame()->SetAdTrackerForTesting(ad_tracker_);

  const char kAdUrl[] = "https://example.com/ad_script.js";
  const char kVanillaUrl[] = "https://example.com/vanilla_image.jpg";
  SimSubresourceRequest ad_resource(kAdUrl, "text/javascript");
  SimSubresourceRequest vanilla_image(kVanillaUrl, "image/jpeg");

  ad_tracker_->SetAdSuffix("ad_script.js");

  main_resource_->Complete("<body></body><script src=ad_script.js></script>");

  ad_resource.Complete(R"SCRIPT(
    image = document.createElement("img");
    image.src = "vanilla_image.jpg";
    document.body.appendChild(image);
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  vanilla_image.Complete("");

  EXPECT_TRUE(IsKnownAdScript(&GetDocument(), kAdUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdUrl));

  // Image loading is async, so we should catch this when async stacks are
  // monitored.
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kVanillaUrl));
}

// Image loaded by ad script is tagged as ad.
TEST_F(AdTrackerSimTest, ImageLoadedWhileExecutingAdScriptAsyncDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAsyncStackAdTagging);

  // Reset the AdTracker so that it gets the latest base::Feature value on
  // construction.
  ad_tracker_ = MakeGarbageCollected<TestAdTracker>(GetDocument().GetFrame());
  GetDocument().GetFrame()->SetAdTrackerForTesting(ad_tracker_);

  const char kAdUrl[] = "https://example.com/ad_script.js";
  const char kVanillaUrl[] = "https://example.com/vanilla_image.jpg";
  SimSubresourceRequest ad_resource(kAdUrl, "text/javascript");
  SimSubresourceRequest vanilla_image(kVanillaUrl, "image/jpeg");

  ad_tracker_->SetAdSuffix("ad_script.js");

  main_resource_->Complete("<body></body><script src=ad_script.js></script>");

  ad_resource.Complete(R"SCRIPT(
    image = document.createElement("img");
    image.src = "vanilla_image.jpg";
    document.body.appendChild(image);
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  vanilla_image.Complete("");

  EXPECT_TRUE(IsKnownAdScript(&GetDocument(), kAdUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdUrl));

  // Image loading is async, so we won't catch this when async stacks aren't
  // monitored.
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(kVanillaUrl));
}

// Frame loaded by ad script is tagged as ad.
TEST_F(AdTrackerSimTest, FrameLoadedWhileExecutingAdScript) {
  const char kAdUrl[] = "https://example.com/ad_script.js";
  const char kVanillaUrl[] = "https://example.com/vanilla_page.html";
  const char kVanillaImgUrl[] = "https://example.com/vanilla_img.jpg";
  SimSubresourceRequest ad_resource(kAdUrl, "text/javascript");
  SimRequest vanilla_page(kVanillaUrl, "text/html");
  SimSubresourceRequest vanilla_image(kVanillaImgUrl, "image/jpeg");

  ad_tracker_->SetAdSuffix("ad_script.js");

  main_resource_->Complete("<body></body><script src=ad_script.js></script>");

  ad_resource.Complete(R"SCRIPT(
    iframe = document.createElement("iframe");
    iframe.src = "vanilla_page.html";
    document.body.appendChild(iframe);
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  vanilla_page.Complete("<img src=vanilla_img.jpg></img>");
  vanilla_image.Complete("");

  EXPECT_TRUE(IsKnownAdScript(&GetDocument(), kAdUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdUrl));
  Frame* child_frame = GetDocument().GetFrame()->Tree().FirstChild();
  EXPECT_TRUE(To<LocalFrame>(child_frame)->IsAdSubframe());
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kVanillaImgUrl));
}

// A script tagged as an ad in one frame shouldn't cause it to be considered
// an ad when executed in another frame.
TEST_F(AdTrackerSimTest, Contexts) {
  // Load a page that loads library.js. It also creates an iframe that also
  // loads library.js (where it gets tagged as an ad). Even though library.js
  // gets tagged as an ad script in the subframe, that shouldn't cause it to
  // be treated as an ad in the main frame.
  SimRequest iframe_resource("https://example.com/iframe.html", "text/html");
  SimSubresourceRequest library_resource("https://example.com/library.js",
                                         "text/javascript");

  main_resource_->Complete(R"HTML(
    <script src=library.js></script>
    <iframe src=iframe.html></iframe>
    )HTML");

  // Complete the main frame's library.js.
  library_resource.Complete("");

  // The library script is loaded for a second time, this time in the
  // subframe. Mark it as an ad.
  SimSubresourceRequest library_resource_for_subframe(
      "https://example.com/library.js", "text/javascript");
  ad_tracker_->SetAdSuffix("library.js");

  iframe_resource.Complete(R"HTML(
    <script src="library.js"></script>
    )HTML");
  library_resource_for_subframe.Complete("");

  // Verify that library.js is an ad script in the subframe's context but not
  // in the main frame's context.
  Frame* subframe = GetDocument().GetFrame()->Tree().FirstChild();
  auto* local_subframe = To<LocalFrame>(subframe);
  EXPECT_TRUE(IsKnownAdScript(local_subframe->GetDocument(),
                              String("https://example.com/library.js")));

  EXPECT_FALSE(IsKnownAdScript(&GetDocument(),
                               String("https://example.com/library.js")));
}

TEST_F(AdTrackerSimTest, SameOriginSubframeFromAdScript) {
  SimSubresourceRequest ad_resource("https://example.com/ad_script.js",
                                    "text/javascript");
  SimRequest iframe_resource("https://example.com/iframe.html", "text/html");
  ad_tracker_->SetAdSuffix("ad_script.js");

  main_resource_->Complete(R"HTML(
    <body></body><script src=ad_script.js></script>
    )HTML");
  ad_resource.Complete(R"SCRIPT(
    var iframe = document.createElement("iframe");
    iframe.src = "iframe.html";
    document.body.appendChild(iframe);
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  iframe_resource.Complete("iframe data");

  Frame* subframe = GetDocument().GetFrame()->Tree().FirstChild();
  auto* local_subframe = To<LocalFrame>(subframe);
  EXPECT_TRUE(local_subframe->IsAdSubframe());
}

TEST_F(AdTrackerSimTest, SameOriginDocWrittenSubframeFromAdScript) {
  SimSubresourceRequest ad_resource("https://example.com/ad_script.js",
                                    "text/javascript");
  ad_tracker_->SetAdSuffix("ad_script.js");

  main_resource_->Complete(R"HTML(
    <body></body><script src=ad_script.js></script>
    )HTML");
  ad_resource.Complete(R"SCRIPT(
    var iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    var iframeDocument = iframe.contentWindow.document;
    iframeDocument.open();
    iframeDocument.write("iframe data");
    iframeDocument.close();
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  Frame* subframe = GetDocument().GetFrame()->Tree().FirstChild();
  auto* local_subframe = To<LocalFrame>(subframe);
  EXPECT_TRUE(local_subframe->IsAdSubframe());
}

class AdTrackerDisabledSimTest : public SimTest,
                                 private ScopedAdTaggingForTest {
 protected:
  AdTrackerDisabledSimTest() : ScopedAdTaggingForTest(false) {}
  void SetUp() override {
    SimTest::SetUp();
    main_resource_ = std::make_unique<SimRequest>(
        "https://example.com/test.html", "text/html");

    LoadURL("https://example.com/test.html");
  }

  std::unique_ptr<SimRequest> main_resource_;
};

TEST_F(AdTrackerDisabledSimTest, VerifyAdTrackingDisabled) {
  main_resource_->Complete("<body></body>");
  EXPECT_FALSE(GetDocument().GetFrame()->GetAdTracker());
  EXPECT_FALSE(GetDocument().GetFrame()->IsAdSubframe());
}

}  // namespace blink
