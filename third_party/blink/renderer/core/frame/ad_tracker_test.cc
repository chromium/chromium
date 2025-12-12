// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/ad_tracker.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "components/subresource_filter/content/renderer/web_document_subresource_filter_impl.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
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

using AdScriptAncestry = AdTracker::AdScriptAncestry;

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

// Returns a new instance of `WebDocumentSubresourceFilterImpl`. The caller
// assumes ownership and is responsible for its proper destruction.
subresource_filter::WebDocumentSubresourceFilterImpl* CreateSubresourceFilter(
    scoped_refptr<const subresource_filter::MemoryMappedRuleset> ruleset) {
  subresource_filter::mojom::ActivationState activation_state(
      subresource_filter::mojom::ActivationLevel::kDryRun,
      subresource_filter::mojom::SubresourceFilterDisabledReason::kUnknown,
      /*filtering_disabled_for_document=*/false,
      /*generic_blocking_rules_disabled=*/false,
      /*measure_performance=*/false,
      /*enable_logging=*/false);

  return new subresource_filter::WebDocumentSubresourceFilterImpl(
      url::Origin::Create(GURL("https://example.com")), activation_state,
      ruleset,
      /*first_disallowed_load_callback=*/base::DoNothing());
}

class FixedSubresourceFilterWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  explicit FixedSubresourceFilterWebFrameClient(
      scoped_refptr<const subresource_filter::MemoryMappedRuleset> ruleset)
      : ruleset_(ruleset) {}

  void DidCommitNavigation(
      WebHistoryCommitType commit_type,
      bool should_reset_browser_interface_broker,
      const network::ParsedPermissionsPolicy& permissions_policy_header,
      const DocumentPolicyFeatureState& document_policy_header) override {
    Frame()->GetDocumentLoader()->SetSubresourceFilter(
        CreateSubresourceFilter(ruleset_));
  }

 private:
  scoped_refptr<const subresource_filter::MemoryMappedRuleset> ruleset_;
};

class TestAdTracker : public AdTracker {
 public:
  explicit TestAdTracker(LocalFrame* frame) : AdTracker(frame) {}
  ~TestAdTracker() override {}

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

  // Test-only override for IsAdScriptInStack. Calls the base implementation
  // and caches its results for later verification in tests.
  bool IsAdScriptInStack(StackType stack_type,
                         MonkeyPatchableApi ignore_monkey_patch,
                         AdScriptAncestry* out_ad_script_ancestry) override {
    bool result = AdTracker::IsAdScriptInStack(stack_type, ignore_monkey_patch,
                                               out_ad_script_ancestry);

    if (sim_test_) {
      last_is_ad_script_in_stack_result_ = result;

      if (out_ad_script_ancestry) {
        last_ad_script_ancestry_ = *out_ad_script_ancestry;
      }
    }

    return result;
  }

  const AdScriptAncestry& last_ad_script_ancestry() const {
    return last_ad_script_ancestry_;
  }

  bool last_is_ad_script_in_stack_result() const {
    return last_is_ad_script_in_stack_result_;
  }

 protected:
  bool CalculateIfAdSubresource(
      ExecutionContext* execution_context,
      const KURL& request_url,
      ResourceType resource_type,
      const FetchInitiatorInfo& initiator_info,
      bool known_ad,
      bool scan_stack_for_ads,
      const subresource_filter::ScopedRule& rule) override {
    bool observed_result = AdTracker::CalculateIfAdSubresource(
        execution_context, request_url, resource_type, initiator_info, known_ad,
        scan_stack_for_ads, rule);

    String resource_url = request_url.GetString();
    is_ad_.insert(resource_url, observed_result);

    if (quit_closure_ && url_to_wait_for_ == resource_url) {
      std::move(quit_closure_).Run();
    }
    return observed_result;
  }

 private:
  HashMap<String, bool> is_ad_;
  HashMap<String, int> script_ids_;

  bool sim_test_ = false;

  bool last_is_ad_script_in_stack_result_ = false;
  AdScriptAncestry last_ad_script_ancestry_;

  base::OnceClosure quit_closure_;
  String url_to_wait_for_;
};

void SetIsAdFrame(LocalFrame* frame, bool created_by_ad_script = true) {
  DCHECK(frame);
  blink::FrameAdEvidence ad_evidence(frame->Parent() &&
                                     frame->Parent()->IsAdFrame());

  if (created_by_ad_script) {
    ad_evidence.set_created_by_ad_script(
        mojom::FrameCreationStackEvidence::kCreatedByAdScript);
  } else {
    ad_evidence.UpdateFilterListResult(
        mojom::FilterListResult::kMatchedBlockingRule);
  }

  ad_evidence.set_is_complete();
  frame->SetAdEvidence(ad_evidence);
}

}  // namespace

class AdTrackerSimTest : public SimTest {
 protected:
  AdTrackerSimTest() {
    // Build a subresource filter ruleset that will consider *ad_script.js and
    // *ad=true urls as ads.
    std::vector<url_pattern_index::proto::UrlRule> rules;
    rules.push_back(
        subresource_filter::testing::CreateSuffixRule("ad_script.js"));
    rules.push_back(subresource_filter::testing::CreateSuffixRule("ad=true"));

    subresource_filter::testing::TestRulesetPair test_ruleset_pair;
    subresource_filter::testing::TestRulesetCreator ruleset_creator;
    ruleset_creator.CreateRulesetWithRules(rules, &test_ruleset_pair);
    ruleset_ = subresource_filter::MemoryMappedRuleset::CreateAndInitialize(
        subresource_filter::testing::TestRuleset::Open(
            test_ruleset_pair.indexed));
  }

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

  std::unique_ptr<frame_test_helpers::TestWebFrameClient>
  CreateWebFrameClientForMainFrame() override {
    return std::make_unique<FixedSubresourceFilterWebFrameClient>(ruleset_);
  }

  bool IsKnownAdScript(ExecutionContext* execution_context, const String& url) {
    return ad_tracker_->IsKnownAdScript(execution_context, url);
  }

  std::unique_ptr<SimRequest> main_resource_;
  Persistent<TestAdTracker> ad_tracker_;
  scoped_refptr<const subresource_filter::MemoryMappedRuleset> ruleset_;
};

// Script loaded by ad script is tagged as ad.
TEST_F(AdTrackerSimTest, ScriptLoadedWhileExecutingAdScript) {
  const char kAdUrl[] = "https://example.com/ad_script.js";
  const char kVanillaUrl[] = "https://example.com/vanilla_script.js";
  SimSubresourceRequest ad_resource(kAdUrl, "text/javascript");
  SimSubresourceRequest vanilla_script(kVanillaUrl, "text/javascript");

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

TEST_F(AdTrackerSimTest, EventHandlerForPostMessageFromAdFrame_NoAdInStack) {
  const char kAdScriptUrl[] = "https://example.com/ad_script.js";
  SimSubresourceRequest ad_script(kAdScriptUrl, "text/javascript");
  const char kVanillaUrl[] = "https://example.com/vanilla_script.js";
  SimSubresourceRequest vanilla_script(kVanillaUrl, "text/javascript");

  SimSubresourceRequest image_resource("https://example.com/image.gif",
                                       "image/gif");

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
  gif.AppendSpan(base::span(kSmallGifData));

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

TEST_F(AdTrackerSimTest, PromiseResolveDetected) {
  const char kAdScriptUrl[] = "https://example.com/ad_script.js";
  const char kImageUrl[] = "https://example.com/image.gif";
  SimSubresourceRequest image(kImageUrl, "image/gif");
  SimSubresourceRequest ad_script(kAdScriptUrl, "text/javascript");

  main_resource_->Complete("<body></body><script src=ad_script.js></script>");

  ad_script.Complete(R"SCRIPT(
    Promise.resolve().then( () => {
      image = document.createElement("img");
      image.src = "image.gif";
      document.body.appendChild(image);
    });
    )SCRIPT");

  ad_tracker_->WaitForSubresource(kImageUrl);

  // Put the gif bytes in a Vector to avoid difficulty with
  // non null-terminated char*.
  Vector<char> gif;
  gif.AppendSpan(base::span(kSmallGifData));

  image.Complete(gif);

  EXPECT_TRUE(
      IsKnownAdScript(GetDocument().GetExecutionContext(), kAdScriptUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdScriptUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kImageUrl));
}

TEST_F(AdTrackerSimTest, PromiseRejectDetected) {
  const char kAdScriptUrl[] = "https://example.com/ad_script.js";
  const char kImageUrl[] = "https://example.com/image.gif";
  SimSubresourceRequest image(kImageUrl, "image/gif");
  SimSubresourceRequest ad_script(kAdScriptUrl, "text/javascript");

  main_resource_->Complete("<body></body><script src=ad_script.js></script>");

  ad_script.Complete(R"SCRIPT(
    Promise.reject().catch( () => {
      image = document.createElement("img");
      image.src = "image.gif";
      document.body.appendChild(image);
    });
    )SCRIPT");

  ad_tracker_->WaitForSubresource(kImageUrl);

  // Put the gif bytes in a Vector to avoid difficulty with
  // non null-terminated char*.
  Vector<char> gif;
  gif.AppendSpan(base::span(kSmallGifData));

  image.Complete(gif);

  EXPECT_TRUE(
      IsKnownAdScript(GetDocument().GetExecutionContext(), kAdScriptUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdScriptUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kImageUrl));
}

TEST_F(AdTrackerSimTest, PromiseChain) {
  const char kAdScriptUrl[] = "https://example.com/ad_script.js";
  const char kImageUrl[] = "https://example.com/image.gif";
  SimSubresourceRequest image(kImageUrl, "image/gif");
  SimSubresourceRequest ad_script(kAdScriptUrl, "text/javascript");

  main_resource_->Complete("<body></body><script src=ad_script.js></script>");

  ad_script.Complete(R"SCRIPT(
    Promise.resolve().then( () => {
      return Promise.resolve();
    }).then( () => {
      image = document.createElement("img");
      image.src = "image.gif";
      document.body.appendChild(image);
    });
    )SCRIPT");

  ad_tracker_->WaitForSubresource(kImageUrl);

  // Put the gif bytes in a Vector to avoid difficulty with
  // non null-terminated char*.
  Vector<char> gif;
  gif.AppendSpan(base::span(kSmallGifData));

  image.Complete(gif);

  EXPECT_TRUE(
      IsKnownAdScript(GetDocument().GetExecutionContext(), kAdScriptUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdScriptUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kImageUrl));
}

TEST_F(AdTrackerSimTest, BrokenPromiseScript) {
  const char kAdScriptUrl[] = "https://example.com/ad_script.js";
  const char kVanillaScriptUrl[] = "https://example.com/vanilla_script.js";

  const char kImageUrl[] = "https://example.com/image.gif";
  SimSubresourceRequest image(kImageUrl, "image/gif");
  SimSubresourceRequest ad_script(kAdScriptUrl, "text/javascript");
  SimSubresourceRequest vanilla_script(kVanillaScriptUrl, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body>
      <script src='ad_script.js'></script>
      <script src='vanilla_script.js'></script>
    </body>
  )HTML");

  // Run some ad script that fails in the midst of execution. This should
  // properly clean up in the AdTracker (e.g., the PromiseHook should send a
  // kAfter) so that the AdTracker doesn't think everything thereafter is ad
  // related.
  ad_script.Complete(R"SCRIPT(
    Promise.resolve().then( () => {
      asjhdklasjdh();  // this causes an exception
      return Promise.resolve();
    });
    )SCRIPT");

  vanilla_script.Complete(R"SCRIPT(
    image = document.createElement("img");
    image.src = "image.gif";
    document.body.appendChild(image);
    )SCRIPT");

  ad_tracker_->WaitForSubresource(kImageUrl);

  // Put the gif bytes in a Vector to avoid difficulty with
  // non null-terminated char*.
  Vector<char> gif;
  gif.AppendSpan(base::span(kSmallGifData));

  image.Complete(gif);

  EXPECT_TRUE(
      IsKnownAdScript(GetDocument().GetExecutionContext(), kAdScriptUrl));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(kAdScriptUrl));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(kVanillaScriptUrl));

  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(kImageUrl));
}

