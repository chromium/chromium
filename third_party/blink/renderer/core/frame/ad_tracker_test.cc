// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/ad_tracker.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/probe/async_task_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

const unsigned char kSmallGifData[] = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01,
                                       0x00, 0x01, 0x00, 0x00, 0xff, 0x00, 0x2c,
                                       0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
                                       0x00, 0x00, 0x02, 0x00, 0x3b};

// The pages include a div with class="test" to ensure the resources in the
// stylesheet are loaded.
const char kPageWithVanillaExternalStylesheet[] = R"HTML(
    <head><link rel="stylesheet" href="style.css"></head>
    <body><div class="test">Test</div></body>
    )HTML";
const char kPageWithAdExternalStylesheet[] = R"HTML(
    <head><link rel="stylesheet" href="style.css?ad=true"></head>
    <body><div class="test">Test</div></body>
    )HTML";
const char kPageWithVanillaScript[] = R"HTML(
    <head><script defer src="script.js"></script></head>
    <body><div class="test">Test</div></body>
    )HTML";
const char kPageWithAdScript[] = R"HTML(
    <head><script defer src="script.js?ad=true"></script></head>
    <body><div class="test">Test</div></body>
    )HTML";
const char kPageWithFrame[] = R"HTML(
    <head></head>
    <body><div class="test">Test</div><iframe src="frame.html"></iframe></body>
    )HTML";
const char kPageWithStyleTagLoadingVanillaResources[] = R"HTML(
    <head><style>
      @font-face {
        font-family: "Vanilla";
        src: url("font.woff2") format("woff2");
      }
      .test {
        font-family: "Vanilla";
        background-image: url("pixel.png");
      }
    </style></head>
    <body><div class="test">Test</div></body>
    )HTML";

const char kStylesheetWithVanillaResources[] = R"CSS(
    @font-face {
      font-family: "Vanilla";
      src: url("font.woff2") format("woff2");
    }
    .test {
      font-family: "Vanilla";
      background-image: url("pixel.png");
    }
    )CSS";
const char kStylesheetWithAdResources[] = R"CSS(
    @font-face {
      font-family: "Ad";
      src: url("font.woff2?ad=true") format("woff2");
    }
    .test {
      font-family: "Ad";
      background-image: url("pixel.png?ad=true");
    }
    )CSS";

class TestAdTracker : public AdTracker {
 public:
  explicit TestAdTracker(LocalFrame* frame) : AdTracker(frame) {}
  void SetScriptAtTopOfStack(const String& url) { script_at_top_ = url; }
  void SetExecutionContext(ExecutionContext* execution_context) {
    execution_context_ = execution_context;
  }

  void SetAdSuffix(const String& ad_suffix) { ad_suffix_ = ad_suffix; }
  ~TestAdTracker() override {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(execution_context_);
    AdTracker::Trace(visitor);
  }

  bool RequestWithUrlTaggedAsAd(const String& url) const {
    DCHECK(is_ad_.Contains(url));
    return is_ad_.at(url);
  }

  bool UrlHasBeenRequested(const String& url) const {
    return is_ad_.Contains(url);
  }

  void SetSimTest() { sim_test_ = true; }

  void WaitForSubresource(const String& url) {
    if (base::Contains(is_ad_, url)) {
      return;
    }
    url_to_wait_for_ = url;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 protected:
  String ScriptAtTopOfStack() override {
    if (sim_test_ && !script_at_top_)
      return AdTracker::ScriptAtTopOfStack();
    return script_at_top_;
  }

  ExecutionContext* GetCurrentExecutionContext() override {
    if (!execution_context_)
      return AdTracker::GetCurrentExecutionContext();

    return execution_context_.Get();
  }

  bool CalculateIfAdSubresource(ExecutionContext* execution_context,
                                const KURL& request_url,
                                ResourceType resource_type,
                                const FetchInitiatorInfo& initiator_info,
                                bool ad_request) override {
    if (!ad_suffix_.empty() && request_url.GetString().EndsWith(ad_suffix_)) {
      ad_request = true;
    }

    ad_request = AdTracker::CalculateIfAdSubresource(
        execution_context, request_url, resource_type, initiator_info,
        ad_request);

    String resource_url = request_url.GetString();
    is_ad_.insert(resource_url, ad_request);

    if (quit_closure_ && url_to_wait_for_ == resource_url) {
      std::move(quit_closure_).Run();
    }
    return ad_request;
  }

 private:
  HashMap<String, bool> is_ad_;
  String script_at_top_;
  Member<ExecutionContext> execution_context_;
  String ad_suffix_;
  bool sim_test_ = false;

  base::OnceClosure quit_closure_;
  String url_to_wait_for_;
};

void SetIsAdFrame(LocalFrame* frame) {
  DCHECK(frame);
  blink::FrameAdEvidence ad_evidence(frame->Parent() &&
                                     frame->Parent()->IsAdFrame());
  ad_evidence.set_created_by_ad_script(
      mojom::FrameCreationStackEvidence::kCreatedByAdScript);
  ad_evidence.set_is_complete();
  frame->SetAdEvidence(ad_evidence);
}

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
    ad_tracker_->SetExecutionContext(GetExecutionContext());
  }

  void WillExecuteScript(const String& script_url,
                         int script_id = v8::Message::kNoScriptIdInfo) {
    auto* execution_context = GetExecutionContext();
    ad_tracker_->WillExecuteScript(
        execution_context, execution_context->GetIsolate()->GetCurrentContext(),
        String(script_url), script_id);
  }

  ExecutionContext* GetExecutionContext() {
    return page_holder_->GetFrame().DomWindow();
  }

  void DidExecuteScript() { ad_tracker_->DidExecuteScript(); }

  bool AnyExecutingScriptsTaggedAsAdResource() {
    return AnyExecutingScriptsTaggedAsAdResourceWithStackType(
        AdTracker::StackType::kBottomAndTop);
  }

  bool AnyExecutingScriptsTaggedAsAdResourceWithStackType(
      AdTracker::StackType stack_type) {
    return ad_tracker_->IsAdScriptInStack(stack_type);
  }

  std::optional<AdScriptIdentifier> BottommostAdScript() {
    std::optional<AdScriptIdentifier> bottom_most_ad_script;
    ad_tracker_->IsAdScriptInStack(AdTracker::StackType::kBottomAndTop,
                                   /*out_ad_script=*/&bottom_most_ad_script);
    return bottom_most_ad_script;
  }

  void AppendToKnownAdScripts(const String& url) {
    ad_tracker_->AppendToKnownAdScripts(*GetExecutionContext(), url);
  }

  void AppendToKnownAdScripts(int script_id) {
    // Matches AdTracker's inline script encoding
    AppendToKnownAdScripts(String::Format("{ id %d }", script_id));
  }

  test::TaskEnvironment task_environment_;
  Persistent<TestAdTracker> ad_tracker_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

