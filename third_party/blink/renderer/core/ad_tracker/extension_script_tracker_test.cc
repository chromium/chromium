// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/extension_script_tracker.h"

#include <memory>
#include <optional>

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/core/ad_tracker/ad_tracker.h"
#include "third_party/blink/renderer/core/ad_tracker/script_initiation_monitor.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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
        monitor);
    tracker_->OnScriptRegistered(*execution_context, script_id, "",
                                 std::nullopt);
  }

  EXPECT_TRUE(tracker_->IsMarkedScript(script_id));
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

}  // namespace blink