TEST_F(AdTrackerSimTest, VanillaPromiseNotDetected) {
  const char kVanillaScriptUrl[] = "https://example.com/vanilla_script.js";
  const char kImageUrl[] = "https://example.com/image.gif";
  SimSubresourceRequest image(kImageUrl, "image/gif");
  SimSubresourceRequest vanilla_script(kVanillaScriptUrl, "text/javascript");

  main_resource_->Complete(
      "<body></body><script src=vanilla_script.js></script>");

  vanilla_script.Complete(R"SCRIPT(
    Promise.resolve('Promise.resolve').then( () => {
      image = document.createElement("img");
      image.src = "image.gif";
      document.body.appendChild(image);
    });
    )SCRIPT");

  ad_tracker_->WaitForSubresource(kImageUrl);

  // Put the gif bytes in a Vector to avoid difficulty with
  // non null-terminated char*.
  Vector<char> gif;
  gif.AppendSpan(base::span(kSmallGifData));

  image.Complete(gif);

  EXPECT_FALSE(
      IsKnownAdScript(GetDocument().GetExecutionContext(), kVanillaScriptUrl));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(kVanillaScriptUrl));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(kImageUrl));
}

// Image loaded by ad script is tagged as ad.
TEST_F(AdTrackerSimTest, DataURLImageLoadedWhileExecutingAdScriptAsyncEnabled) {
  const char kAdUrl[] = "https://example.com/ad_script.js";
  SimSubresourceRequest ad_resource(kAdUrl, "text/javascript");

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
  base::RunLoop().RunUntilIdle();

  Frame* subframe = GetDocument().GetFrame()->Tree().FirstChild();
  auto* local_subframe = To<LocalFrame>(subframe);

  subresource_filter::testing::TestRulesetPair test_ruleset_pair;
  subresource_filter::testing::TestRulesetCreator ruleset_creator;
  ruleset_creator.CreateRulesetToDisallowURLsWithPathSuffix("library.js",
                                                            &test_ruleset_pair);
  scoped_refptr<const subresource_filter::MemoryMappedRuleset> new_ruleset =
      subresource_filter::MemoryMappedRuleset::CreateAndInitialize(
          subresource_filter::testing::TestRuleset::Open(
              test_ruleset_pair.indexed));

  local_subframe->GetDocument()->Loader()->SetSubresourceFilter(
      CreateSubresourceFilter(new_ruleset));

  // The library script is loaded for a second time, this time in the
  // subframe that treats 'library.js' as an ad suffix.
  SimSubresourceRequest library_resource_for_subframe(
      "https://example.com/library.js", "text/javascript");

  iframe_resource.Complete(R"HTML(
    <script src="library.js"></script>
    )HTML");
  library_resource_for_subframe.Complete("");

  // Verify that library.js is an ad script in the subframe's context but not
  // in the main frame's context.
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

// Tests that ad-script calling an inline script does not re-tag the inline
// script as an ad.
TEST_F(AdTrackerSimTest, InlineAdScriptOnlyTaggedWhenFirstRun) {
  String add_inline_url = "https://example.com/add_inline.js";
  String vanilla_script_url = "https://example.com/script.js";
  String ad_script_url = "https://example.com/script.js?ad=true";
  String script3_url = "https://example.com/script3.js";
  String script4_url = "https://example.com/script4.js";

  SimSubresourceRequest add_inline(add_inline_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest script3(script3_url, "text/javascript");
  SimSubresourceRequest script4(script4_url, "text/javascript");

  // Load the main frame, which loads three scripts.
  // 1. add_inline: creates some dynamically added inline script.
  // 2. ad_script: calls one of the inline functions which fetches a script.
  // 3. vanilla_script: calls the other inline function which fetches a script.
  // The result of (2) should be tagged as an ad but not (3).
  main_resource_->Complete(R"HTML(
    <body>
      <script src="add_inline.js"></script>
      // Ad script calls a function in the inline script.
      <script src="script.js?ad=true"></script>
      // Vanilla script calls a function in the inline script to fetch an image.
      <script src="script.js"></script>
      </body>
  )HTML");

  // Creates some inline script with 2 methods that can be called.
  add_inline.Complete(R"SCRIPT(
    let script = document.createElement("script");
    script.textContent = `
        function loadScript3() {
          let script = document.createElement("script");
          script.src = "script3.js";
          document.body.appendChild(script);
        }

        function loadScript4() {
          let script = document.createElement("script");
          script.src = "script4.js";
          document.body.appendChild(script);
        }`
    document.body.appendChild(script);
  )SCRIPT");

  // Ad script loads and calls one of the inline script's vanilla functions.
  // This should not retag the inline script as an ad script. The script is run
  // asynchronously to cause the inline script to be executed directly from
  // blink.
  ad_script.Complete("setTimeout(loadScript3, 0);");
  ad_tracker_->WaitForSubresource(script3_url);
  script3.Complete();

  // Vanilla code now calls the inline script's other function to fetch an
  // image, said image should not be tagged as an ad.
  vanilla_script.Complete("loadScript4();");
  base::RunLoop().RunUntilIdle();
  script4.Complete();

  ad_tracker_->WaitForSubresource(script4_url);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(script3_url));

  // This is what we're really testing.
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(script4_url));
}

// Tests that when the script at the top of the stack is an ad script,
// `IsAdScriptInStack` correctly identifies it and returns the expected
// `AdScriptIdentifier`.
TEST_F(AdTrackerSimTest, AdScriptAncestry_AdScriptAtTopOfStack) {
  String vanilla_script_url = "https://example.com/script.js";
  String ad_script_url = "https://example.com/script.js?ad=true";
  String ad_document_url = "https://example.com/ad_document.html";

  // Load an ad script and a vanilla script. The vanilla script calls a
  // function on the ad script which creates an ad iframe. The ad script is at
  // top of stack when it creates the frame and IsAdScriptInStack should return
  // the script id, verify that they look right.
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimRequest ad_document(ad_document_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    function createIframe() {
      frame = document.createElement("iframe");
      frame.src = "ad_document.html";
      document.body.appendChild(frame);
    }
  )SCRIPT");

  vanilla_script.Complete(R"SCRIPT(
    createIframe();
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // Verify frame was tagged as an ad.
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame->IsFrameCreatedByAdScript());

  // Verify that IsAdScriptInStack() returned the right script information.
  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  EXPECT_GT(ad_tracker_->last_ad_script_ancestry().ancestry_chain[0].id, 0);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad=true|");

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));

  // Clean up for SimTest expectations.
  ad_document.Complete("<body></body>");
}

// Tests that when the script at the top of the *async* stack is an ad script,
// `IsAdScriptInStack` correctly identifies it (via the bottommost async ad
// script) and returns the expected `AdScriptIdentifier`.
TEST_F(AdTrackerSimTest, AdScriptAncestry_AdScriptAtTopOfAsyncStack) {
  String vanilla_script_url = "https://example.com/script.js";
  String ad_script_url = "https://example.com/script.js?ad=true";
  String ad_document_url = "https://example.com/ad_document.html";

  // Load an ad script and a vanilla script. The vanilla script calls a
  // function on the ad script which asynchronously calls a function on the
  // vanilla script to create an ad iframe. The ad script is at top of *async*
  // stack when it creates the frame and IsAdScriptInStack should return
  // the script id, verify that they look right.
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimRequest ad_document(ad_document_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    function createIframeAsync() {
      setTimeout(() => {
        createIframe();
      });
    }
  )SCRIPT");

  vanilla_script.Complete(R"SCRIPT(
    function createIframe() {
      frame = document.createElement("iframe");
      frame.src = "ad_document.html";
      document.body.appendChild(frame);
    }

    createIframeAsync();
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // Verify frame was tagged as an ad.
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame->IsFrameCreatedByAdScript());

  // Verify that IsAdScriptInStack() returned the right script information.
  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  EXPECT_GT(ad_tracker_->last_ad_script_ancestry().ancestry_chain[0].id, 0);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad=true|");

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));

  // Clean up for SimTest expectations.
  ad_document.Complete("<body></body>");
}