void AdTrackerTest::SetUp() {
  page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
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

TEST_F(AdTrackerTest, BottomScriptTaggedAsAdResource) {
  AppendToKnownAdScripts("https://example.com/ad.js");

  WillExecuteScript("https://example.com/ad.js");
  ad_tracker_->SetScriptAtTopOfStack("https://example.com/vanilla.js");
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResourceWithStackType(
      AdTracker::StackType::kBottomAndTop));
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResourceWithStackType(
      AdTracker::StackType::kBottomOnly));
}

TEST_F(AdTrackerTest, TopScriptTaggedAsAdResource) {
  AppendToKnownAdScripts("https://example.com/ad.js");

  WillExecuteScript("https://example.com/vanilla.js");
  ad_tracker_->SetScriptAtTopOfStack("https://example.com/ad.js");

  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResourceWithStackType(
      AdTracker::StackType::kBottomAndTop));
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResourceWithStackType(
      AdTracker::StackType::kBottomOnly));
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
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResourceWithStackType(
      AdTracker::StackType::kBottomOnly));

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
  CreateAdTracker();

  // Put an ad script on the stack.
  AppendToKnownAdScripts("https://example.com/ad.js");
  WillExecuteScript("https://example.com/ad.js");
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());

  // Create a fake task void*.
  probe::AsyncTaskContext async_task_context;

  // Create an async task while ad script is running.
  ad_tracker_->DidCreateAsyncTask(&async_task_context);

  // Finish executing the ad script.
  DidExecuteScript();
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  // Start and stop the async task created by the ad script.
  ad_tracker_->DidStartAsyncTask(&async_task_context);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
  ad_tracker_->DidFinishAsyncTask(&async_task_context);
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  // Do it again.
  ad_tracker_->DidStartAsyncTask(&async_task_context);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
  ad_tracker_->DidFinishAsyncTask(&async_task_context);
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  // Call the task recursively.
  ad_tracker_->DidStartAsyncTask(&async_task_context);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
  ad_tracker_->DidStartAsyncTask(&async_task_context);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
  ad_tracker_->DidFinishAsyncTask(&async_task_context);
  EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
  ad_tracker_->DidFinishAsyncTask(&async_task_context);
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());
}

TEST_F(AdTrackerTest, BottommostAdScript) {
  AppendToKnownAdScripts("https://example.com/ad.js");
  AppendToKnownAdScripts("https://example.com/ad2.js");
  AppendToKnownAdScripts(/*script_id=*/5);
  EXPECT_FALSE(BottommostAdScript().has_value());

  WillExecuteScript("https://example.com/vanilla.js", /*script_id=*/1);
  EXPECT_FALSE(BottommostAdScript().has_value());

  WillExecuteScript("https://example.com/ad.js", /*script_id=*/2);
  ASSERT_TRUE(BottommostAdScript().has_value());
  EXPECT_EQ(BottommostAdScript()->id, 2);

  // Additional scripts (ad or not) don't change the bottommost ad script.
  WillExecuteScript("https://example.com/vanilla.js", /*script_id=*/3);
  ASSERT_TRUE(BottommostAdScript().has_value());
  EXPECT_EQ(BottommostAdScript()->id, 2);
  DidExecuteScript();

  WillExecuteScript("https://example.com/ad2.js", /*script_id=*/4);
  ASSERT_TRUE(BottommostAdScript().has_value());
  EXPECT_EQ(BottommostAdScript()->id, 2);
  DidExecuteScript();

  // The bottommost ad script can have an empty name.
  DidExecuteScript();
  EXPECT_FALSE(BottommostAdScript().has_value());

  WillExecuteScript("", /*script_id=*/5);
  ASSERT_TRUE(BottommostAdScript().has_value());
  EXPECT_EQ(BottommostAdScript()->id, 5);
}

