// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_CORE_UNIT_TEST_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_CORE_UNIT_TEST_HELPER_H_

#include <gtest/gtest.h>
#include <memory>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/use_mock_scrollbar_settings.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class SingleChildLocalFrameClient final : public EmptyLocalFrameClient {
 public:
  static SingleChildLocalFrameClient* Create() {
    return new SingleChildLocalFrameClient();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(child_);
    EmptyLocalFrameClient::Trace(visitor);
  }

  // LocalFrameClient overrides:
  LocalFrame* FirstChild() const override { return child_.Get(); }
  LocalFrame* CreateFrame(const AtomicString& name,
                          HTMLFrameOwnerElement*) override;

  void DidDetachChild() { child_ = nullptr; }

 private:
  explicit SingleChildLocalFrameClient() = default;

  Member<LocalFrame> child_;
};

class LocalFrameClientWithParent final : public EmptyLocalFrameClient {
 public:
  static LocalFrameClientWithParent* Create(LocalFrame* parent) {
    return new LocalFrameClientWithParent(parent);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(parent_);
    EmptyLocalFrameClient::Trace(visitor);
  }

  // FrameClient overrides:
  void Detached(FrameDetachType) override;
  LocalFrame* Parent() const override { return parent_.Get(); }
  LocalFrame* Top() const override { return parent_.Get(); }

 private:
  explicit LocalFrameClientWithParent(LocalFrame* parent) : parent_(parent) {}

  Member<LocalFrame> parent_;
};

class RenderingTest : public PageTestBase, public UseMockScrollbarSettings {
  USING_FAST_MALLOC(RenderingTest);

 public:
  virtual FrameSettingOverrideFunction SettingOverrider() const {
    return nullptr;
  }
  virtual ChromeClient& GetChromeClient() const;

  explicit RenderingTest(LocalFrameClient* = nullptr);

  const Node* HitTest(int x, int y);
  HitTestResult::NodeSet RectBasedHitTest(LayoutRect rect);

 protected:
  void SetUp() override;
  void TearDown() override;

  LayoutView& GetLayoutView() const {
    return *GetDocument().View()->GetLayoutView();
  }

  LocalFrame& ChildFrame() {
    return *ToLocalFrame(GetFrame().Tree().FirstChild());
  }
  Document& ChildDocument() { return *ChildFrame().GetDocument(); }

  void SetChildFrameHTML(const String&);

  // Both enables compositing and runs the document lifecycle.
  void EnableCompositing() {
    // This Page is not actually being shown by a compositor, but we act like it
    // will in order to test behaviour.
    GetPage().GetSettings().SetAcceleratedCompositingEnabled(true);
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
    GetDocument().View()->UpdateAllLifecyclePhases();
  }

  LayoutObject* GetLayoutObjectByElementId(const char* id) const {
    const auto* element = GetElementById(id);
    return element ? element->GetLayoutObject() : nullptr;
  }

 private:
  Persistent<LocalFrameClient> local_frame_client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_CORE_UNIT_TEST_HELPER_H_