// Tests a scenario where a non-subresource-filter-flagged script redirects to a
// subresource-filter-flagged URL, and this script creates an iframe.
//
// This test specifically verifies the *current* behavior affected by
// crbug.com/417756984. Due to this bug, the ad-tagging decision for the
// iframe is incorrectly based on the script's *initial*
// non-subresource-filter-flagged URL. Consequently, this test expects the
// iframe to NOT be ad-tagged.
//
// Once the bug is fixed, the iframe should be ad-tagged, and its ad script
// ancestry should contain one script.
TEST_F(
    AdTrackerSimTest,
    AdScriptAncestry_ScriptRedirectedFromNonFilterlistedUrlToFilterlistedUrl) {
  String ad_script_url = "https://example.com/script.js?ad=true";

  SimRequest::Params params;
  params.redirect_url = ad_script_url;

  String redirect_from_script_url =
      "https://example.com/redirect-from-script.js";
  String ad_document_url = "https://example.com/ad_document.html";

  // Scenario:
  // 1. A script (redirect_from_script_url) is loaded directly in the main
  //    frame.
  // 2. This script gets redirected to a subresource-filter-flagged URL
  //    (ad_script_url).
  // 3. The script (ad_script_url) then creates an iframe (child_frame).
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest redirect_from_script(redirect_from_script_url,
                                             "text/javascript", params);

  SimRequest ad_document(ad_document_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="https://example.com/redirect-from-script.js"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    const iframe = document.createElement('iframe');
    iframe.src = 'ad_document.html';
    document.body.appendChild(iframe);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // Current behavior (due to crbug.com/417756984):
  // The frame is NOT ad-tagged, as the ad-tagging decision is incorrectly based
  // on the creation script's initial URL.
  //
  // Expected behavior (post-fix):
  // The frame is ad-tagged, as its creation script's final URL matches the
  // filterlist.
  //
  // TODO: Update the expectations once the bug is fixed.
  // EXPECT_TRUE(child_frame->IsFrameCreatedByAdScript());
  // EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(),
  //           1u);
  // EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry().root_script_filterlist_rule.ToString())),
  //           "ad=true|");
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_FALSE(child_frame->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 0u);
  EXPECT_FALSE(ad_tracker_->last_ad_script_ancestry()
                   .root_script_filterlist_rule.IsValid());

  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(redirect_from_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));

  // Clean up for SimTest expectations.
  ad_document.Complete("<body></body>");
}

// Tests a scenario where a subresource-filter-flagged script redirects to a
// non-subresource-filter-flagged URL, and then a new script with the same
// non-filterlisted URL is loaded and creates an iframe.

// This test expects the iframe to be ad-tagged but has no associated filter
// rule.
TEST_F(
    AdTrackerSimTest,
    AdScriptAncestry_RedirectedNonFilterlistedUrlEncounteredAgainAsInitialUrl) {
  String vanilla_script_url = "https://example.com/vanilla-script.js";
  String trigger_script_url = "https://example.com/trigger-script.js";

  SimRequest::Params params;
  params.redirect_url = vanilla_script_url;

  String redirect_from_script_url =
      "https://example.com/redirect-from-script.js?ad=true";
  String ad_document_url = "https://example.com/ad_document.html";

  // Scenario:
  // 1. An ad script (redirect_from_script_url) is loaded directly in the main
  //    frame.
  // 2. This script gets redirected to a non-subresource-filter-flagged URL
  //    (vanilla_script_url).
  // 3. The non-filterlisted script (vanilla_script_url) is loaded again in the
  //    main frame, this time directly without any redirect.
  // 4. The script (vanilla_script_url) then creates an iframe (child_frame).
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest trigger_script(trigger_script_url, "text/javascript");
  SimSubresourceRequest redirect_from_script(redirect_from_script_url,
                                             "text/javascript", params);

  SimRequest ad_document(ad_document_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="https://example.com/redirect-from-script.js?ad=true"></script>
      <script src="https://example.com/trigger-script.js"></script>
    </body>
  )HTML");

  vanilla_script.Complete(R"SCRIPT(
    // Empty
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  SimSubresourceRequest vanilla_script_second_load(vanilla_script_url,
                                                   "text/javascript");

  trigger_script.Complete(R"SCRIPT(
    let script = document.createElement("script");
    script.src = 'vanilla-script.js';
    document.body.appendChild(script);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  vanilla_script_second_load.Complete(R"SCRIPT(
    let frame = document.createElement("iframe");
    frame.src = "ad_document.html";
    document.body.appendChild(frame);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // The frame is ad-tagged. Its ad script ancestry contains one script but no
  // no filterlist rule. This is because the script URL was first encountered as
  // a non-filterlisted URL redirected from a filterlisted URL.
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  EXPECT_FALSE(ad_tracker_->last_ad_script_ancestry()
                   .root_script_filterlist_rule.IsValid());

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(redirect_from_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(trigger_script_url));

  // Clean up for SimTest expectations.
  ad_document.Complete("<body></body>");
}

// Tests a scenario where a subresource-filter-flagged script redirects to a
// different subresource-filter-flagged URL, and this script creates an iframe.
//
// The iframe should be ad-tagged. Its ad script ancestry should contain one
// script, with the filterlist rule attributed to the initial URL.
TEST_F(AdTrackerSimTest,
       AdScriptAncestry_ScriptRedirectedToDifferentFilterlistedUrl) {
  String ad_script_url = "https://example.com/script.js?ad=true";

  SimRequest::Params params;
  params.redirect_url = ad_script_url;

  String redirect_from_ad_script_url =
      "https://example.com/redirect_from_ad_script.js";
  String ad_document_url = "https://example.com/ad_document.html";

  // Scenario:
  // 1. A subresource-filter-flagged script (redirect_from_ad_script_url) is
  //    loaded directly in the main frame.
  // 2. This script gets redirected to a different subresource-filter-flagged
  //    URL (ad_script_url).
  // 3. The script (ad_script_url) then creates an iframe (child_frame).
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest redirect_from_ad_script(redirect_from_ad_script_url,
                                                "text/javascript", params);

  SimRequest ad_document(ad_document_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="https://example.com/redirect_from_ad_script.js"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    const iframe = document.createElement('iframe');
    iframe.src = 'ad_document.html';
    document.body.appendChild(iframe);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // The frame is ad-tagged. Its ad script ancestry contains one script, with
  // the filterlist rule attributed to the initial URL.
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad_script.js|");

  EXPECT_TRUE(
      ad_tracker_->RequestWithUrlTaggedAsAd(redirect_from_ad_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));

  // Clean up for SimTest expectations.
  ad_document.Complete("<body></body>");
}

// Tests that `IsAdScriptInStack` returns the correct ad script ancestry when
// the final ad frame is created through script execution originating from an
// initial subresource-filter-flagged ad script. The stack ad script for this
// iframe is tagged because the initial ad script is present at the bottom of
// the creation stack.
TEST_F(AdTrackerSimTest,
       AdScriptAncestry_TransitiveScriptTaggedDueToBottomOfStackMatch) {
  String vanilla_script_url = "https://example.com/vanilla-script.js";
  String ad_script_url = "https://example.com/script.js?ad=true";
  String ad_document1_url = "https://example.com/ad_document1.html";
  String ad_document2_url = "https://example.com/ad_document2.html";

  // Scenario:
  // 1. An ad script (ad_script_url) is loaded directly in the main frame.
  // 2. This ad script creates an iframe (child_frame1).
  // 3. The ad script also loads a vanilla script (vanilla_script_url).
  // 4. The vanilla script (vanilla_script_url) creates another iframe
  //    (child_frame2).
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");

  SimRequest ad_document1(ad_document1_url, "text/html");
  SimRequest ad_document2(ad_document2_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="script.js?ad=true"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    const iframe = document.createElement('iframe');
    iframe.src = 'ad_document1.html';
    document.body.appendChild(iframe);

    const script = document.createElement('script');
    script.src = 'vanilla-script.js';
    document.body.appendChild(script);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame1, there is one ad script in the ancestry (the initiating
  // subresource-filter-flagged script).
  auto* child_frame1 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame1->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  auto frame1_stack_ad_script =
      ad_tracker_->last_ad_script_ancestry().ancestry_chain[0];

  vanilla_script.Complete(R"SCRIPT(
    const iframe2 = document.createElement('iframe');
    iframe2.src = 'ad_document2.html';
    document.body.appendChild(iframe2);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame2, there are two ad scripts in the ancestry. The source ad
  // script should match the one captured during the creation of child_frame1.
  auto* child_frame2 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(/*index=*/1));
  EXPECT_TRUE(child_frame2->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 2u);
  EXPECT_NE(ad_tracker_->last_ad_script_ancestry().ancestry_chain[0],
            ad_tracker_->last_ad_script_ancestry().ancestry_chain[1]);
  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain[1],
            frame1_stack_ad_script);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad=true|");

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));

  // Clean up for SimTest expectations.
  ad_document1.Complete("<body></body>");
  ad_document2.Complete("<body></body>");
}

// Tests that `IsAdScriptInStack` returns the correct ad script ancestry when
// the final ad frame is created through script execution originating from an
// initial subresource-filter-flagged ad script. The stack ad script for this
// iframe is tagged because the initial ad script is present at the top of the
// creation stack.
TEST_F(AdTrackerSimTest,
       AdScriptAncestry_TransitiveScriptTaggedDueToTopOfStackMatch) {
  String vanilla_script_url = "https://example.com/vanilla-script.js";
  String trigger_script_url = "https://example.com/trigger-script.js";
  String ad_script_url = "https://example.com/script.js?ad=true";
  String ad_document1_url = "https://example.com/ad_document1.html";
  String ad_document2_url = "https://example.com/ad_document2.html";

  // Scenario:
  // 1. An ad script (ad_script_url) is loaded directly in the main frame.
  // 2. This ad script creates an iframe (child_frame1).
  // 3. The ad script also defines a function `createScript()`.
  // 4. A trigger script (trigger_script_url) is loaded directly in the main
  //    frame, which invokes `createScript()` to create a vanilla script
  //    (vanilla_script_url).
  // 5. The vanilla script (vanilla_script_url) creates another iframe
  //    (child_frame2).
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest trigger_script(trigger_script_url, "text/javascript");
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");

  SimRequest ad_document1(ad_document1_url, "text/html");
  SimRequest ad_document2(ad_document2_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="script.js?ad=true"></script>
      <script src="trigger-script.js"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    const iframe = document.createElement('iframe');
    iframe.src = 'ad_document1.html';
    document.body.appendChild(iframe);

    function createScript() {
      const script = document.createElement('script');
      script.src = 'vanilla-script.js';
      document.body.appendChild(script);
    }
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame1, there is one ad script in the ancestry (the initiating
  // subresource-filter-flagged script).
  auto* child_frame1 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame1->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  auto frame1_stack_ad_script =
      ad_tracker_->last_ad_script_ancestry().ancestry_chain[0];

  trigger_script.Complete(R"SCRIPT(
    createScript();
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  vanilla_script.Complete(R"SCRIPT(
    const iframe2 = document.createElement('iframe');
    iframe2.src = 'ad_document2.html';
    document.body.appendChild(iframe2);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame2, there are two ad scripts in the ancestry. The source ad
  // script should match the one captured during the creation of child_frame1.
  auto* child_frame2 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(/*index=*/1));
  EXPECT_TRUE(child_frame2->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 2u);
  EXPECT_NE(ad_tracker_->last_ad_script_ancestry().ancestry_chain[0],
            ad_tracker_->last_ad_script_ancestry().ancestry_chain[1]);
  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain[1],
            frame1_stack_ad_script);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad=true|");

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));

  // Clean up for SimTest expectations.
  ad_document1.Complete("<body></body>");
  ad_document2.Complete("<body></body>");
}

TEST_F(AdTrackerSimTest, SelectivePermissionsInterventionOn) {
  ScopedSelectivePermissionsInterventionForTest feature(true);

  String ad_script_url = "https://example.com/script.js?ad=true";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  main_resource_->Complete(R"HTML(
    <body>
      <script src="script.js?ad=true"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
     navigator.geolocation.getCurrentPosition(() => {}, () => {console.log("Failed")});
  )SCRIPT");
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return ConsoleMessages().size() == 3; }));
  EXPECT_TRUE(ConsoleMessages()[0].StartsWith(
      "Blocked call to geolocation because ad-script"));
  EXPECT_TRUE(ConsoleMessages()[1].StartsWith(
      "Permissions policy violation: Geolocation"));
  EXPECT_EQ("Failed", ConsoleMessages()[2]);
}