TEST_F(AdTrackerTest, BottommostAsyncAdScript) {
  CreateAdTracker();

  // Put an ad script on the stack.
  AppendToKnownAdScripts("https://example.com/ad.js");
  AppendToKnownAdScripts("https://example.com/ad2.js");

  EXPECT_FALSE(BottommostAdScript().has_value());

  // Create a couple of async tasks while ad script is running.
  WillExecuteScript("https://example.com/ad.js", 1);
  probe::AsyncTaskContext async_task_context1;
  ad_tracker_->DidCreateAsyncTask(&async_task_context1);
  DidExecuteScript();
  EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());

  WillExecuteScript("https://example.com/ad2.js", 2);
  probe::AsyncTaskContext async_task_context2;
  ad_tracker_->DidCreateAsyncTask(&async_task_context2);
  DidExecuteScript();

  // Start and stop the async task created by the ad script.
  {
    ad_tracker_->DidStartAsyncTask(&async_task_context1);
    EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_TRUE(BottommostAdScript().has_value());
    EXPECT_EQ(BottommostAdScript()->id, 1);

    ad_tracker_->DidFinishAsyncTask(&async_task_context1);
    EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_FALSE(BottommostAdScript().has_value());
  }

  // Run two async tasks
  {
    ad_tracker_->DidStartAsyncTask(&async_task_context1);
    EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_TRUE(BottommostAdScript().has_value());
    EXPECT_EQ(BottommostAdScript()->id, 1);

    ad_tracker_->DidStartAsyncTask(&async_task_context2);
    EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_TRUE(BottommostAdScript().has_value());
    EXPECT_EQ(BottommostAdScript()->id, 1);

    ad_tracker_->DidFinishAsyncTask(&async_task_context2);
    EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_TRUE(BottommostAdScript().has_value());
    EXPECT_EQ(BottommostAdScript()->id, 1);

    ad_tracker_->DidFinishAsyncTask(&async_task_context1);
    EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_FALSE(BottommostAdScript().has_value());
  }

  // Run an async task followed by sync.
  {
    ad_tracker_->DidStartAsyncTask(&async_task_context2);
    EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_TRUE(BottommostAdScript().has_value());
    EXPECT_EQ(BottommostAdScript()->id, 2);

    WillExecuteScript("https://example.com/ad.js");
    EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_TRUE(BottommostAdScript().has_value());
    EXPECT_EQ(BottommostAdScript()->id, 2);

    ad_tracker_->DidStartAsyncTask(&async_task_context1);
    EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_TRUE(BottommostAdScript().has_value());
    EXPECT_EQ(BottommostAdScript()->id, 2);

    ad_tracker_->DidFinishAsyncTask(&async_task_context1);
    EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_TRUE(BottommostAdScript().has_value());
    EXPECT_EQ(BottommostAdScript()->id, 2);

    DidExecuteScript();
    EXPECT_TRUE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_TRUE(BottommostAdScript().has_value());
    EXPECT_EQ(BottommostAdScript()->id, 2);

    ad_tracker_->DidFinishAsyncTask(&async_task_context2);
    EXPECT_FALSE(AnyExecutingScriptsTaggedAsAdResource());
    EXPECT_FALSE(BottommostAdScript().has_value());
  }
}

class AdTrackerSimTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    main_resource_ = std::make_unique<SimRequest>(
        "https://example.com/test.html", "text/html");

    LoadURL("https://example.com/test.html");
    ad_tracker_ = MakeGarbageCollected<TestAdTracker>(GetDocument().GetFrame());
    ad_tracker_->SetSimTest();
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

  EXPECT_TRUE(IsKnownAdScript(GetDocument().GetExecutionContext(), kAdUrl));
  EXPECT_TRUE(
      IsKnownAdScript(GetDocument().GetExecutionContext(), kVanillaUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kVanillaUrl));
}

// Unknown script running in an ad context should be labeled as ad script.
TEST_F(AdTrackerSimTest, ScriptDetectedByContext) {
  // Create an iframe that's considered an ad.
  main_resource_->Complete("<body><iframe></iframe></body>");
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  SetIsAdFrame(child_frame);

  // Now run unknown script in the child's context. It should be considered an
  // ad based on context alone.
  ad_tracker_->SetExecutionContext(child_frame->DomWindow());
  ad_tracker_->SetScriptAtTopOfStack("foo.js");
  EXPECT_TRUE(
      ad_tracker_->IsAdScriptInStack(AdTracker::StackType::kBottomAndTop));
}

