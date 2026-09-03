// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/extension_script_tracker.h"

#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/ad_tracker/ad_tracker.h"
#include "third_party/blink/renderer/core/ad_tracker/script_initiation_monitor.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class TestExtensionScriptTracker final : public ExtensionScriptTracker {
 public:
  TestExtensionScriptTracker(LocalFrame* frame,
                             ScriptInitiationMonitor* monitor)
      : ExtensionScriptTracker(frame, monitor) {}
  ~TestExtensionScriptTracker() override = default;

  using ExtensionScriptTracker::IsMarkedScript;
  using ExtensionScriptTracker::OnScriptRegistered;
  using ScriptAncestryTracker::DidCreateFrame;
  using ScriptAncestryTracker::GetInitiatingScriptId;

  void OnScriptRegistered(ExecutionContext& execution_context,
                          V8ScriptId script_id,
                          const String& url,
                          std::optional<V8ScriptId> marked_script_id) override {
    ExtensionScriptTracker::OnScriptRegistered(execution_context, script_id,
                                               url, marked_script_id);
    registered_urls_.insert(script_id, url);
  }

  V8ScriptId FindScriptIdByUrl(const String& url_substring) const {
    for (const auto& [script_id, url] : registered_urls_) {
      if (url.find(url_substring) != kNotFound) {
        return script_id;
      }
    }
    return V8ScriptId();
  }

 private:
  HashMap<V8ScriptId, String> registered_urls_;
};

}  // namespace

class ExtensionScriptTrackerTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    CommonSchemeRegistry::RegisterURLSchemeAsExtension("chrome-extension");
    main_resource_ = std::make_unique<SimRequest>(
        "https://example.com/test.html", "text/html");

    LoadURL("https://example.com/test.html");
    tracker_ = MakeGarbageCollected<TestExtensionScriptTracker>(
        GetDocument().GetFrame(),
        GetDocument().GetFrame()->GetOrCreateScriptInitiationMonitor());
  }

  void TearDown() override {
    CommonSchemeRegistry::RemoveURLSchemeAsExtensionForTest("chrome-extension");
    if (tracker_) {
      tracker_->Shutdown();
      tracker_ = nullptr;
    }
    main_resource_.reset();
    SimTest::TearDown();
  }

  const HashMap<V8ScriptId, String>& extension_scripts() const {
    return tracker_->extension_scripts_;
  }

  std::unique_ptr<SimRequest> main_resource_;
  Persistent<TestExtensionScriptTracker> tracker_;
};

TEST_F(ExtensionScriptTrackerTest, ExtensionScriptDetectedBySchema) {
  main_resource_->Complete("");
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ASSERT_TRUE(execution_context);

  V8ScriptId script_id(100);
  tracker_->OnScriptRegistered(*execution_context, script_id,
                               "chrome-extension://abcdefghijklmnop/script.js",
                               std::nullopt);

  EXPECT_TRUE(tracker_->IsMarkedScript(script_id));
}

TEST_F(ExtensionScriptTrackerTest, ExtensionIdTrackedAndInherited) {
  main_resource_->Complete("");
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ASSERT_TRUE(execution_context);

  V8ScriptId parent_id(100);
  tracker_->OnScriptRegistered(
      *execution_context, parent_id,
      "chrome-extension://gomeoncjolbgiaildclhhbneblomedoa/script.js",
      std::nullopt);

  EXPECT_TRUE(tracker_->IsMarkedScript(parent_id));
  auto it_parent = extension_scripts().find(parent_id);
  ASSERT_NE(it_parent, extension_scripts().end());
  EXPECT_EQ(it_parent->value, "gomeoncjolbgiaildclhhbneblomedoa");

  // Child script loaded by the extension script inherits extension ID.
  V8ScriptId child_id(101);
  tracker_->OnScriptRegistered(*execution_context, child_id,
                               "https://example.com/dynamic.js",
                               std::make_optional(parent_id));

  EXPECT_TRUE(tracker_->IsMarkedScript(child_id));
  auto it_child = extension_scripts().find(child_id);
  ASSERT_NE(it_child, extension_scripts().end());
  EXPECT_EQ(it_child->value, "gomeoncjolbgiaildclhhbneblomedoa");
}

