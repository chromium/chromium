// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/installedapp/installed_app_controller.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class InstalledAppControllerTest : public testing::Test {
 public:
  InstalledAppControllerTest()
      : holder_(std::make_unique<DummyPageHolder>()),
        handle_scope_(GetScriptState()->GetIsolate()),
        context_(GetScriptState()->GetContext()),
        context_scope_(context_) {}

  Document& GetDocument() { return holder_->GetDocument(); }

  LocalFrame& GetFrame() { return holder_->GetFrame(); }

  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(&holder_->GetFrame());
  }

  void ResetContext() { holder_.reset(); }

 protected:
  void SetUp() override {
    url_test_helpers::RegisterMockedURLLoad(
        KURL("https://example.com/manifest.json"), "", "");
    GetFrame().Loader().CommitNavigation(
        WebNavigationParams::CreateWithEmptyHTMLForTesting(
            KURL("https://example.com")),
        nullptr /* extra_data */);
    test::RunPendingTasks();

    auto* link_manifest = MakeGarbageCollected<HTMLLinkElement>(
        GetDocument(), CreateElementFlags());
    link_manifest->setAttribute(blink::html_names::kRelAttr,
                                AtomicString("manifest"));
    GetDocument().head()->AppendChild(link_manifest);
    link_manifest->setAttribute(
        html_names::kHrefAttr,
        AtomicString("https://example.com/manifest.json"));

    ManifestManager::From(*GetFrame().DomWindow())->DidChangeManifest();
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> holder_;
  v8::HandleScope handle_scope_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

TEST_F(InstalledAppControllerTest, DestroyContextBeforeCallback) {
  auto* controller = InstalledAppController::From(*GetFrame().DomWindow());
  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<RelatedApplication>>>(GetScriptState());
  auto promise = resolver->Promise();
  controller->GetInstalledRelatedApps(
      std::make_unique<
          CallbackPromiseAdapter<IDLSequence<RelatedApplication>, void>>(
          resolver));

  ExecutionContext::From(GetScriptState())->NotifyContextDestroyed();

  test::RunPendingTasks();

  // Not to crash is enough.
}

}  // namespace blink