TEST_F(AdTrackerSimTest, EventHandlerForPostMessageFromAdFrame_NoAdInStack) {
  const char kAdScriptUrl[] = "https://example.com/ad_script.js";
  SimSubresourceRequest ad_script(kAdScriptUrl, "text/javascript");
  const char kVanillaUrl[] = "https://example.com/vanilla_script.js";
  SimSubresourceRequest vanilla_script(kVanillaUrl, "text/javascript");

  SimSubresourceRequest image_resource("https://example.com/image.gif",
                                       "image/gif");

  ad_tracker_->SetAdSuffix("ad_script.js");

  // Create an iframe that's considered an ad.
  main_resource_->Complete(R"(<body>
    <script src='vanilla_script.js'></script>
    <script src='ad_script.js'></script>
    </body>)");

  // Register a postMessage handler which is not considered to be ad script,
  // which loads an image.
  vanilla_script.Complete(R"SCRIPT(
    window.addEventListener('message', e => {
      image = document.createElement("img");
      image.src = "image.gif";
      document.body.appendChild(image);
    });)SCRIPT");

  // Post message from an ad iframe to the non-ad script in the parent frame.
  ad_script.Complete(R"SCRIPT(
    frame = document.createElement("iframe");
    document.body.appendChild(frame);
    iframeDocument = frame.contentWindow.document;
    iframeDocument.open();
    iframeDocument.write(
      "<html><script>window.parent.postMessage('a', '*');</script></html>");
    iframeDocument.close();
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  image_resource.Complete("data");

  // The image should not be considered an ad even if it was loaded in response
  // to an ad initiated postMessage.
  EXPECT_FALSE(
      ad_tracker_->RequestWithUrlTaggedAsAd("https://example.com/image.gif"));
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
  SimRequest ad_frame("https://example.com/ad_frame.html", "text/html");
  SimSubresourceRequest foo_css("https://example.com/foo.css", "text/style");

  // Create an iframe that's considered an ad.
  main_resource_->Complete(
      "<body><iframe src='ad_frame.html'></iframe></body>");
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  SetIsAdFrame(child_frame);

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
// new frame should be considered as created by ad script (and would therefore
// be tagged as an ad).
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

  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());

  // Verify that the new frame is considered created by ad script then set it
  // as an ad subframe. This emulates the embedder tagging a frame as an ad.
  EXPECT_TRUE(child_frame->IsFrameCreatedByAdScript());
  SetIsAdFrame(child_frame);

  // Create a new sibling frame to the ad frame. The ad context calls the non-ad
  // context's (top frame) appendChild.
  ad_iframe.Complete(R"HTML(
    <script>
      frame = document.createElement("iframe");
      frame.name = "ad_sibling";
      parent.document.body.appendChild(frame);
    </script>
    )HTML");

  // The new sibling frame should also be identified as created by ad script.
  EXPECT_TRUE(To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(
                                 AtomicString("ad_sibling")))
                  ->IsFrameCreatedByAdScript());
}

// Image loaded by ad script is tagged as ad.
TEST_F(AdTrackerSimTest, ImageLoadedWhileExecutingAdScriptAsyncEnabled) {
  // Reset the AdTracker so that it gets the latest base::Feature value on
  // construction.
  ad_tracker_ = MakeGarbageCollected<TestAdTracker>(GetDocument().GetFrame());
  GetDocument().GetFrame()->SetAdTrackerForTesting(ad_tracker_);

  const char kAdUrl[] = "https://example.com/ad_script.js";
  const char kVanillaUrl[] = "https://example.com/vanilla_image.gif";
  SimSubresourceRequest ad_resource(kAdUrl, "text/javascript");
  SimSubresourceRequest vanilla_image(kVanillaUrl, "image/gif");

  ad_tracker_->SetAdSuffix("ad_script.js");

  main_resource_->Complete("<body></body><script src=ad_script.js></script>");

  ad_resource.Complete(R"SCRIPT(
    image = document.createElement("img");
    image.src = "vanilla_image.gif";
    document.body.appendChild(image);
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  // Put the gif bytes in a Vector to avoid difficulty with
  // non null-terminated char*.
  Vector<char> gif;
  gif.Append(kSmallGifData, sizeof(kSmallGifData));

  vanilla_image.Complete(gif);

  EXPECT_TRUE(IsKnownAdScript(GetDocument().GetExecutionContext(), kAdUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdUrl));

  // Image loading is async, so we should catch this when async stacks are
  // monitored.
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kVanillaUrl));

  // Walk through the DOM to get the image element.
  Element* doc_element = GetDocument().documentElement();
  Element* body_element = Traversal<Element>::LastChild(*doc_element);
  HTMLImageElement* image_element =
      Traversal<HTMLImageElement>::FirstChild(*body_element);

  // When async stacks are monitored, we should also tag the
  // HTMLImageElement as ad-related.
  ASSERT_TRUE(image_element);
  EXPECT_TRUE(image_element->IsAdRelated());
}