TEST_F(AdTrackerSimTest, SelectivePermissionsInterventionOff) {
  ScopedSelectivePermissionsInterventionForTest feature(false);

  String ad_script_url = "https://example.com/script.js?ad=true";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  main_resource_->Complete(R"HTML(
    <body>
      <script src="script.js?ad=true"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    navigator.geolocation.getCurrentPosition(() => {console.log("Success")}, () => {console.log("Failed")});
  )SCRIPT");
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return ConsoleMessages().size() == 1; }));

  // It still fails in this environment, but not because of permission policy.
  EXPECT_EQ("Failed", ConsoleMessages()[0]);
}

// Tests that `IsAdScriptInStack` returns the correct ad script ancestry when
// the final ad frame is created through inline script execution originating
// from an initial subresource-filter-flagged ad script.
TEST_F(AdTrackerSimTest, AdScriptAncestry_TransitiveInlineScript) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String trigger_script_url = "https://example.com/trigger-script.js";
  String ad_document1_url = "https://example.com/ad_document1.html";
  String ad_document2_url = "https://example.com/ad_document2.html";

  // Scenario:
  // 1. An ad script (ad_script_url) is loaded directly in the main frame.
  // 2. This ad script creates an iframe (child_frame1).
  // 3. The ad script also loads a inline script that defines a function
  //    (createIframe()) to create an ad iframe.
  // 4. A trigger script (trigger_script_url) is loaded directly in the main
  //    frame, which invokes createIframe().
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest trigger_script(trigger_script_url, "text/javascript");

  SimRequest ad_document1(ad_document1_url, "text/html");
  SimRequest ad_document2(ad_document2_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="script.js?ad=true"></script>
      <script src="trigger-script.js"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    const iframe = document.createElement('iframe');
    iframe.src = 'ad_document1.html';
    document.body.appendChild(iframe);

    const inlineScript = `
      function createIframe() {
        const iframe2 = document.createElement('iframe');
        iframe2.src = 'ad_document2.html';
        document.body.appendChild(iframe2);
      }
    `;

    const script = document.createElement('script');
    script.textContent = inlineScript;
    document.body.appendChild(script);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame1, there is one ad script in the ancestry (the initiating
  // subresource-filter-flagged script).
  auto* child_frame1 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame1->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  auto frame1_stack_ad_script =
      ad_tracker_->last_ad_script_ancestry().ancestry_chain[0];

  trigger_script.Complete(R"SCRIPT(
    createIframe();
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame2, there are two ad scripts in the ancestry. The source ad
  // script should match the one captured during the creation of child_frame1.
  auto* child_frame2 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(/*index=*/1));
  EXPECT_TRUE(child_frame2->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 2u);
  EXPECT_NE(ad_tracker_->last_ad_script_ancestry().ancestry_chain[0],
            ad_tracker_->last_ad_script_ancestry().ancestry_chain[1]);
  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain[1],
            frame1_stack_ad_script);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad=true|");

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(trigger_script_url));

  // Clean up for SimTest expectations.
  ad_document1.Complete("<body></body>");
  ad_document2.Complete("<body></body>");
}

// Tests that an inlined module script created by an ad script is correctly
// identified as ad related.
TEST_F(AdTrackerSimTest, AsyncInlineScript) {
  String ad_script_url = "https://example.com/ad_script.js?ad=true";
  String ad_document_url = "https://example.com/ad_document.html";

  // Scenario:
  // 1. An ad script (ad_script_url) is loaded directly in the main frame.
  // 2. This ad script creates a inline module script.
  // 3. The inline module script loads asynchronously, but should still
  //    be detected as an ad.
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimRequest ad_document(ad_document_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="ad_script.js?ad=true"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    const script = document.createElement('script');
    script.type = "module";
    script.innerText =  `
          const iframe = document.createElement('iframe');
          iframe.src = 'ad_document.html';
          document.body.appendChild(iframe);
    `;
    document.body.appendChild(script);
  )SCRIPT");

  // Wait for the document load.
  ad_document.Complete("<html>Hello world!</html>");

  // The frame should be ad-tagged.
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_TRUE(child_frame->IsFrameCreatedByAdScript());
}

// Tests that `IsAdScriptInStack` returns the correct ad script ancestry when
// the final ad frame is created through multiple levels of asynchronous script
// execution originating from an initial subresource-filter-flagged ad script.
TEST_F(AdTrackerSimTest,
       AdScriptAncestry_TransitiveScript_MultipleLevelsAndAsync) {
  String vanilla_script_url = "https://example.com/vanilla-script.js";
  String vanilla_script2_url = "https://example.com/vanilla-script2.js";
  String ad_script_url = "https://example.com/ad_script.js";
  String ad_script2_url = "https://example.com/script2.js?ad=true";
  String ad_document1_url = "https://example.com/ad_document1.html";
  String ad_document2_url = "https://example.com/ad_document2.html";
  String ad_document3_url = "https://example.com/ad_document3.html";
  String ad_document4_url = "https://example.com/ad_document4.html";

  // Scenario:
  // 1. An ad script (ad_script_url) is loaded directly in the main frame.
  // 2. This ad script creates an iframe (child_frame1), and asynchronously
  //    loads an ad script (ad_script2_url).
  // 3. The new ad script creates an iframe (child_frame2), and asynchronously
  //    loads a vanilla script (vanilla_script_url).
  // 4. The vanilla script creates an iframe (child_frame3), and asynchronously
  //    loads another vanilla script (vanilla_script2_url).
  // 5. The new vanilla script asynchronously creates another iframe
  //    (child_frame4).
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script2(vanilla_script2_url, "text/javascript");
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest ad_script2(ad_script2_url, "text/javascript");

  SimRequest ad_document1(ad_document1_url, "text/html");
  SimRequest ad_document2(ad_document2_url, "text/html");
  SimRequest ad_document3(ad_document3_url, "text/html");
  SimRequest ad_document4(ad_document4_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="ad_script.js"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    setTimeout(() => {
      const script = document.createElement('script');
      script.src = 'script2.js?ad=true';
      document.body.appendChild(script);
    });

    const iframe = document.createElement('iframe');
    iframe.src = 'ad_document1.html';
    document.body.appendChild(iframe);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame1, there is one ad script in the ancestry (script.js).
  auto* child_frame1 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame1->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad_script.js|");
  auto frame1_stack_ad_script =
      ad_tracker_->last_ad_script_ancestry().ancestry_chain[0];

  ad_script2.Complete(R"SCRIPT(
    setTimeout(() => {
      const script = document.createElement('script');
      script.src = 'vanilla-script.js';
      document.body.appendChild(script);
    });

    const iframe2 = document.createElement('iframe');
    iframe2.src = 'ad_document2.html';
    document.body.appendChild(iframe2);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame2, there is one ad script in the ancestry (script2.js).
  auto* child_frame2 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(/*index=*/1));
  EXPECT_TRUE(child_frame2->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad=true|");
  auto frame2_stack_ad_script =
      ad_tracker_->last_ad_script_ancestry().ancestry_chain[0];
  EXPECT_NE(frame1_stack_ad_script, frame2_stack_ad_script);

  vanilla_script.Complete(R"SCRIPT(
    setTimeout(() => {
      const script = document.createElement('script');
      script.src = 'vanilla-script2.js';
      document.body.appendChild(script);
    });

    const iframe3 = document.createElement('iframe');
    iframe3.src = 'ad_document3.html';
    document.body.appendChild(iframe3);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame3, there are two ad scripts in the ancestry. The source ad
  // script should match the one captured during the creation of child_frame2
  // (script2.js). This is the nearest subresource-filter-flagged script in
  // vanilla-script.js's creation ancestry.
  auto* child_frame3 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(/*index=*/2));
  EXPECT_TRUE(child_frame3->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 2u);
  EXPECT_NE(ad_tracker_->last_ad_script_ancestry().ancestry_chain[0],
            ad_tracker_->last_ad_script_ancestry().ancestry_chain[1]);
  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain[1],
            frame2_stack_ad_script);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad=true|");
  auto frame3_stack_ad_script =
      ad_tracker_->last_ad_script_ancestry().ancestry_chain[0];

  vanilla_script2.Complete(R"SCRIPT(
    setTimeout(() => {
      const iframe4 = document.createElement('iframe');
      iframe4.src = 'ad_document4.html';
      document.body.appendChild(iframe4);
    });
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame4, there are three ad scripts in the ancestry:
  // - The source ad script should match the one captured during the creation of
  //   child_frame2 (script2.js). This is the nearest
  //   subresource-filter-flagged script in vanilla-script.js's creation
  //   ancestry.
  // - The next script down the ancestry should match the one captured during
  //   the creation of child_frame3 (vanilla-script.js). This is the direct
  //   ancestor script of vanilla-script2.js.
  auto* child_frame4 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(/*index=*/3));
  EXPECT_TRUE(child_frame4->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 3u);
  EXPECT_NE(ad_tracker_->last_ad_script_ancestry().ancestry_chain[0],
            ad_tracker_->last_ad_script_ancestry().ancestry_chain[1]);
  EXPECT_NE(ad_tracker_->last_ad_script_ancestry().ancestry_chain[0],
            ad_tracker_->last_ad_script_ancestry().ancestry_chain[2]);
  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain[1],
            frame3_stack_ad_script);
  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain[2],
            frame2_stack_ad_script);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad=true|");

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script2_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script2_url));

  // Clean up for SimTest expectations.
  ad_document1.Complete("<body></body>");
  ad_document2.Complete("<body></body>");
  ad_document3.Complete("<body></body>");
  ad_document4.Complete("<body></body>");
}