TEST_F(ExtensionScriptTrackerTest, InitiatorPrecedenceOverUrl) {
  main_resource_->Complete("");
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ASSERT_TRUE(execution_context);

  V8ScriptId parent_id(100);
  tracker_->OnScriptRegistered(
      *execution_context, parent_id,
      "chrome-extension://aaaaaajcolbgiaildclhhbneblomedoa/script.js",
      std::nullopt);

  EXPECT_TRUE(tracker_->IsMarkedScript(parent_id));
  auto it_parent = extension_scripts().find(parent_id);
  ASSERT_NE(it_parent, extension_scripts().end());
  EXPECT_EQ(it_parent->value, "aaaaaajcolbgiaildclhhbneblomedoa");

  // Child script loaded by Extension A with Extension B's URL is attributed to
  // Extension A.
  V8ScriptId child_id(101);
  tracker_->OnScriptRegistered(
      *execution_context, child_id,
      "chrome-extension://bbbbbbjcolbgiaildclhhbneblomedoa/script.js",
      std::make_optional(parent_id));

  EXPECT_TRUE(tracker_->IsMarkedScript(child_id));
  auto it_child = extension_scripts().find(child_id);
  ASSERT_NE(it_child, extension_scripts().end());
  EXPECT_EQ(it_child->value, "aaaaaajcolbgiaildclhhbneblomedoa");
}

TEST_F(ExtensionScriptTrackerTest, VanillaScriptNotMarked) {
  main_resource_->Complete("");
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ASSERT_TRUE(execution_context);

  V8ScriptId script_id(101);
  tracker_->OnScriptRegistered(*execution_context, script_id,
                               "https://example.com/vanilla.js", std::nullopt);

  EXPECT_FALSE(tracker_->IsMarkedScript(script_id));
}

TEST_F(ExtensionScriptTrackerTest, TransitiveExtensionScriptMarking) {
  main_resource_->Complete("");
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ASSERT_TRUE(execution_context);

  V8ScriptId parent_id(102);
  tracker_->OnScriptRegistered(*execution_context, parent_id,
                               "chrome-extension://abcdefghijklmnop/parent.js",
                               std::nullopt);
  EXPECT_TRUE(tracker_->IsMarkedScript(parent_id));

  // Child script without extension schema, but with extension marked script on
  // stack.
  V8ScriptId child_id(103);
  tracker_->OnScriptRegistered(*execution_context, child_id,
                               "https://example.com/child.js", parent_id);
  EXPECT_TRUE(tracker_->IsMarkedScript(child_id));
}

TEST_F(ExtensionScriptTrackerTest, ScriptLoadedWhileExecutingExtensionScript) {
  const char kExtensionUrl[] = "chrome-extension://abcdefghijklmnop/script.js";
  const char kVanillaUrl[] = "https://example.com/vanilla_script.js";
  SimSubresourceRequest extension_resource(kExtensionUrl, "text/javascript");
  SimSubresourceRequest vanilla_script(kVanillaUrl, "text/javascript");

  main_resource_->Complete(
      "<body></body><script "
      "src='chrome-extension://abcdefghijklmnop/script.js'></script>");

  extension_resource.Complete(R"SCRIPT(
    script = document.createElement("script");
    script.src = "vanilla_script.js";
    document.body.appendChild(script);
    )SCRIPT");

  // Wait for script to run and initiate subresource request.
  base::RunLoop().RunUntilIdle();

  V8ScriptId extension_script_id =
      tracker_->FindScriptIdByUrl("abcdefghijklmnop/script.js");
  EXPECT_NE(V8ScriptId(), extension_script_id);
  EXPECT_TRUE(tracker_->IsMarkedScript(extension_script_id));

  vanilla_script.Complete("");

  // Wait for vanilla_script to compile and run.
  base::RunLoop().RunUntilIdle();

  V8ScriptId vanilla_script_id =
      tracker_->FindScriptIdByUrl("vanilla_script.js");
  EXPECT_NE(V8ScriptId(), vanilla_script_id);
  EXPECT_TRUE(tracker_->IsMarkedScript(vanilla_script_id));
}