// Image loaded by ad script is tagged as ad.
TEST_F(AdTrackerSimTest, DataURLImageLoadedWhileExecutingAdScriptAsyncEnabled) {
  // Reset the AdTracker so that it gets the latest base::Feature value on
  // construction.
  ad_tracker_ = MakeGarbageCollected<TestAdTracker>(GetDocument().GetFrame());
  GetDocument().GetFrame()->SetAdTrackerForTesting(ad_tracker_);

  const char kAdUrl[] = "https://example.com/ad_script.js";
  SimSubresourceRequest ad_resource(kAdUrl, "text/javascript");

  ad_tracker_->SetAdSuffix("ad_script.js");

  main_resource_->Complete("<body></body><script src=ad_script.js></script>");

  ad_resource.Complete(R"SCRIPT(
    image = document.createElement("img");
    image.src = "data:image/gif;base64,R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs=";
    document.body.appendChild(image);
    )SCRIPT");

  // Wait for script to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsKnownAdScript(GetDocument().GetExecutionContext(), kAdUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdUrl));

  // Walk through the DOM to get the image element.
  Element* doc_element = GetDocument().documentElement();
  Element* body_element = Traversal<Element>::LastChild(*doc_element);
  HTMLImageElement* image_element =
      Traversal<HTMLImageElement>::FirstChild(*body_element);

  // When async stacks are monitored, we should also tag the
  // HTMLImageElement as ad-related.
  ASSERT_TRUE(image_element);
  EXPECT_TRUE(image_element->IsAdRelated());
}

// Frame loaded by ad script is considered created by ad script.
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

  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());

  // Verify that the new frame is considered created by ad script then set it
  // as an ad subframe. This emulates the SubresourceFilterAgent's tagging.
  EXPECT_TRUE(child_frame->IsFrameCreatedByAdScript());
  SetIsAdFrame(child_frame);

  vanilla_page.Complete("<img src=vanilla_img.jpg></img>");
  vanilla_image.Complete("");

  EXPECT_TRUE(IsKnownAdScript(GetDocument().GetExecutionContext(), kAdUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdUrl));
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
  EXPECT_TRUE(
      IsKnownAdScript(local_subframe->GetDocument()->GetExecutionContext(),
                      String("https://example.com/library.js")));

  EXPECT_FALSE(IsKnownAdScript(GetDocument().GetExecutionContext(),
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

  auto* subframe =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(subframe->IsFrameCreatedByAdScript());
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

  auto* subframe =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(subframe->IsFrameCreatedByAdScript());
}

// This test class allows easy running of tests that only differ by whether
// one resource (or a set of resources) is vanilla or an ad.
class AdTrackerVanillaOrAdSimTest : public AdTrackerSimTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  bool IsAdRun() { return GetParam(); }

  String FlipURLOnAdRun(String vanilla_url) {
    return IsAdRun() ? vanilla_url + "?ad=true" : vanilla_url;
  }
};

TEST_P(AdTrackerVanillaOrAdSimTest, VanillaExternalStylesheetLoadsResources) {
  String vanilla_stylesheet_url = "https://example.com/style.css";
  String font_url = FlipURLOnAdRun("https://example.com/font.woff2");
  String image_url = FlipURLOnAdRun("https://example.com/pixel.png");
  SimSubresourceRequest stylesheet(vanilla_stylesheet_url, "text/css");
  SimSubresourceRequest font(font_url, "font/woff2");
  SimSubresourceRequest image(image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(kPageWithVanillaExternalStylesheet);
  stylesheet.Complete(IsAdRun() ? kStylesheetWithAdResources
                                : kStylesheetWithVanillaResources);

  // Wait for stylesheet to fetch resources.
  ad_tracker_->WaitForSubresource(font_url);
  ad_tracker_->WaitForSubresource(image_url);

  font.Complete();
  image.Complete();

  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_stylesheet_url));
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(font_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(image_url), IsAdRun());
}

TEST_P(AdTrackerVanillaOrAdSimTest, AdExternalStylesheetLoadsResources) {
  String ad_stylesheet_url = "https://example.com/style.css?ad=true";
  String font_url = FlipURLOnAdRun("https://example.com/font.woff2");
  String image_url = FlipURLOnAdRun("https://example.com/pixel.png");
  SimSubresourceRequest stylesheet(ad_stylesheet_url, "text/css");
  SimSubresourceRequest font(font_url, "font/woff2");
  SimSubresourceRequest image(image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(kPageWithAdExternalStylesheet);
  stylesheet.Complete(IsAdRun() ? kStylesheetWithAdResources
                                : kStylesheetWithVanillaResources);

  // Wait for stylesheet to fetch resources.
  ad_tracker_->WaitForSubresource(font_url);
  ad_tracker_->WaitForSubresource(image_url);

  font.Complete();
  image.Complete();

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_stylesheet_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(font_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(image_url));
}

TEST_P(AdTrackerVanillaOrAdSimTest, LinkRelStylesheetAddedByScript) {
  String script_url = FlipURLOnAdRun("https://example.com/script.js");
  String vanilla_stylesheet_url = "https://example.com/style.css";
  String vanilla_font_url = "https://example.com/font.woff2";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest script(script_url, "text/javascript");
  SimSubresourceRequest stylesheet(vanilla_stylesheet_url, "text/css");
  SimSubresourceRequest font(vanilla_font_url, "font/woff2");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(IsAdRun() ? kPageWithAdScript
                                     : kPageWithVanillaScript);
  script.Complete(R"SCRIPT(
    let link = document.createElement("link");
    link.rel = "stylesheet";
    link.href = "style.css";
    document.head.appendChild(link);
    )SCRIPT");

  // Wait for script to run.
  ad_tracker_->WaitForSubresource(vanilla_stylesheet_url);

  stylesheet.Complete(kStylesheetWithVanillaResources);

  // Wait for stylesheet to fetch resources.
  ad_tracker_->WaitForSubresource(vanilla_font_url);
  ad_tracker_->WaitForSubresource(vanilla_image_url);

  font.Complete();
  image.Complete();

  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(script_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_stylesheet_url),
            IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_font_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url),
            IsAdRun());
}