// Tests a scenario where a transitively added, subresource-filter-flagged
// script redirects to a non-subresource-filter-flagged URL, and this script
// creates an iframe. No further ancestor attribution is required, because the
// resource request itself made the URL ad-tagged. `IsAdScriptInStack` should
// return just one script.
TEST_F(
    AdTrackerSimTest,
    AdScriptAncestry_TransitiveScriptRedirectedFromFilterlistedUrlToNonFilterlistedUrl) {
  String vanilla_script_url = "https://example.com/vanilla-script.js";

  SimRequest::Params params;
  params.redirect_url = vanilla_script_url;

  String redirect_from_script_url =
      "https://example.com/redirect-from-script.js?ad=true";
  String ad_script_url = "https://example.com/ad_script.js";
  String ad_document_url = "https://example.com/ad_document.html";

  // Scenario:
  // 1. An ad script (ad_script_url) is loaded directly in the main frame.
  // 2. This ad script loads another ad script (redirect_from_script_url), which
  //    gets redirected to a non-subresource-filter-flagged URL
  //    (vanilla_script_url).
  // 3. The vanilla script (vanilla_script_url) creates an iframe (child_frame).
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest redirect_from_script(redirect_from_script_url,
                                             "text/javascript", params);

  SimRequest ad_document(ad_document_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="ad_script.js"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    const script = document.createElement('script');
    script.src = 'redirect-from-script.js?ad=true';
    document.body.appendChild(script);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  vanilla_script.Complete(R"SCRIPT(
    const iframe = document.createElement('iframe');
    iframe.src = 'ad_document.html';
    document.body.appendChild(iframe);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // There is one ad script in the ancestry. The resource request's initial
  // ad-tagged status propagates to the final URL after the redirect, so the
  // script does not require further ancestor attribution.
  //
  // Note: due to crbug.com/417756984, the real reason for this 'one script'
  // outcome is because the ancestor attribution decision is erroneously
  // dictated only by the script's initial, filterlisted URL. Coincidentally,
  // this bug's outcome aligns with the intended behavior for this specific
  // scenario.
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad=true|");

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(redirect_from_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));

  // Clean up for SimTest expectations.
  ad_document.Complete("<body></body>");
}

// Tests a scenario where a transitively added, non-subresource-filter-flagged
// script redirects to a subresource-filter-flagged URL, and this script creates
// an iframe.
//
// This test specifically verifies the *current* behavior affected by
// crbug.com/417756984. Due to this bug, the ad script ancestor decision is
// incorrectly based on the script's *initial* non-subresource-filter-flagged
// URL. This results in ancestor attribution, and the frame's creation ad script
// ancestry includes two scripts.
//
// Once the bug is fixed, the frame's creation ad script ancestry should contain
// one script. Ancestor attribution should not occur for the redirecting script
// because its final URL matches the filterlist.
TEST_F(
    AdTrackerSimTest,
    AdScriptAncestry_TransitiveScriptRedirectedFromNonFilterlistedUrlToFilterlistedUrl) {
  String final_ad_script_url = "https://example.com/script.js?ad=true";

  SimRequest::Params params;
  params.redirect_url = final_ad_script_url;

  String redirect_from_script_url =
      "https://example.com/redirect-from-script.js";
  String ad_script_url = "https://example.com/ad_script.js";
  String ad_document_url = "https://example.com/ad_document.html";

  // Scenario:
  // 1. An ad script (ad_script_url) is loaded directly in the main frame.
  // 2. This ad script loads another script (redirect_from_script_url), which
  //    gets redirected to a subresource-filter-flagged URL
  //    (final_ad_script_url).
  // 3. The final script (final_ad_script_url) creates an iframe (child_frame).
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest final_ad_script(final_ad_script_url, "text/javascript");
  SimSubresourceRequest redirect_from_script(redirect_from_script_url,
                                             "text/javascript", params);

  SimRequest ad_document(ad_document_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="ad_script.js"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    const script = document.createElement('script');
    script.src = 'redirect-from-script.js';
    document.body.appendChild(script);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  final_ad_script.Complete(R"SCRIPT(
    const iframe = document.createElement('iframe');
    iframe.src = 'ad_document.html';
    document.body.appendChild(iframe);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // Current behavior (due to crbug.com/417756984):
  // There are two ad scripts in the ancestry. Ancestor attribution occurs
  // because the decision is erroneously dictated by the script's initial,
  // non-filterlisted URL.
  //
  // Expected behavior (post-fix):
  // The frame's creation ad script ancestry contains one script. Ancestor
  // attribution does not occur because the transitive script's final URL
  // matches the filterlist.
  //
  // TODO: Update expectations once the bug is fixed.
  // EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(),
  //           1u);
  // EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry().root_script_filterlist_rule.ToString()),
  //           "ad=true|");
  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 2u);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad_script.js|");

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(redirect_from_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(final_ad_script_url));

  // Clean up for SimTest expectations.
  ad_document.Complete("<body></body>");
}

// Tests a scenario where the same vanilla script is loaded from two different
// ancestor ad scripts. The vanilla script is designed to create an ad iframe
// only on its second execution.
//
// Expectation: The ancestor ad script attributed to the final iframe is the
// *first* ad script that loaded the vanilla script. This verifies that the
// ancestor ad script is associated with the vanilla script upon its initial
// load and persists across subsequent loads from different ad scripts.
TEST_F(AdTrackerSimTest,
       AdScriptAncestry_SameTransitiveScriptLoadedFromDifferentAncestors) {
  String vanilla_script_url = "https://example.com/vanilla-script.js";
  String ad_script1_url = "https://example.com/ad_script.js";
  String ad_script2_url = "https://example.com/script2.js?ad=true";
  String ad_document1_url = "https://example.com/ad_document1.html";
  String ad_document2_url = "https://example.com/ad_document2.html";
  String ad_document3_url = "https://example.com/ad_document3.html";

  // Scenario:
  // 1. An ad script (ad_script1_url) is loaded directly in the main frame.
  // 2. This ad script creates an iframe (child_frame1).
  // 3. The ad script also loads a vanilla script (vanilla_script_url).
  // 4. A second ad script (ad_script2_url) is loaded directly in the main
  //    frame.
  // 5. This second ad script creates an iframe (child_frame2).
  // 6. This second ad script also loads the same vanilla script
  //    (vanilla_script_url).
  // 7. The vanilla script, in this second load, creates an iframe
  //    (child_frame3).
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest ad_script1(ad_script1_url, "text/javascript");
  SimSubresourceRequest ad_script2(ad_script2_url, "text/javascript");

  SimRequest ad_document1(ad_document1_url, "text/html");
  SimRequest ad_document2(ad_document2_url, "text/html");
  SimRequest ad_document3(ad_document3_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script>
        let vanillaScriptLoadCount = 0;
      </script>
      <script src="ad_script.js"></script>
      <script src="script2.js?ad=true"></script>
    </body>
  )HTML");

  ad_script1.Complete(R"SCRIPT(
    const iframe1 = document.createElement('iframe');
    iframe1.src = 'ad_document1.html';
    document.body.appendChild(iframe1);

    const script1 = document.createElement('script');
    script1.src = 'vanilla-script.js';
    document.body.appendChild(script1);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame1, there is one ad script in the ancestry (`ad_script1`).
  auto* child_frame1 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame1->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad_script.js|");
  auto frame1_stack_ad_script =
      ad_tracker_->last_ad_script_ancestry().ancestry_chain[0];

  ad_script2.Complete(R"SCRIPT(
    const iframe2 = document.createElement('iframe');
    iframe2.src = 'ad_document2.html';
    document.body.appendChild(iframe2);

    const script2 = document.createElement('script');
    script2.src = 'vanilla-script.js';
    document.body.appendChild(script2);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame2, there is one ad script in the ancestry (`ad_script2`).
  auto* child_frame2 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(/*index=*/1));
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad=true|");
  EXPECT_TRUE(child_frame2->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  auto frame2_stack_ad_script =
      ad_tracker_->last_ad_script_ancestry().ancestry_chain[0];

  EXPECT_NE(frame1_stack_ad_script, frame2_stack_ad_script);

  vanilla_script.Complete(R"SCRIPT(
    vanillaScriptLoadCount += 1;

    if (vanillaScriptLoadCount > 1) {
      const iframe3 = document.createElement('iframe');
      iframe3.src = 'ad_document3.html';
      document.body.appendChild(iframe3);
    }
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame3, the source ad script should be the one that *first*
  // loaded this `vanilla_script` (`ad_script1`).
  auto* child_frame3 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(/*index=*/2));
  EXPECT_TRUE(child_frame3->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 2u);
  EXPECT_NE(ad_tracker_->last_ad_script_ancestry().ancestry_chain[0],
            ad_tracker_->last_ad_script_ancestry().ancestry_chain[1]);
  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain[1],
            frame1_stack_ad_script);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad_script.js|");

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script1_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script2_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));

  // Clean up for SimTest expectations.
  ad_document1.Complete("<body></body>");
  ad_document2.Complete("<body></body>");
  ad_document3.Complete("<body></body>");
}

// Tests that `IsAdScriptInStack` returns the correct ad script ancestry when
// the originating ad script executes in a different browsing context than the
// final ad frame's creation.
TEST_F(AdTrackerSimTest, AdScriptAncestry_TrackedAcrossContexts) {
  String vanilla_script_url = "https://example.com/vanilla-script.js";
  String ad_script_url = "https://example.com/script.js?ad=true";

  String vanilla_document_url = "https://example.com/vanilla_document.html";
  String ad_document1_url = "https://example.com/ad_document1.html";
  String ad_document2_url = "https://example.com/ad_document2.html";

  // Scenario:
  // 1. A child frame (vanilla_document_url) is embedded in the main frame.
  // 2. An ad script (ad_script_url) is loaded within the child frame.
  // 3. The ad script creates a nested ad iframe (ad_document1_url).
  // 4. The ad script also injects a vanilla script (vanilla_script_url) into
  //    the parent (main) frame's context.
  // 5. This injected non-ad script then creates another ad iframe
  //    (ad_document2_url) in the main frame.
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");

  SimRequest vanilla_document(vanilla_document_url, "text/html");
  SimRequest ad_document1(ad_document1_url, "text/html");
  SimRequest ad_document2(ad_document2_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <iframe src="vanilla_document.html"></iframe>
    </body>
  )HTML");
  base::RunLoop().RunUntilIdle();

  // child_frame1 is not an ad frame.
  auto* child_frame1 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_FALSE(child_frame1->IsFrameCreatedByAdScript());

  child_frame1->GetDocument()->Loader()->SetSubresourceFilter(
      CreateSubresourceFilter(ruleset_));

  vanilla_document.Complete(R"HTML(
    <body>
      <script src="script.js?ad=true"></script>
    </body>
  )HTML");
  base::RunLoop().RunUntilIdle();

  ad_script.Complete(R"SCRIPT(
    const iframe = document.createElement('iframe');
    iframe.src = 'ad_document1.html';
    document.body.appendChild(iframe);

    const parentWindow = window.parent;
    const parentDocument = parentWindow.document;

    const script = document.createElement('script');
    script.src = 'vanilla-script.js';
    parentDocument.body.appendChild(script);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For nested_frame, there is one ad script in the ancestry (`ad_script`).
  auto* nested_frame = To<LocalFrame>(child_frame1->Tree().FirstChild());
  EXPECT_TRUE(nested_frame->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  auto nested_frame_stack_ad_script =
      ad_tracker_->last_ad_script_ancestry().ancestry_chain[0];

  vanilla_script.Complete(R"SCRIPT(
    const ad_iframe = document.createElement('iframe');
    ad_iframe.src = 'ad_document2.html';
    document.body.appendChild(ad_iframe);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame2, there are two ad scripts in the ancestry. The source ad
  // script should match the one captured during the creation of nested_frame.
  // This confirms that the ancestry tracking mechanism spans across browsing
  // context boundaries.
  auto* child_frame2 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(/*index=*/1));
  EXPECT_TRUE(child_frame2->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 2u);
  EXPECT_NE(ad_tracker_->last_ad_script_ancestry().ancestry_chain[0],
            ad_tracker_->last_ad_script_ancestry().ancestry_chain[1]);
  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain[1],
            nested_frame_stack_ad_script);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "ad=true|");

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));

  // Clean up for SimTest expectations.
  ad_document1.Complete("<body></body>");
  ad_document2.Complete("<body></body>");
}