TEST_F(ExtensionScriptTrackerTest, InjectedExtensionScriptExecutionScope) {
  main_resource_->Complete("");
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ASSERT_TRUE(execution_context);

  V8ScriptId script_id(105);
  ScriptInitiationMonitor* monitor =
      GetDocument().GetFrame()->GetScriptInitiationMonitor();
  ASSERT_TRUE(monitor);

  {
    ScriptInitiationMonitor::ScopedInjectedExtensionScriptExecution scope(
        monitor, "abcdefghijklmnop");
    tracker_->OnScriptRegistered(*execution_context, script_id, "",
                                 std::nullopt);
  }

  EXPECT_TRUE(tracker_->IsMarkedScript(script_id));
  auto it = extension_scripts().find(script_id);
  ASSERT_NE(it, extension_scripts().end());
  EXPECT_EQ(it->value, "abcdefghijklmnop");
}

TEST_F(ExtensionScriptTrackerTest, NonExtensionHostInjectionNotTagged) {
  main_resource_->Complete("");
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ASSERT_TRUE(execution_context);

  V8ScriptId script_id(106);
  ScriptInitiationMonitor* monitor =
      GetDocument().GetFrame()->GetScriptInitiationMonitor();
  ASSERT_TRUE(monitor);

  // When a non-extension host (e.g. WebUI, ControlledFrame) injects a script,
  // script_injector_id is empty, so no ScopedInjectedExtensionScriptExecution
  // is entered. The script should not be marked or tracked as an extension
  // script.
  tracker_->OnScriptRegistered(*execution_context, script_id, "", std::nullopt);

  EXPECT_FALSE(tracker_->IsMarkedScript(script_id));
  EXPECT_TRUE(tracker_->ExtensionScriptInStack().empty());
}

TEST_F(ExtensionScriptTrackerTest, AsyncAdTrackerSideEffect) {
  GetDocument().GetFrame()->SetAdTrackerForTesting(
      MakeGarbageCollected<AdTracker>(
          GetDocument().GetFrame(),
          GetDocument().GetFrame()->GetOrCreateScriptInitiationMonitor()));

  const char kExtensionUrl[] = "chrome-extension://abcdefghijklmnop/script.js";
  SimSubresourceRequest extension_resource(kExtensionUrl, "text/javascript");

  main_resource_->Complete(
      "<body></body><script "
      "src='chrome-extension://abcdefghijklmnop/script.js'></script>");

  extension_resource.Complete(R"SCRIPT(
    setTimeout(() => {
      const btn = document.createElement('button');
      btn.id = 'mybtn';
      btn.setAttribute('onclick', 'console.log("button clicked");');
      document.body.appendChild(btn);
      btn.dispatchEvent(new Event('click'));
    }, 0);
    )SCRIPT");

  base::RunLoop().RunUntilIdle();

  V8ScriptId inline_id = tracker_->FindScriptIdByUrl("{ id ");
  EXPECT_NE(V8ScriptId(), inline_id);
  EXPECT_TRUE(tracker_->IsMarkedScript(inline_id));

  AdTracker* ad_tracker = GetDocument().GetFrame()->GetAdTracker();
  ASSERT_TRUE(ad_tracker);

  // AdTracker should not incorrectly read the extension's async context and
  // tag it as an ad.
  EXPECT_FALSE(ad_tracker->IsMarkedScript(inline_id));
}