TEST_P(AdTrackerVanillaOrAdSimTest, ExternalStylesheetInFrame) {
  String vanilla_stylesheet_url = "https://example.com/style.css";
  String vanilla_font_url = "https://example.com/font.woff2";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimRequest frame("https://example.com/frame.html", "text/html");
  SimSubresourceRequest stylesheet(vanilla_stylesheet_url, "text/css");
  SimSubresourceRequest font(vanilla_font_url, "font/woff2");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(kPageWithFrame);
  if (IsAdRun()) {
    auto* subframe =
        To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
    SetIsAdFrame(subframe);
  }

  frame.Complete(kPageWithVanillaExternalStylesheet);
  stylesheet.Complete(kStylesheetWithVanillaResources);
  Compositor().BeginFrame();

  // Wait for stylesheet to fetch resources.
  ad_tracker_->WaitForSubresource(vanilla_font_url);
  ad_tracker_->WaitForSubresource(vanilla_image_url);

  font.Complete();
  image.Complete();

  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_stylesheet_url),
            IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_font_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url),
            IsAdRun());
}

// Note that we skip fonts as at rules aren't valid in inline CSS.
TEST_P(AdTrackerVanillaOrAdSimTest, InlineCSSSetByScript) {
  String script_url = FlipURLOnAdRun("https://example.com/script.js");
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest script(script_url, "text/javascript");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(IsAdRun() ? kPageWithAdScript
                                     : kPageWithVanillaScript);
  script.Complete(R"SCRIPT(
    let div = document.getElementsByClassName("test")[0];
    div.style = "background-image: url('pixel.png');";
    )SCRIPT");

  ad_tracker_->WaitForSubresource(vanilla_image_url);

  image.Complete();

  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(script_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url),
            IsAdRun());
}

TEST_F(AdTrackerSimTest, StyleTagInMainframe) {
  String vanilla_font_url = "https://example.com/font.woff2";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest font(vanilla_font_url, "font/woff2");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(kPageWithStyleTagLoadingVanillaResources);

  // Wait for stylesheet to fetch resources.
  ad_tracker_->WaitForSubresource(vanilla_font_url);
  ad_tracker_->WaitForSubresource(vanilla_image_url);

  font.Complete();
  image.Complete();

  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_font_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url));
}

// This verifies that style tag resources in ad frames are correctly tagged
// according to the heuristic that all requests from an ad frame should also be
// tagged as ads.
TEST_P(AdTrackerVanillaOrAdSimTest, StyleTagInSubframe) {
  String vanilla_font_url = "https://example.com/font.woff2";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimRequest frame("https://example.com/frame.html", "text/html");
  SimSubresourceRequest font(vanilla_font_url, "font/woff2");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(kPageWithFrame);
  if (IsAdRun()) {
    auto* subframe =
        To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
    SetIsAdFrame(subframe);
  }

  frame.Complete(kPageWithStyleTagLoadingVanillaResources);

  // Wait for stylesheet to fetch resources.
  ad_tracker_->WaitForSubresource(vanilla_font_url);
  ad_tracker_->WaitForSubresource(vanilla_image_url);

  font.Complete();
  image.Complete();

  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_font_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url),
            IsAdRun());
}

TEST_P(AdTrackerVanillaOrAdSimTest, StyleTagAddedByScript) {
  String script_url = FlipURLOnAdRun("https://example.com/script.js");
  String vanilla_font_url = "https://example.com/font.woff2";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest script(script_url, "text/javascript");
  SimSubresourceRequest font(vanilla_font_url, "font/woff2");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(IsAdRun() ? kPageWithAdScript
                                     : kPageWithVanillaScript);
  script.Complete(String::Format(
      R"SCRIPT(
        let style = document.createElement("style");
        let text = document.createTextNode(`%s`);
        style.appendChild(text);
        document.head.appendChild(style);
      )SCRIPT",
      kStylesheetWithVanillaResources));

  // Wait for stylesheet to fetch resources.
  ad_tracker_->WaitForSubresource(vanilla_font_url);
  ad_tracker_->WaitForSubresource(vanilla_image_url);

  font.Complete();
  image.Complete();

  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(script_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_font_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url),
            IsAdRun());
}

TEST_P(AdTrackerVanillaOrAdSimTest, VanillaImportInStylesheet) {
  String stylesheet_url = FlipURLOnAdRun("https://example.com/style.css");
  String vanilla_imported_stylesheet_url = "https://example.com/imported.css";
  String vanilla_font_url = "https://example.com/font.woff2";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest stylesheet(stylesheet_url, "text/css");
  SimSubresourceRequest imported_stylesheet(vanilla_imported_stylesheet_url,
                                            "text/css");
  SimSubresourceRequest font(vanilla_font_url, "font/woff2");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(IsAdRun() ? kPageWithAdExternalStylesheet
                                     : kPageWithVanillaExternalStylesheet);
  stylesheet.Complete(R"CSS(
    @import url(imported.css);
  )CSS");
  imported_stylesheet.Complete(kStylesheetWithVanillaResources);

  // Wait for stylesheets to fetch resources.
  ad_tracker_->WaitForSubresource(vanilla_font_url);
  ad_tracker_->WaitForSubresource(vanilla_image_url);

  font.Complete();
  image.Complete();

  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(stylesheet_url), IsAdRun());
  EXPECT_EQ(
      ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_imported_stylesheet_url),
      IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_font_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url),
            IsAdRun());
}