// Verifies that when a non-ad script instructs an ad context (created by ad
// script) to asynchronously create an iframe, that new iframe will be correctly
// identified as not an ad since it's 1p script running more 1p script.
TEST_F(AdTrackerSimTest,
       AdScriptAncestry_AdFrameScriptedToAsynchronouslyCreateIframe) {
  String ad_script_url = "https://example.com/ad_script.js";
  String trigger_script_url = "https://example.com/trigger-script.js";

  String ad_document1_url = "https://example.com/ad_document1.html";
  String ad_document2_url = "https://example.com/ad_document2.html";

  // Scenario:
  // 1. An ad script (ad_script_url) is loaded within the main frame and creates
  //    a child iframe (ad_document1_url).
  // 2. Another script (trigger_script_url) is loaded within the main frame. It
  //    is scripting the child ad frame to asynchronously create another ad
  //    iframe (ad_document2_url) in the main frame.
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest trigger_script(trigger_script_url, "text/javascript");

  SimRequest ad_document1(ad_document1_url, "text/html");
  SimRequest ad_document2(ad_document2_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="ad_script.js"></script>
      <script src="trigger-script.js"></script>
    </body>
  )HTML");

  ad_script.Complete(R"SCRIPT(
    const ad_iframe1 = document.createElement('iframe');
    ad_iframe1.src = 'ad_document1.html';
    document.body.appendChild(ad_iframe1);
  )SCRIPT");

  ad_document1.Complete(R"HTML(
    <body>
    </body>
  )HTML");
  base::RunLoop().RunUntilIdle();

  auto* child_frame1 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_TRUE(child_frame1->IsFrameCreatedByAdScript());
  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 1u);
  auto frame1_stack_ad_script =
      ad_tracker_->last_ad_script_ancestry().ancestry_chain[0];

  // This emulates the SubresourceFilterAgent's tagging.
  SetIsAdFrame(child_frame1);

  trigger_script.Complete(R"SCRIPT(
    const iframe = document.querySelector('iframe');
    iframe.contentWindow.setTimeout(() => {
      const ad_iframe2 = document.createElement('iframe');
      ad_iframe2.src = 'ad_document2.html';
      document.body.appendChild(ad_iframe2);
    });
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // child_frame2 is not an ad frame. While the asynchronous setTimeout callback
  // is ad-related, it's ultimately 1p context running 1p script in the 1p
  // context.
  auto* child_frame2 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(/*index=*/1));
  EXPECT_FALSE(child_frame2->IsFrameCreatedByAdScript());

  // Clean up for SimTest expectations.
  ad_document2.Complete("<body></body>");
}

// Test the scenario where a non-ad script instructs an ad context (flagged
// directly by subresource filter) to asynchronously create a script. Expect no
// crashing. This is a regression test for https://crbug.com/421164512.
TEST_F(AdTrackerSimTest,
       FilterlistedAdFrameScriptedToAsynchronouslyCreateScript) {
  String trigger_script_url = "https://example.com/trigger-script.js";

  String ad_document_url = "https://example.com/ad_document.html";

  // Scenario:
  // 1. A child ad frame (ad_document_url) is embedded in the main frame.
  // 2. Another script (trigger_script_url) is loaded within the main frame. It
  //    is scripting the child ad frame to asynchronously create another inline
  //    script in the main frame.
  SimSubresourceRequest trigger_script(trigger_script_url, "text/javascript");

  SimRequest ad_document(ad_document_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <iframe src="ad_document.html"></iframe>
      <script src="trigger-script.js"></script>
    </body>
  )HTML");

  ad_document.Complete(R"HTML(
    <body>
    </body>
  )HTML");
  base::RunLoop().RunUntilIdle();

  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_FALSE(child_frame->IsFrameCreatedByAdScript());

  // This emulates the SubresourceFilterAgent's tagging, indicating this frame
  // is an ad frame due to direct filterlist matching.
  SetIsAdFrame(child_frame, /*created_by_ad_script=*/false);

  trigger_script.Complete(R"SCRIPT(
    const iframe = document.querySelector('iframe');
    iframe.contentWindow.setTimeout(() => {
      const script = document.createElement('script');
      script.textContent = `
        // Some random comment to make this script non-trivial
      `;
      document.body.appendChild(script);
    });
  )SCRIPT");
  base::RunLoop().RunUntilIdle();
}

