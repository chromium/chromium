// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/mock_clipboard_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

class ChangeClipboardEventListener final : public NativeEventListener {
 public:
  explicit ChangeClipboardEventListener(
      mojom::blink::ClipboardHost* clipboard_host)
      : clipboard_host_(clipboard_host) {}

  void Invoke(ExecutionContext*, Event*) override {
    clipboard_host_->WriteText("changed");
    clipboard_host_->CommitWrite();
  }

 private:
  raw_ptr<mojom::blink::ClipboardHost> clipboard_host_;
};

class EventCountingListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override { ++event_count_; }
  int EventCount() const { return event_count_; }

 private:
  int event_count_ = 0;
};

void ExpectDefaultPasteResult(const AtomicString* clipboard_change_event_type,
                              const String& expected_html) {
  test::TaskEnvironment task_environment;
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(1, 1));
  LocalFrame& frame = page_holder->GetFrame();

  PageTestBase::MockClipboardHostProvider mock_clipboard_host_provider(
      frame.GetBrowserInterfaceBroker());
  mojom::blink::ClipboardHost* clipboard_host =
      mock_clipboard_host_provider.clipboard_host();

  HTMLElement* body = page_holder->GetDocument().body();
  body->setAttribute(html_names::kContenteditableAttr, keywords::kTrue);
  body->Focus();
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  frame.Selection().SetSelection(
      SelectionInDomTree::Builder().SelectAllChildren(*body).Build(),
      SetSelectionOptions());
  ASSERT_TRUE(
      frame.Selection().ComputeVisibleSelectionInDomTree().IsContentEditable());

  clipboard_host->WriteText("original");
  clipboard_host->CommitWrite();
  test::RunPendingTasks();

  if (clipboard_change_event_type) {
    body->addEventListener(
        *clipboard_change_event_type,
        MakeGarbageCollected<ChangeClipboardEventListener>(clipboard_host));
  }
  auto* before_input_listener = MakeGarbageCollected<EventCountingListener>();
  body->addEventListener(event_type_names::kBeforeinput, before_input_listener);

  frame.GetEditor().ExecuteCommand("Paste");

  EXPECT_EQ(before_input_listener->EventCount(), 1);
  EXPECT_EQ(body->GetInnerHTMLString(), expected_html);
}

}  // namespace

// Paste in kSelection mode (the state ExecutePasteGlobalSelection sets on
// middle-click) must not leak an image planted on the kStandard buffer
// through GetFragmentFromClipboard's image fallback.
TEST(ClipboardCommandsPasteTest, PasteInSelectionModeDoesNotLeakStandardImage) {
  ScopedClipboardPasteImageRespectBufferForTest scoped_feature(true);
  test::TaskEnvironment task_environment;
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(1, 1));
  LocalFrame& frame = page_holder->GetFrame();

  PageTestBase::MockClipboardHostProvider mock_clipboard_host_provider(
      frame.GetBrowserInterfaceBroker());

  HTMLElement* body = page_holder->GetDocument().body();
  body->setAttribute(html_names::kContenteditableAttr, keywords::kTrue);
  body->Focus();
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  frame.Selection().SetSelection(
      SelectionInDomTree::Builder().SelectAllChildren(*body).Build(),
      SetSelectionOptions());
  ASSERT_TRUE(
      frame.Selection().ComputeVisibleSelectionInDomTree().IsContentEditable());

  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocPixelsFlags(
      SkImageInfo::Make(4, 3, kN32_SkColorType, kOpaque_SkAlphaType), 0));
  mojom::blink::ClipboardHost* clipboard_host =
      mock_clipboard_host_provider.clipboard_host();
  clipboard_host->WriteImage(bitmap);
  clipboard_host->CommitWrite();
  test::RunPendingTasks();

  frame.GetSystemClipboard()->SetSelectionMode(true);
  frame.GetEditor().ExecuteCommand("Paste");
  frame.GetSystemClipboard()->SetSelectionMode(false);

  const String html = body->GetInnerHTMLString();
  EXPECT_FALSE(html.contains("data:image/png"))
      << "Image fallback leaked kStandard PNG while buffer_ was kSelection: "
      << html.Utf8();
}

TEST(ClipboardCommandsPasteTest,
     DefaultPasteWithoutClipboardChangePastesClipboardContent) {
  ExpectDefaultPasteResult(nullptr, "original");
}

TEST(ClipboardCommandsPasteTest,
     ForeignClipboardChangeDuringPastePreventsDefaultPaste) {
  ExpectDefaultPasteResult(&event_type_names::kPaste, "");
}

TEST(ClipboardCommandsPasteTest,
     ForeignClipboardChangeDuringBeforeInputPreventsDefaultPaste) {
  ExpectDefaultPasteResult(&event_type_names::kBeforeinput, "");
}

}  // namespace blink