TEST_P(AdTrackerVanillaOrAdSimTest, AdImportInStylesheet) {
  String stylesheet_url = FlipURLOnAdRun("https://example.com/style.css");
  String ad_imported_stylesheet_url =
      "https://example.com/imported.css?ad=true";
  String vanilla_font_url = "https://example.com/font.woff2";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest stylesheet(stylesheet_url, "text/css");
  SimSubresourceRequest imported_stylesheet(ad_imported_stylesheet_url,
                                            "text/css");
  SimSubresourceRequest font(vanilla_font_url, "font/woff2");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(IsAdRun() ? kPageWithAdExternalStylesheet
                                     : kPageWithVanillaExternalStylesheet);
  stylesheet.Complete(R"CSS(
    @import url(imported.css?ad=true);
  )CSS");
  imported_stylesheet.Complete(kStylesheetWithVanillaResources);

  // Wait for stylesheets to fetch resources.
  ad_tracker_->WaitForSubresource(vanilla_font_url);
  ad_tracker_->WaitForSubresource(vanilla_image_url);

  font.Complete();
  image.Complete();

  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(stylesheet_url), IsAdRun());
  EXPECT_TRUE(
      ad_tracker_->RequestWithUrlTaggedAsAd(ad_imported_stylesheet_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_font_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url));
}

TEST_P(AdTrackerVanillaOrAdSimTest, ImageSetInStylesheet) {
  String stylesheet_url = FlipURLOnAdRun("https://example.com/style.css");
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest stylesheet(stylesheet_url, "text/css");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(IsAdRun() ? kPageWithAdExternalStylesheet
                                     : kPageWithVanillaExternalStylesheet);

  // The image with the lowest scale factor that is still larger than the
  // device's scale factor is used.
  stylesheet.Complete(R"CSS(
    .test {
      background-image: -webkit-image-set( url("pixel.png") 100x,
                                           url("too_high.png") 999x);
    }
  )CSS");

  // Wait for stylesheet to fetch resource.
  ad_tracker_->WaitForSubresource(vanilla_image_url);

  image.Complete();

  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(stylesheet_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url),
            IsAdRun());
}

TEST_P(AdTrackerVanillaOrAdSimTest, ConstructableCSSCreatedByScript) {
  String script_url = FlipURLOnAdRun("https://example.com/script.js");
  String vanilla_font_url = "https://example.com/font.woff2";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest script(script_url, "text/javascript");
  SimSubresourceRequest font(vanilla_font_url, "font/woff2");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(IsAdRun() ? kPageWithAdScript
                                     : kPageWithVanillaScript);
  script.Complete(R"SCRIPT(
    const sheet = new CSSStyleSheet();
    sheet.insertRule(`
      @font-face {
        font-family: "Vanilla";
        src: url("font.woff2") format("woff2");
      }`);
    sheet.insertRule(`
      .test {
        font-family: "Vanilla";
        background-image: url("pixel.png");
      }`);
    document.adoptedStyleSheets = [sheet];
  )SCRIPT");

  // Wait for stylesheet to fetch resources.
  ad_tracker_->WaitForSubresource(vanilla_font_url);
  ad_tracker_->WaitForSubresource(vanilla_image_url);

  font.Complete();
  image.Complete();

  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(script_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_font_url), IsAdRun());
  EXPECT_EQ(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url),
            IsAdRun());
}

// Vanilla resources loaded due to an ad's script's style recalculation
// shouldn't be tagged.
TEST_F(AdTrackerSimTest, StyleRecalcCausedByAdScript) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_stylesheet_url = "https://example.com/style.css";
  String vanilla_font_url = "https://example.com/font.woff2";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest script(ad_script_url, "text/javascript");
  SimSubresourceRequest stylesheet(vanilla_stylesheet_url, "text/css");
  SimSubresourceRequest font(vanilla_font_url, "font/woff2");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(R"HTML(
    <head><link rel="stylesheet" href="style.css">
        <script async src="script.js?ad=true"></script></head>
    <body><div>Test</div></body>
  )HTML");
  stylesheet.Complete(kStylesheetWithVanillaResources);

  Compositor().BeginFrame();
  base::RunLoop().RunUntilIdle();
  // @font-face rules have fetches set up for src descriptors when the font face
  // is initialized in FontFace::InitCSSFontFace(). The fetch is not actually
  // performed, but the AdTracker is notified.
  EXPECT_TRUE(ad_tracker_->UrlHasBeenRequested(vanilla_font_url));
  EXPECT_FALSE(ad_tracker_->UrlHasBeenRequested(vanilla_image_url));

  // We override these to ensure the ad script appears on top of the stack when
  // the requests are made.
  ad_tracker_->SetExecutionContext(GetDocument().GetExecutionContext());
  ad_tracker_->SetScriptAtTopOfStack(ad_script_url);

  script.Complete(R"SCRIPT(
    let div = document.getElementsByTagName("div")[0];
    div.className = "test";
  )SCRIPT");

  // Wait for stylesheets to fetch resources.
  ad_tracker_->WaitForSubresource(vanilla_font_url);
  ad_tracker_->WaitForSubresource(vanilla_image_url);

  font.Complete();
  image.Complete();

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_stylesheet_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_font_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url));
}