// Tests that `IsAdScriptInStack` returns the correct filterlist rule even when
// the originating document's ruleset is updated.
TEST_F(AdTrackerSimTest, AdScriptAncestry_RulesetInOriginatingDocumentUpdated) {
  String vanilla_script_url = "https://example.com/vanilla-script.js";
  String ad_script_url = "https://example.com/library.js";

  String vanilla_document_url = "https://example.com/vanilla_document.html";
  String ad_document_url = "https://example.com/ad_document.html";

  // Scenario:
  // 1. A child frame (vanilla_document_url) is embedded in the main frame.
  // 2. An ad script (ad_script_url) is loaded within the child frame.
  // 3. The ad script injects a vanilla script (vanilla_script_url) into
  //    the parent (main) frame's context.
  // 4. The ruleset in the child frame is updated.
  // 5. This injected non-ad script then creates an ad iframe (ad_document_url)
  //    in the main frame.
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");

  SimRequest vanilla_document(vanilla_document_url, "text/html");
  SimRequest ad_document(ad_document_url, "text/html");

  main_resource_->Complete(R"HTML(
    <body>
      <iframe src="vanilla_document.html"></iframe>
    </body>
  )HTML");
  base::RunLoop().RunUntilIdle();

  // child_frame1 is not an ad frame.
  auto* child_frame1 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  EXPECT_FALSE(child_frame1->IsFrameCreatedByAdScript());

  {
    subresource_filter::testing::TestRulesetPair test_ruleset_pair;
    subresource_filter::testing::TestRulesetCreator ruleset_creator;
    ruleset_creator.CreateRulesetToDisallowURLsWithPathSuffix(
        "library.js", &test_ruleset_pair);
    scoped_refptr<const subresource_filter::MemoryMappedRuleset>
        child1_ruleset =
            subresource_filter::MemoryMappedRuleset::CreateAndInitialize(
                subresource_filter::testing::TestRuleset::Open(
                    test_ruleset_pair.indexed));

    child_frame1->GetDocument()->Loader()->SetSubresourceFilter(
        CreateSubresourceFilter(std::move(child1_ruleset)));
  }

  vanilla_document.Complete(R"HTML(
    <body>
      <script src="library.js"></script>
    </body>
  )HTML");
  base::RunLoop().RunUntilIdle();

  ad_script.Complete(R"SCRIPT(
    const parentWindow = window.parent;
    const parentDocument = parentWindow.document;

    const script = document.createElement('script');
    script.src = 'vanilla-script.js';
    parentDocument.body.appendChild(script);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // Update the ruleset in `child_frame1`.
  {
    subresource_filter::testing::TestRulesetPair test_ruleset_pair;
    subresource_filter::testing::TestRulesetCreator ruleset_creator;
    ruleset_creator.CreateRulesetToDisallowURLsWithPathSuffix(
        "library2.js", &test_ruleset_pair);
    scoped_refptr<const subresource_filter::MemoryMappedRuleset>
        updated_child1_ruleset =
            subresource_filter::MemoryMappedRuleset::CreateAndInitialize(
                subresource_filter::testing::TestRuleset::Open(
                    test_ruleset_pair.indexed));

    child_frame1->GetDocument()->Loader()->SetSubresourceFilter(
        CreateSubresourceFilter(std::move(updated_child1_ruleset)));

    // Make sure the original SubresourceFilter (that contains the original
    // ruleset reference) is destroyed.
    v8::Isolate::GetCurrent()->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
  }

  vanilla_script.Complete(R"SCRIPT(
    const ad_iframe = document.createElement('iframe');
    ad_iframe.src = 'ad_document.html';
    document.body.appendChild(ad_iframe);
  )SCRIPT");
  base::RunLoop().RunUntilIdle();

  // For child_frame2, there are two ad scripts in the ancestry. The filterlist
  // rule should still be valid even though the originating document's ruleset
  // is updated.
  auto* child_frame2 =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().ScopedChild(/*index=*/1));
  EXPECT_TRUE(child_frame2->IsFrameCreatedByAdScript());

  EXPECT_EQ(ad_tracker_->last_ad_script_ancestry().ancestry_chain.size(), 2u);
  EXPECT_NE(ad_tracker_->last_ad_script_ancestry().ancestry_chain[0],
            ad_tracker_->last_ad_script_ancestry().ancestry_chain[1]);
  EXPECT_EQ(String(ad_tracker_->last_ad_script_ancestry()
                       .root_script_filterlist_rule.ToString()),
            "library.js|");

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));

  // Clean up for SimTest expectations.
  ad_document.Complete("<body></body>");
}