TEST_F(ExtensionScriptTrackerTest, AsyncExtensionTrackerSideEffect) {
  AdTracker* ad_tracker = MakeGarbageCollected<AdTracker>(
      GetDocument().GetFrame(),
      GetDocument().GetFrame()->GetOrCreateScriptInitiationMonitor());
  GetDocument().GetFrame()->SetAdTrackerForTesting(ad_tracker);

  SimRequest iframe_resource("https://example.com/ad_frame.html", "text/html");
  main_resource_->Complete(
      "<body><iframe src='https://example.com/ad_frame.html'></iframe></body>");

  LocalFrame* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  ASSERT_TRUE(child_frame);

  FrameAdEvidence ad_evidence(/*parent_is_ad=*/false);
  ad_evidence.set_created_by_ad_script(
      mojom::FrameCreationStackEvidence::kCreatedByAdScript);
  ad_evidence.set_is_complete();
  child_frame->SetAdEvidence(ad_evidence);

  iframe_resource.Complete(R"HTML(
    <script>
    setTimeout(() => {
      const btn = document.createElement('button');
      btn.id = 'mybtn';
      btn.setAttribute('onclick', 'console.log("button clicked");');
      document.body.appendChild(btn);
      btn.dispatchEvent(new Event('click'));
    }, 0);
    </script>
  )HTML");

  base::RunLoop().RunUntilIdle();

  V8ScriptId inline_id = tracker_->FindScriptIdByUrl("{ id ");
  EXPECT_NE(V8ScriptId(), inline_id);

  // AdTracker should have tagged the async inline script as an ad.
  EXPECT_TRUE(ad_tracker->IsMarkedScript(inline_id));

  // ExtensionScriptTracker should not incorrectly read the ad tracker's async
  // context and tag it as an extension script.
  EXPECT_FALSE(tracker_->IsMarkedScript(inline_id));
}

TEST_F(ExtensionScriptTrackerTest, FrameCreatedByExtensionScriptTagged) {
  const char kExtensionUrl[] = "chrome-extension://abcdefghijklmnop/script.js";
  SimSubresourceRequest extension_resource(kExtensionUrl, "text/javascript");

  main_resource_->Complete(
      "<body></body><script "
      "src='chrome-extension://abcdefghijklmnop/script.js'></script>");

  extension_resource.Complete(R"SCRIPT(
    const iframe = document.createElement("iframe");
    iframe.srcdoc = "<script>console.log('srcdoc');</script>";
    document.body.appendChild(iframe);
    )SCRIPT");

  test::RunPendingTasks();

  V8ScriptId srcdoc_script_id = tracker_->FindScriptIdByUrl("{ id ");
  EXPECT_GT(srcdoc_script_id.value(), 0);
  EXPECT_TRUE(tracker_->IsMarkedScript(srcdoc_script_id));
  auto it = extension_scripts().find(srcdoc_script_id);
  ASSERT_NE(it, extension_scripts().end());
  EXPECT_EQ(it->value, "abcdefghijklmnop");
}

TEST_F(ExtensionScriptTrackerTest, ExtensionScriptInStack) {
  const char kExtensionUrl[] = "chrome-extension://abcdefghijklmnop/script.js";
  SimSubresourceRequest extension_resource(kExtensionUrl, "text/javascript");

  main_resource_->Complete(
      "<body></body><script "
      "src='chrome-extension://abcdefghijklmnop/script.js'></script>");

  extension_resource.Complete(R"SCRIPT(
    const iframe = document.createElement("iframe");
    iframe.srcdoc = "<script>console.log('srcdoc');</script>";
    document.body.appendChild(iframe);
    )SCRIPT");

  test::RunPendingTasks();

  LocalFrame* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  ASSERT_TRUE(child_frame);

  // Outside the child frame context, no extension script is in stack.
  EXPECT_TRUE(tracker_->ExtensionScriptInStack().empty());

  // Inside the child frame context, ExtensionScriptInStack returns the
  // extension ID that created the frame.
  v8::HandleScope handle_scope(Window().GetIsolate());
  v8::Context::Scope context_scope(
      ToScriptStateForMainWorld(child_frame)->GetContext());
  EXPECT_EQ(tracker_->ExtensionScriptInStack(), "abcdefghijklmnop");
}