// A dynamically added script with no src is still tagged as an ad if created
// by an ad script.
TEST_F(AdTrackerSimTest, DynamicallyAddedScriptNoSrc_StillTagged) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
        <script src="script.js"></script></body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    let script = document.createElement("script");
    let text = document.createTextNode(
        "function getImage() { fetch('pixel.png'); }");
    script.appendChild(text);
    document.body.appendChild(script);
  )SCRIPT");

  // Fetch a resource using the function defined by dynamically added ad script.
  vanilla_script.Complete(R"SCRIPT(
    getImage();
  )SCRIPT");

  ad_tracker_->WaitForSubresource(vanilla_image_url);

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url));
}

// A dynamically added script with no src isn't tagged as an ad if not created
// by an ad script, even if it's later used by an ad script.
TEST_F(AdTrackerSimTest,
       DynamicallyAddedScriptNoSrc_NotTaggedBasedOnUseByAdScript) {
  String vanilla_script_url = "https://example.com/script.js";
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script2_url = "https://example.com/script2.js";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script2(vanilla_script2_url, "text/javascript");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js"></script>
        <script src="script.js?ad=true"></script>
        <script src="script2.js"></script></body>
  )HTML");

  vanilla_script.Complete(R"SCRIPT(
    let script = document.createElement("script");
    let text = document.createTextNode(
        "function doNothing() {} " +
        "function getImage() { fetch('pixel.png'); }");
    script.appendChild(text);
    document.body.appendChild(script);
  )SCRIPT");

  ad_script.Complete(R"SCRIPT(
    doNothing();
  )SCRIPT");

  vanilla_script2.Complete(R"SCRIPT(
    getImage();
  )SCRIPT");

  ad_tracker_->WaitForSubresource(vanilla_image_url);

  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script2_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url));
}

TEST_F(AdTrackerSimTest, VanillaModuleScript_ResourceNotTagged) {
  String vanilla_script_url = "https://example.com/script.js";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(R"HTML(
    <head><script type="module" src="script.js"></script></head>
    <body><div>Test</div></body>
  )HTML");

  vanilla_script.Complete(R"SCRIPT(
    fetch('pixel.png');
  )SCRIPT");

  ad_tracker_->WaitForSubresource(vanilla_image_url);

  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url));
}

TEST_F(AdTrackerSimTest, AdModuleScript_ResourceTagged) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(R"HTML(
    <head><script type="module" src="script.js?ad=true"></script></head>
    <body><div>Test</div></body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    fetch('pixel.png');
  )SCRIPT");

  ad_tracker_->WaitForSubresource(vanilla_image_url);

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url));
}

// A resource fetched with ad script at top of stack is still tagged as an ad
// when the ad script defines a sourceURL.
TEST_F(AdTrackerSimTest, AdScriptWithSourceURLAtTopOfStack_StillTagged) {
  String vanilla_script_url = "https://example.com/script.js";
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(R"HTML(
    <head><script src="script.js?ad=true"></script>
          <script src="script.js"></script></head>
    <body><div>Test</div></body>
  )HTML");

  // We don't directly fetch in ad script as we aim to test ScriptAtTopOfStack()
  // not WillExecuteScript().
  ad_script.Complete(R"SCRIPT(
    function getImage() { fetch('pixel.png'); }
    //# sourceURL=source.js
  )SCRIPT");

  vanilla_script.Complete(R"SCRIPT(
    getImage();
  )SCRIPT");

  ad_tracker_->WaitForSubresource(vanilla_image_url);

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url));
}

// A dynamically added script with no src is still tagged as an ad if created
// by an ad script even if it defines a sourceURL.
TEST_F(AdTrackerSimTest, InlineAdScriptWithSourceURLAtTopOfStack_StillTagged) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  String vanilla_image_url = "https://example.com/pixel.png";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest image(vanilla_image_url, "image/png");

  ad_tracker_->SetAdSuffix("ad=true");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
        <script src="script.js"></script></body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    let script = document.createElement("script");
    let text = document.createTextNode(
        "function getImage() { fetch('pixel.png'); } \n"
        + "//# sourceURL=source.js");
    script.appendChild(text);
    document.body.appendChild(script);
  )SCRIPT");

  // Fetch a resource using the function defined by dynamically added ad script.
  vanilla_script.Complete(R"SCRIPT(
    getImage();
  )SCRIPT");

  ad_tracker_->WaitForSubresource(vanilla_image_url);

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_image_url));
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
  EXPECT_FALSE(GetDocument().GetFrame()->IsAdFrame());
}

INSTANTIATE_TEST_SUITE_P(All,
                         AdTrackerVanillaOrAdSimTest,
                         ::testing::Values(true, false));

}  // namespace blink