// Tests that a call is correctly flagged as an ad when the API is not
// monkeypatched. The `ignore_monkey_patch` heuristic does not apply, and the
// ad script at the top of the stack triggers the detection.
TEST_F(AdTrackerSimTest, IgnoreMonkeyPatchHeuristic_ApiNotMonkeypatched_IsAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  // The ad script defines a function that calls pushState directly. The API
  // itself is not monkeypatched.
  ad_script.Complete(R"SCRIPT(
    function doAdWork() {
      window.history.pushState({}, '', '/new-url');
    }
  )SCRIPT");

  // The vanilla script calls the function defined in the ad script.
  vanilla_script.Complete(R"SCRIPT(
    doAdWork();
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // The call is correctly identified as originating from an ad script because
  // the API is native, so the heuristic to ignore monkey patch pattern is
  // skipped.
  EXPECT_TRUE(ad_tracker_->last_is_ad_script_in_stack_result());
}

// Tests that the heuristic correctly ignores the first call to a monkeypatched
// API from a non-ad script. This prevents misattributing the call to the ad
// script, which is likely acting only as a proxy.
TEST_F(AdTrackerSimTest, IgnoreMonkeyPatchHeuristic_FirstProxiedCall_IsNotAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  // The ad script monkeypatches history.pushState.
  ad_script.Complete(R"SCRIPT(
    const originalPushState = window.history.pushState;
    window.history.pushState = function(...args) {
      originalPushState.apply(window.history, args);
    };
  )SCRIPT");

  // The vanilla script calls the now-monkeypatched API. The call stack will
  // have the ad script's wrapper at the top.
  vanilla_script.Complete(R"SCRIPT(
    window.history.pushState({}, '', '/new-url');
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // The IsAdScriptInStack check is triggered by the pushState implementation.
  // The heuristic identifies the monkeypatch and, for this first call, assumes
  // the ad script is a proxy and returns false.
  EXPECT_FALSE(ad_tracker_->last_is_ad_script_in_stack_result());
}

// Tests that the heuristic correctly ignores the first call to a monkeypatched
// API from a non-ad script. This prevents misattributing the call to the ad
// script, which is likely acting only as a proxy. The only difference from
// the test above is that the monkeypatch function has a name.
TEST_F(AdTrackerSimTest,
       IgnoreMonkeyPatchHeuristic_FirstNamedProxiedCall_IsNotAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  // The ad script monkeypatches history.pushState.
  ad_script.Complete(R"SCRIPT(
    const originalPushState = window.history.pushState;
    window.history.pushState = function Foo(...args) {
      originalPushState.apply(window.history, args);
    };
  )SCRIPT");

  // The vanilla script calls the now-monkeypatched API. The call stack will
  // have the ad script's wrapper at the top.
  vanilla_script.Complete(R"SCRIPT(
    window.history.pushState({}, '', '/new-url');
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // The IsAdScriptInStack check is triggered by the pushState implementation.
  // The heuristic identifies the monkeypatch and, for this first call, assumes
  // the ad script is a proxy and returns false.
  EXPECT_FALSE(ad_tracker_->last_is_ad_script_in_stack_result());
}

// Tests that monkeypatched API is invoked from non-ad script, and the second
// proxy within the monkeypatch function is correctly flagged as an ad. The
// heuristic is designed to only ignore the *first* call, assuming subsequent
// calls from the same ad script are genuine ad behavior.
TEST_F(AdTrackerSimTest, IgnoreMonkeyPatchHeuristic_SecondProxiedCall_IsAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  // The ad script monkeypatches history.pushState and, within the patch,
  // invokes the original function twice.
  ad_script.Complete(R"SCRIPT(
    const originalPushState = window.history.pushState;
    window.history.pushState = function(...args) {
      originalPushState.apply(window.history, args);
      originalPushState.apply(window.history, args);
    };
  )SCRIPT");

  // The vanilla script calls the monkeypatched API once.
  vanilla_script.Complete(R"SCRIPT(
    window.history.pushState({}, '', '/new-url');
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // The first call to the native pushState is ignored by the heuristic. The
  // second call is not, as the heuristic only applies once per API per task.
  // The TestAdTracker stores the result of the *last* call, which should be
  // true.
  EXPECT_TRUE(ad_tracker_->last_is_ad_script_in_stack_result());
}

// Tests that a nested task (triggered by document.write) does not reset the
// heuristic's state for the top-level task. A second proxied call after the
// nested task completes is still correctly flagged as an ad.
TEST_F(AdTrackerSimTest,
       IgnoreMonkeyPatchHeuristic_NestedTaskDoesNotResetState) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  // The ad script monkeypatches history.pushState and, within the patch,
  // invokes the original function twice, creating a nested task in between.
  ad_script.Complete(R"SCRIPT(
    const originalPushState = window.history.pushState;
    window.history.pushState = function(...args) {
      originalPushState.apply(window.history, args);

      // Trigger a nested task.
      document.write(`
        <script>
          // Non-trivial script
        <\/script>
      `);

      originalPushState.apply(window.history, args);
    };
  )SCRIPT");

  // The vanilla script calls the monkeypatched API once.
  vanilla_script.Complete(R"SCRIPT(
    window.history.pushState({}, '', '/new-url');
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // The first call to the native pushState is ignored by the heuristic. The
  // second call is not, as the heuristic only applies once per API per task.
  // The TestAdTracker stores the result of the *last* call, which should be
  // true.
  EXPECT_TRUE(ad_tracker_->last_is_ad_script_in_stack_result());
}

// Tests a limitation of the heuristic. When a non-ad script makes
// two separate calls to a monkeypatched API, the first call is correctly
// ignored, but the second is flagged as an ad. This is a known false positive
// because the heuristic's state is tied to the broader AdTracker task, not the
// immediate V8 stack frame.
TEST_F(AdTrackerSimTest,
       IgnoreMonkeyPatchHeuristic_TwoProxiedCallsInSameTask_SecondIsAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  // The ad script monkeypatches history.pushState.
  ad_script.Complete(R"SCRIPT(
    const originalPushState = window.history.pushState;
    window.history.pushState = function(...args) {
      originalPushState.apply(window.history, args);
    };
  )SCRIPT");

  // The non-ad script calls the monkeypatched API twice.
  vanilla_script.Complete(R"SCRIPT(
    window.history.pushState({}, '', '/new-url');
    window.history.pushState({}, '', '/new-url');
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // The first call is ignored, but the second is flagged as an ad because it's
  // the second time the heuristic has seen a call to this API within the same
  // synchronous task. The last recorded result will be true.
  EXPECT_TRUE(ad_tracker_->last_is_ad_script_in_stack_result());
}

// Tests that the bottom-of-stack check takes precedence over the monkeypatching
// heuristic. When an ad script initiates the execution (and is thus at the
// bottom of the stack), any calls it makes are flagged as ads, even if it also
// monkeypatches the API.
TEST_F(AdTrackerSimTest,
       IgnoreMonkeyPatchHeuristic_AdScriptAtBottomOfStack_IsAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script></body>
  )HTML");

  // The ad script first monkeypatches the API and then immediately calls it.
  ad_script.Complete(R"SCRIPT(
    const originalPushState = window.history.pushState;
    window.history.pushState = function(...args) {
      originalPushState.apply(window.history, args);
    };

    window.history.pushState({}, '', '/new-url');
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // The call is flagged as an ad because the AdTracker first identifies that an
  // ad script is at the bottom of the execution stack and returns true
  // immediately, without evaluating the top-of-stack heuristic.
  EXPECT_TRUE(ad_tracker_->last_is_ad_script_in_stack_result());
}

// Tests that the heuristic correctly identifies an ad when a non-ad script
// calls an ad function that performs a "just-in-time" monkeypatch and then
// calls the API. This is not a legitimate monkey patch pattern, and should be
// flagged.
TEST_F(AdTrackerSimTest,
       IgnoreMonkeyPatchHeuristic_JustInTimePatchByAdFunction_IsAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  // The ad script defines a function that will perform the monkeypatch and
  // call.
  ad_script.Complete(R"SCRIPT(
    function performAdAction() {
      const originalPushState = window.history.pushState;
      window.history.pushState = function(...args) {
        originalPushState.apply(window.history, args);
      };

      window.history.pushState({}, '', '/new-url');
    }
  )SCRIPT");

  // The vanilla script initiates the entire chain by calling the ad function.
  vanilla_script.Complete(R"SCRIPT(
    performAdAction();
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // The heuristic correctly flags this as an ad. It walks the stack and finds
  // that the function at the ad/non-ad boundary is `performAdAction`, not the
  // `pushState` API itself. This indicates it's not a simple proxy call, so
  // the ad script at the top of the stack is not ignored.
  EXPECT_TRUE(ad_tracker_->last_is_ad_script_in_stack_result());
}

// Tests that the heuristic correctly ignores the first proxied call in an
// asynchronous task (e.g., a setTimeout callback).
TEST_F(AdTrackerSimTest,
       IgnoreMonkeyPatchHeuristic_AsyncFirstProxiedCall_IsNotAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  // The ad script monkeypatches history.pushState.
  ad_script.Complete(R"SCRIPT(
    const originalPushState = window.history.pushState;
    window.history.pushState = function(...args) {
      originalPushState.apply(window.history, args);
    };
  )SCRIPT");

  // The vanilla script uses setTimeout to asynchronously call the monkeypatched
  // API.
  vanilla_script.Complete(R"SCRIPT(
    setTimeout(() => {
      window.history.pushState({}, '', '/new-url');
    }, 0);
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // The heuristic still applies within the async task. Because it's the first
  // call within this new task, it returns false.
  EXPECT_FALSE(ad_tracker_->last_is_ad_script_in_stack_result());
}

// Tests that a second call to a monkeypatched API within the same asynchronous
// task is correctly flagged as an ad.
TEST_F(AdTrackerSimTest,
       IgnoreMonkeyPatchHeuristic_AsyncSecondCallInSameTask_IsAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  // The ad script monkeypatches history.pushState and invokes the original
  // function twice within the patch.
  ad_script.Complete(R"SCRIPT(
    const originalPushState = window.history.pushState;
    window.history.pushState = function(...args) {
      originalPushState.apply(window.history, args);
      originalPushState.apply(window.history, args);
    };
  )SCRIPT");

  // The vanilla script uses setTimeout to asynchronously call the monkeypatched
  // API.
  vanilla_script.Complete(R"SCRIPT(
    setTimeout(() => {
      window.history.pushState({}, '', '/new-url');
    }, 0);
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // The first call within the async task is ignored. The second is not. The
  // TestAdTracker stores the result of the *last* call, which should be true.
  EXPECT_TRUE(ad_tracker_->last_is_ad_script_in_stack_result());
}

// Tests that the heuristic correctly resets across separate asynchronous tasks.
// When two separate setTimeout callbacks from a non-ad script each call the
// monkeypatched API, both calls are ignored because they are the first call
// within their respective new task scopes.
TEST_F(AdTrackerSimTest,
       IgnoreMonkeyPatchHeuristic_TwoSeparateAsyncTasks_BothAreNotAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  // The ad script monkeypatches history.pushState.
  ad_script.Complete(R"SCRIPT(
    const originalPushState = window.history.pushState;
    window.history.pushState = function(...args) {
      originalPushState.apply(window.history, args);
    };
  )SCRIPT");

  // The vanilla script uses setTimeout twice, each asynchronously calling the
  // monkeypatched API.
  vanilla_script.Complete(R"SCRIPT(
    setTimeout(() => {
      window.history.pushState({}, '', '/new-url-1');
    }, 0);
    setTimeout(() => {
      window.history.pushState({}, '', '/new-url-2');
    }, 0);
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // Each setTimeout creates a new synchronous task, resetting the heuristic's
  // state. Therefore, the call in the second task is also considered a "first
  // call" and is ignored. The last recorded result is false.
  EXPECT_FALSE(ad_tracker_->last_is_ad_script_in_stack_result());
}

// Tests a known limitation where the heuristic does not apply to calls within
// a Promise callback (e.g., .then()). This is because AdTracker does not
// currently track synchronous task scopes for promise resolutions, so the call
// is flagged as an ad based on the top-of-stack script.
TEST_F(AdTrackerSimTest,
       IgnoreMonkeyPatchHeuristic_PromiseCallbackDoesNotApply_IsAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/script.js";
  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");

  main_resource_->Complete(R"HTML(
    <body><script src="script.js?ad=true"></script>
          <script src="script.js"></script></body>
  )HTML");

  // The ad script monkeypatches history.pushState.
  ad_script.Complete(R"SCRIPT(
    const originalPushState = window.history.pushState;
    window.history.pushState = function(...args) {
      originalPushState.apply(window.history, args);
    };
  )SCRIPT");

  // The vanilla script uses Promise.resolve().then() to asynchronously call
  // the monkeypatched API.
  vanilla_script.Complete(R"SCRIPT(
    Promise.resolve().then(() => {
      window.history.pushState({}, '', '/new-url');
    });
  )SCRIPT");

  base::RunLoop().RunUntilIdle();

  // AdTracker does not currently track synchronous task scopes for promise
  // resolutions. The check therefore falls back to identifying the ad script at
  // the top of the stack and returns true.
  EXPECT_TRUE(ad_tracker_->last_is_ad_script_in_stack_result());
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

// Tests that a non-ad script listening for DOM mutations does not have its
// image loads tagged as ads, even when the mutation is caused by an ad script.
TEST_F(AdTrackerSimTest, ImageLoadInMutationObserverFromAdScriptIsNotAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/vanilla_script.js";
  String image_url = "https://example.com/image.gif";

  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest image(image_url, "image/gif");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="vanilla_script.js"></script>
      <script src="script.js?ad=true"></script>
    </body>
  )HTML");

  // The vanilla script sets up a mutation observer. When the ad script adds an
  // iframe, this observer will trigger and load an image.
  vanilla_script.Complete(R"SCRIPT(
    const observer = new MutationObserver((mutationsList, observer) => {
      let image = document.createElement("img");
      image.src = "image.gif";
      document.body.appendChild(image);
      observer.disconnect();
    });
    observer.observe(document.body, { childList: true });
  )SCRIPT");

  // The ad script simply creates an iframe, which triggers the mutation
  // observer in the vanilla script.
  ad_script.Complete(R"SCRIPT(
    let iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
  )SCRIPT");

  ad_tracker_->WaitForSubresource(image_url);
  image.Complete();

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));

  // The image load was initiated by the vanilla script's mutation observer,
  // so it should not be tagged as an ad.
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(image_url));
}

// Tests that a non-ad script listening for attribute mutations does not have
// its image loads tagged as ads, even when the mutation is caused by an ad
// script.
TEST_F(AdTrackerSimTest,
       ImageLoadInAttributeMutationObserverFromAdScriptIsNotAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/vanilla_script.js";
  String image_url = "https://example.com/image.gif";

  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest image(image_url, "image/gif");

  main_resource_->Complete(R"HTML(
    <body data-foo="bar">
      <script src="vanilla_script.js"></script>
      <script src="script.js?ad=true"></script>
    </body>
  )HTML");

  // The vanilla script sets up a mutation observer for attributes. When the ad
  // script changes an attribute, this observer will trigger and load an image.
  vanilla_script.Complete(R"SCRIPT(
    const observer = new MutationObserver((mutationsList, observer) => {
      let image = document.createElement("img");
      image.src = "image.gif";
      document.body.appendChild(image);
      observer.disconnect();
    });
    observer.observe(document.body, { attributes: true });
  )SCRIPT");

  // The ad script changes an attribute, which triggers the mutation observer in
  // the vanilla script.
  ad_script.Complete(R"SCRIPT(
    document.body.setAttribute('data-foo', 'baz');
  )SCRIPT");

  ad_tracker_->WaitForSubresource(image_url);
  image.Complete();

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(vanilla_script_url));

  // The image load was initiated by the vanilla script's mutation observer,
  // so it should not be tagged as an ad.
  EXPECT_FALSE(ad_tracker_->RequestWithUrlTaggedAsAd(image_url));
}

// Tests that an ad script listening for DOM mutations is properly tagged as ad
// related.
TEST_F(AdTrackerSimTest, AdImageLoadInMutationObserverFromAdScriptIsAd) {
  String ad_script_url = "https://example.com/script.js?ad=true";
  String vanilla_script_url = "https://example.com/vanilla_script.js";
  String image_url = "https://example.com/image.gif";

  SimSubresourceRequest ad_script(ad_script_url, "text/javascript");
  SimSubresourceRequest vanilla_script(vanilla_script_url, "text/javascript");
  SimSubresourceRequest image(image_url, "image/gif");

  main_resource_->Complete(R"HTML(
    <body>
      <script src="script.js?ad=true"></script>
    </body>
  )HTML");

  // The ad script creates an iframe, which triggers its mutation
  // observer to run.
  ad_script.Complete(R"SCRIPT(
    const observer = new MutationObserver((mutationsList, observer) => {
      let image = document.createElement("img");
      image.src = "image.gif";
      document.body.appendChild(image);
      observer.disconnect();
    });
    observer.observe(document.body, { childList: true });

    let iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
  )SCRIPT");

  ad_tracker_->WaitForSubresource(image_url);
  image.Complete();

  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(ad_script_url));

  // The image load was initiated by the vanilla script's mutation observer,
  // so it should not be tagged as an ad.
  EXPECT_TRUE(ad_tracker_->RequestWithUrlTaggedAsAd(image_url));
}

}  // namespace blink