TEST_F(ExtensionScriptTrackerTest,
       FrameCreatedByExtensionScriptPreservedAcrossSameProcessNavigation) {
  const char kExtensionUrl[] = "chrome-extension://abcdefghijklmnop/script.js";
  SimSubresourceRequest extension_resource(kExtensionUrl, "text/javascript");
  SimRequest child_frame_doc1("https://example.com/frame1.html", "text/html");

  main_resource_->Complete(
      "<body><script "
      "src='chrome-extension://abcdefghijklmnop/script.js'></script></body>");

  extension_resource.Complete(R"SCRIPT(
    var iframe = document.createElement("iframe");
    iframe.id = "target_frame";
    iframe.src = "frame1.html";
    document.body.appendChild(iframe);
    )SCRIPT");

  test::RunPendingTasks();
  child_frame_doc1.Complete("<body>frame 1</body>");

  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  ASSERT_TRUE(child_frame);
  EXPECT_TRUE(tracker_->IsMarkedFrame(child_frame));

  // Navigate the child iframe same-process to frame2.html (LocalFrame <->
  // LocalFrame swap).
  SimRequest child_frame_doc2("https://example.com/frame2.html", "text/html");
  SimSubresourceRequest child_script("https://example.com/child_script.js",
                                     "text/javascript");
  MainFrame().ExecuteScript(WebScriptSource(
      "document.getElementById('target_frame').src = 'frame2.html';"));

  base::RunLoop().RunUntilIdle();
  child_frame_doc2.Complete("<script src='child_script.js'></script>");
  child_script.Complete("console.log('in frame 2');");
  base::RunLoop().RunUntilIdle();

  auto* new_child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  ASSERT_TRUE(new_child_frame);
  EXPECT_TRUE(tracker_->IsMarkedFrame(new_child_frame));

  // Verify that script running in the navigated frame is tracked as an
  // extension script.
  V8ScriptId frame2_script_id = tracker_->FindScriptIdByUrl("child_script.js");
  EXPECT_GT(frame2_script_id.value(), 0);
  EXPECT_TRUE(tracker_->IsMarkedScript(frame2_script_id));
}

TEST_F(ExtensionScriptTrackerTest, ScriptInjectionPolicyLifecycle) {
  SimRequest child_resource("https://example.com/child.html", "text/html");
  main_resource_->Complete(R"(
    <iframe id="child" src="https://example.com/child.html"></iframe>
  )");
  child_resource.Complete("");

  LocalFrame* frame = GetDocument().GetFrame();
  ASSERT_TRUE(frame);
  auto* child_element = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("child")));
  ASSERT_TRUE(child_element);
  LocalFrame* child_frame = To<LocalFrame>(child_element->ContentFrame());
  ASSERT_TRUE(child_frame);

  EXPECT_EQ(nullptr, frame->GetExtensionScriptTracker());
  EXPECT_EQ(nullptr, child_frame->GetExtensionScriptTracker());

  frame->Loader().GetDocumentLoader()->SetScriptInjectionPolicyForTesting(
      mojom::blink::ScriptInjectionPolicy::kNavigationProtection);
  frame->UpdateExtensionScriptTracking();
  ExtensionScriptTracker* tracker = frame->GetExtensionScriptTracker();
  EXPECT_NE(nullptr, tracker);
  EXPECT_EQ(tracker, child_frame->GetExtensionScriptTracker());

  // Calling again maintains the same tracker.
  frame->UpdateExtensionScriptTracking();
  EXPECT_EQ(tracker, frame->GetExtensionScriptTracker());
  EXPECT_EQ(tracker, child_frame->GetExtensionScriptTracker());

  // Setting policy to kNone shuts down and clears the tracker for both root
  // and subframe.
  frame->Loader().GetDocumentLoader()->SetScriptInjectionPolicyForTesting(
      mojom::blink::ScriptInjectionPolicy::kNone);
  frame->UpdateExtensionScriptTracking();
  EXPECT_EQ(nullptr, frame->GetExtensionScriptTracker());
  EXPECT_EQ(nullptr, child_frame->GetExtensionScriptTracker());
}

TEST_F(ExtensionScriptTrackerTest, ExtensionScriptUrlsTestingAPI) {
  main_resource_->Complete("");
  ScopedExtensionScriptTaggingTestingAPIForTest enable_testing_api(true);
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  ASSERT_TRUE(execution_context);

  V8ScriptId script_id(200);
  tracker_->OnScriptRegistered(*execution_context, script_id,
                               "chrome-extension://abcdefghijklmnop/script.js",
                               std::nullopt);

  EXPECT_TRUE(tracker_->IsExtensionScriptUrlMarked(
      "chrome-extension://abcdefghijklmnop/script.js"));
  EXPECT_FALSE(
      tracker_->IsExtensionScriptUrlMarked("https://example.com/unrelated.js"));
}
}  // namespace blink
