/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_plugin_container.h"

#include <memory>
#include <string>

#include "build/build_config.h"
#include "cc/layers/layer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/fake_web_plugin.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/scoped_fake_plugin_registry.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

using blink::test::RunPendingTasks;

namespace blink {

class WebPluginContainerTest : public PageTestBase {
 public:
  WebPluginContainerTest() : base_url_("http://www.test.com/") {}

  void SetUp() override {
    PageTestBase::SetUp();
    mock_clipboard_host_provider_.Install(
        GetFrame().GetBrowserInterfaceBroker());
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
    PageTestBase::TearDown();
  }

  void CalculateGeometry(WebPluginContainerImpl* plugin_container_impl,
                         gfx::Rect& window_rect,
                         gfx::Rect& clip_rect,
                         gfx::Rect& unobscured_rect) {
    plugin_container_impl->CalculateGeometry(window_rect, clip_rect,
                                             unobscured_rect);
  }

  void RegisterMockedURL(
      const std::string& file_name,
      const std::string& mime_type = std::string("text/html")) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via the WebViewHelper in each test case.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name), WebString::FromUTF8(mime_type));
  }

  void UpdateAllLifecyclePhases(WebViewImpl* web_view) {
    web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

 protected:
  ScopedFakePluginRegistry fake_plugins_;
  std::string base_url_;

 private:
  PageTestBase::MockClipboardHostProvider mock_clipboard_host_provider_;
};

namespace {

#if BUILDFLAG(IS_MAC)
const WebInputEvent::Modifiers kEditingModifier = WebInputEvent::kMetaKey;
#else
const WebInputEvent::Modifiers kEditingModifier = WebInputEvent::kControlKey;
#endif

template <typename T>
class CustomPluginWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  WebPlugin* CreatePlugin(const WebPluginParams& params) override {
    return new T(params);
  }
};

class TestPluginWebFrameClient;

// Subclass of FakeWebPlugin that has a selection of 'x' as plain text and 'y'
// as markup text.
class TestPlugin : public FakeWebPlugin {
 public:
  TestPlugin(const WebPluginParams& params,
             TestPluginWebFrameClient* test_client)
      : FakeWebPlugin(params), test_client_(test_client) {}

  bool HasSelection() const override { return true; }
  WebString SelectionAsText() const override { return WebString("x"); }
  WebString SelectionAsMarkup() const override { return WebString("y"); }
  bool CanCopy() const override;
  bool SupportsPaginatedPrint() override { return true; }
  int PrintBegin(const WebPrintParams& print_params) override { return 1; }
  void PrintPage(int page_index, cc::PaintCanvas* canvas) override;

 private:
  ~TestPlugin() override = default;

  TestPluginWebFrameClient* const test_client_;
};

// Subclass of FakeWebPlugin used for testing edit commands, so HasSelection()
// and CanEditText() return true by default.
class TestPluginWithEditableText : public FakeWebPlugin {
 public:
  static TestPluginWithEditableText* FromContainer(WebElement* element) {
    WebPlugin* plugin =
        To<WebPluginContainerImpl>(element->PluginContainer())->Plugin();
    return static_cast<TestPluginWithEditableText*>(plugin);
  }

  explicit TestPluginWithEditableText(const WebPluginParams& params)
      : FakeWebPlugin(params), cut_called_(false), paste_called_(false) {}

  bool HasSelection() const override { return true; }
  bool CanEditText() const override { return true; }
  bool ExecuteEditCommand(const WebString& name,
                          const WebString& value) override {
    if (name == "Cut") {
      cut_called_ = true;
      return true;
    }
    if (name == "Paste" || name == "PasteAndMatchStyle") {
      paste_called_ = true;
      return true;
    }
    return false;
  }

  bool IsCutCalled() const { return cut_called_; }
  bool IsPasteCalled() const { return paste_called_; }
  void ResetEditCommandState() {
    cut_called_ = false;
    paste_called_ = false;
  }

 private:
  ~TestPluginWithEditableText() override = default;

  bool cut_called_;
  bool paste_called_;
};

class TestPluginWebFrameClient : public frame_test_helpers::TestWebFrameClient {
  WebLocalFrame* CreateChildFrame(
      mojom::blink::TreeScopeType scope,
      const WebString& name,
      const WebString& fallback_name,
      const FramePolicy&,
      const WebFrameOwnerProperties&,
      FrameOwnerElementType owner_type,
      WebPolicyContainerBindParams policy_container_bind_params,
      ukm::SourceId document_ukm_source_id,
      FinishChildFrameCreationFn finish_creation) override {
    return CreateLocalChild(
        *Frame(), scope, std::make_unique<TestPluginWebFrameClient>(),
        std::move(policy_container_bind_params), finish_creation);
  }

  WebPlugin* CreatePlugin(const WebPluginParams& params) override {
    if (params.mime_type == "application/x-webkit-test-webplugin" ||
        params.mime_type == "application/pdf") {
      if (has_editable_text_)
        return new TestPluginWithEditableText(params);

      return new TestPlugin(params, this);
    }
    return WebLocalFrameClient::CreatePlugin(params);
  }

 public:
  TestPluginWebFrameClient() = default;

  void OnPrintPage() { printed_page_ = true; }
  bool PrintedAtLeastOnePage() const { return printed_page_; }
  void SetHasEditableText(bool has_editable_text) {
    has_editable_text_ = has_editable_text;
  }
  void SetCanCopy(bool can_copy) { can_copy_ = can_copy; }
  bool CanCopy() const { return can_copy_; }

 private:
  bool printed_page_ = false;
  bool has_editable_text_ = false;
  bool can_copy_ = true;
};

bool TestPlugin::CanCopy() const {
  DCHECK(test_client_);
  return test_client_->CanCopy();
}

void TestPlugin::PrintPage(int page_index, cc::PaintCanvas* canvas) {
  DCHECK(test_client_);
  test_client_->OnPrintPage();
}

void EnablePlugins(WebView* web_view, const gfx::Size& size) {
  DCHECK(web_view);
  web_view->GetSettings()->SetPluginsEnabled(true);
  web_view->MainFrameWidget()->Resize(size);
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);
  RunPendingTasks();
}

WebPluginContainer* GetWebPluginContainer(WebViewImpl* web_view,
                                          const WebString& id) {
  WebElement element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(id);
  return element.PluginContainer();
}

String ReadClipboard(LocalFrame& frame) {
  // Run all tasks in a message loop to allow asynchronous clipboard writing
  // to happen before reading from it synchronously.
  test::RunPendingTasks();
  return frame.GetSystemClipboard()->ReadPlainText();
}

void ClearClipboardBuffer(LocalFrame& frame) {
  frame.GetSystemClipboard()->WritePlainText(String(""));
  frame.GetSystemClipboard()->CommitWrite();
  EXPECT_EQ(String(""), ReadClipboard(frame));
}

void CreateAndHandleKeyboardEvent(WebElement* plugin_container_one_element,
                                  WebInputEvent::Modifiers modifier_key,
                                  int key_code) {
  WebKeyboardEvent web_keyboard_event(
      WebInputEvent::Type::kRawKeyDown, modifier_key,
      WebInputEvent::GetStaticTimeStampForTests());
  web_keyboard_event.windows_key_code = key_code;
  KeyboardEvent* key_event = KeyboardEvent::Create(web_keyboard_event, nullptr);
  To<WebPluginContainerImpl>(plugin_container_one_element->PluginContainer())
      ->HandleEvent(*key_event);
}

void ExecuteContextMenuCommand(WebViewImpl* web_view,
                               const WebString& command_name) {
  auto event = frame_test_helpers::CreateMouseEvent(
      WebMouseEvent::Type::kMouseDown, WebMouseEvent::Button::kRight,
      gfx::Point(30, 30), 0);
  event.click_count = 1;

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  EXPECT_TRUE(
      web_view->MainFrame()->ToWebLocalFrame()->ExecuteCommand(command_name));
}

}  // namespace

TEST_F(WebPluginContainerTest, WindowToLocalPointTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebPluginContainer* plugin_container_one =
      GetWebPluginContainer(web_view, WebString::FromUTF8("translated-plugin"));
  DCHECK(plugin_container_one);
  gfx::Point point1 =
      plugin_container_one->RootFrameToLocalPoint(gfx::Point(10, 10));
  ASSERT_EQ(0, point1.x());
  ASSERT_EQ(0, point1.y());
  gfx::Point point2 =
      plugin_container_one->RootFrameToLocalPoint(gfx::Point(100, 100));
  ASSERT_EQ(90, point2.x());
  ASSERT_EQ(90, point2.y());

  WebPluginContainer* plugin_container_two =
      GetWebPluginContainer(web_view, WebString::FromUTF8("rotated-plugin"));
  DCHECK(plugin_container_two);
  gfx::Point point3 =
      plugin_container_two->RootFrameToLocalPoint(gfx::Point(0, 10));
  ASSERT_EQ(10, point3.x());
  ASSERT_EQ(0, point3.y());
  gfx::Point point4 =
      plugin_container_two->RootFrameToLocalPoint(gfx::Point(-10, 10));
  ASSERT_EQ(10, point4.x());
  ASSERT_EQ(10, point4.y());
}

TEST_F(WebPluginContainerTest, LocalToWindowPointTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebPluginContainer* plugin_container_one =
      GetWebPluginContainer(web_view, WebString::FromUTF8("translated-plugin"));
  DCHECK(plugin_container_one);
  gfx::Point point1 =
      plugin_container_one->LocalToRootFramePoint(gfx::Point(0, 0));
  ASSERT_EQ(10, point1.x());
  ASSERT_EQ(10, point1.y());
  gfx::Point point2 =
      plugin_container_one->LocalToRootFramePoint(gfx::Point(90, 90));
  ASSERT_EQ(100, point2.x());
  ASSERT_EQ(100, point2.y());

  WebPluginContainer* plugin_container_two =
      GetWebPluginContainer(web_view, WebString::FromUTF8("rotated-plugin"));
  DCHECK(plugin_container_two);
  gfx::Point point3 =
      plugin_container_two->LocalToRootFramePoint(gfx::Point(10, 0));
  ASSERT_EQ(0, point3.x());
  ASSERT_EQ(10, point3.y());
  gfx::Point point4 =
      plugin_container_two->LocalToRootFramePoint(gfx::Point(10, 10));
  ASSERT_EQ(-10, point4.x());
  ASSERT_EQ(10, point4.y());
}

// Verifies executing the command 'Copy' results in copying to the clipboard.
TEST_F(WebPluginContainerTest, Copy) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive `web_view_helper`.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  web_view->MainFrameImpl()
      ->GetDocument()
      .Unwrap<Document>()
      ->body()
      ->getElementById(AtomicString("translated-plugin"))
      ->Focus();
  EXPECT_TRUE(web_view->MainFrame()->ToWebLocalFrame()->ExecuteCommand("Copy"));

  LocalFrame* local_frame = web_view->MainFrameImpl()->GetFrame();
  EXPECT_EQ(String("x"), ReadClipboard(*local_frame));
  ClearClipboardBuffer(*local_frame);
}

// Verifies executing the command 'Copy' results in copying nothing to the
// clipboard when the plugin does not have the copy permission.
TEST_F(WebPluginContainerTest, CopyWithoutPermission) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive `web_view_helper`.
  TestPluginWebFrameClient plugin_web_frame_client;
  // Make sure to create a plugin without the copy permission.
  plugin_web_frame_client.SetCanCopy(false);
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  web_view->MainFrameImpl()
      ->GetDocument()
      .Unwrap<Document>()
      ->body()
      ->getElementById(AtomicString("translated-plugin"))
      ->Focus();
  EXPECT_TRUE(web_view->MainFrame()->ToWebLocalFrame()->ExecuteCommand("Copy"));

  LocalFrame* local_frame = web_view->MainFrameImpl()->GetFrame();
  EXPECT_EQ(String(""), ReadClipboard(*local_frame));
  ClearClipboardBuffer(*local_frame);
}

TEST_F(WebPluginContainerTest, CopyFromContextMenu) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive `web_view_helper`.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  // Make sure the right-click + command works in common scenario.
  ExecuteContextMenuCommand(web_view, "Copy");

  LocalFrame* local_frame = web_view->MainFrameImpl()->GetFrame();
  EXPECT_EQ(String("x"), ReadClipboard(*local_frame));
  ClearClipboardBuffer(*local_frame);

  auto event = frame_test_helpers::CreateMouseEvent(
      WebMouseEvent::Type::kMouseDown, WebMouseEvent::Button::kRight,
      gfx::Point(30, 30), 0);
  event.click_count = 1;

  // Now, let's try a more complex scenario:
  // 1) open the context menu. This will focus the plugin.
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  // 2) document blurs the plugin, because it can.
  web_view->FocusedElement()->blur();
  // 3) Copy should still operate on the context node, even though the focus had
  //    shifted.
  EXPECT_TRUE(web_view->MainFrameImpl()->ExecuteCommand("Copy"));

  EXPECT_EQ(String("x"), ReadClipboard(*local_frame));
  ClearClipboardBuffer(*local_frame);
}

TEST_F(WebPluginContainerTest, CopyFromContextMenuWithoutCopyPermission) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive `web_view_helper`.
  TestPluginWebFrameClient plugin_web_frame_client;
  // Make sure to create a plugin without the copy permission.
  plugin_web_frame_client.SetCanCopy(false);
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  // Make sure the right-click + command copies nothing in common scenario.
  ExecuteContextMenuCommand(web_view, "Copy");
  LocalFrame* local_frame = web_view->MainFrameImpl()->GetFrame();
  EXPECT_EQ(String(""), ReadClipboard(*local_frame));
  ClearClipboardBuffer(*local_frame);

  auto event = frame_test_helpers::CreateMouseEvent(
      WebMouseEvent::Type::kMouseDown, WebMouseEvent::Button::kRight,
      gfx::Point(30, 30), 0);
  event.click_count = 1;

  // Now, make sure the context menu copies nothing in a more complex scenario.
  // 1) open the context menu. This will focus the plugin.
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  // 2) document blurs the plugin, because it can.
  web_view->FocusedElement()->blur();
  // 3) Copy should still operate on the context node, even though the focus had
  //    shifted.
  EXPECT_TRUE(web_view->MainFrameImpl()->ExecuteCommand("Copy"));
  EXPECT_EQ(String(""), ReadClipboard(*local_frame));
  ClearClipboardBuffer(*local_frame);
}

// Verifies `Ctrl-C` and `Ctrl-Insert` keyboard events, results in copying to
// the clipboard.
TEST_F(WebPluginContainerTest, CopyInsertKeyboardEventsTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive `web_view_helper`.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));
  WebInputEvent::Modifiers modifier_key = static_cast<WebInputEvent::Modifiers>(
      kEditingModifier | WebInputEvent::kNumLockOn | WebInputEvent::kIsLeft);
  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_C);
  LocalFrame* local_frame = web_view->MainFrameImpl()->GetFrame();
  EXPECT_EQ(String("x"), ReadClipboard(*local_frame));
  ClearClipboardBuffer(*local_frame);

  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_INSERT);
  EXPECT_EQ(String("x"), ReadClipboard(*local_frame));
  ClearClipboardBuffer(*local_frame);
}

// Verifies `Ctrl-C` and `Ctrl-Insert` keyboard events, results in copying
// nothing to the clipboard.
TEST_F(WebPluginContainerTest,
       CopyInsertKeyboardEventsTestWithoutCopyPermission) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive `web_view_helper`.
  TestPluginWebFrameClient plugin_web_frame_client;
  // Make sure to create a plugin without the copy permission.
  plugin_web_frame_client.SetCanCopy(false);
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));
  WebInputEvent::Modifiers modifier_key = static_cast<WebInputEvent::Modifiers>(
      kEditingModifier | WebInputEvent::kNumLockOn | WebInputEvent::kIsLeft);
  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_C);
  LocalFrame* local_frame = web_view->MainFrameImpl()->GetFrame();
  EXPECT_EQ(String(""), ReadClipboard(*local_frame));
  ClearClipboardBuffer(*local_frame);

  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_INSERT);
  EXPECT_EQ(String(""), ReadClipboard(*local_frame));
  ClearClipboardBuffer(*local_frame);
}

// Verifies |Ctrl-X| and |Shift-Delete| keyboard events, results in the "Cut"
// command being invoked.
TEST_F(WebPluginContainerTest, CutDeleteKeyboardEventsTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "Cut".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  WebInputEvent::Modifiers modifier_key = static_cast<WebInputEvent::Modifiers>(
      kEditingModifier | WebInputEvent::kNumLockOn | WebInputEvent::kIsLeft);
  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_X);

  // Check that "Cut" command is invoked.
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsCutCalled());

  // Reset Cut status for next time.
  test_plugin->ResetEditCommandState();

  modifier_key = static_cast<WebInputEvent::Modifiers>(
      WebInputEvent::kShiftKey | WebInputEvent::kNumLockOn |
      WebInputEvent::kIsLeft);

  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_DELETE);

  // Check that "Cut" command is invoked.
  EXPECT_TRUE(test_plugin->IsCutCalled());
}

// Verifies |Ctrl-V| and |Shift-Insert| keyboard events, results in the "Paste"
// command being invoked.
TEST_F(WebPluginContainerTest, PasteInsertKeyboardEventsTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "Paste".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  WebInputEvent::Modifiers modifier_key = static_cast<WebInputEvent::Modifiers>(
      kEditingModifier | WebInputEvent::kNumLockOn | WebInputEvent::kIsLeft);
  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_V);

  // Check that "Paste" command is invoked.
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsPasteCalled());

  // Reset Paste status for next time.
  test_plugin->ResetEditCommandState();

  modifier_key = static_cast<WebInputEvent::Modifiers>(
      WebInputEvent::kShiftKey | WebInputEvent::kNumLockOn |
      WebInputEvent::kIsLeft);

  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_INSERT);

  // Check that "Paste" command is invoked.
  EXPECT_TRUE(test_plugin->IsPasteCalled());
}

// Verifies |Ctrl-Shift-V| keyboard event results in the "PasteAndMatchStyle"
// command being invoked.
TEST_F(WebPluginContainerTest, PasteAndMatchStyleKeyboardEventsTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "PasteAndMatchStyle".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  WebInputEvent::Modifiers modifier_key = static_cast<WebInputEvent::Modifiers>(
      kEditingModifier | WebInputEvent::kShiftKey | WebInputEvent::kNumLockOn |
      WebInputEvent::kIsLeft);
  CreateAndHandleKeyboardEvent(&plugin_container_one_element, modifier_key,
                               VKEY_V);

  // Check that "PasteAndMatchStyle" command is invoked.
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsPasteCalled());
}

TEST_F(WebPluginContainerTest, CutFromContextMenu) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "Cut".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  ExecuteContextMenuCommand(web_view, "Cut");
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsCutCalled());
}

TEST_F(WebPluginContainerTest, PasteFromContextMenu) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "Paste".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  ExecuteContextMenuCommand(web_view, "Paste");
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsPasteCalled());
}

TEST_F(WebPluginContainerTest, PasteAndMatchStyleFromContextMenu) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;

  // Use TestPluginWithEditableText for testing "Paste".
  plugin_web_frame_client.SetHasEditableText(true);

  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));

  ExecuteContextMenuCommand(web_view, "PasteAndMatchStyle");
  auto* test_plugin =
      TestPluginWithEditableText::FromContainer(&plugin_container_one_element);
  EXPECT_TRUE(test_plugin->IsPasteCalled());
}

// A class to facilitate testing that events are correctly received by plugins.
class EventTestPlugin : public FakeWebPlugin {
 public:
  explicit EventTestPlugin(const WebPluginParams& params)
      : FakeWebPlugin(params),
        last_event_type_(WebInputEvent::Type::kUndefined),
        last_event_modifiers_(WebInputEvent::kNoModifiers) {}

  WebInputEventResult HandleInputEvent(
      const WebCoalescedInputEvent& coalesced_event,
      ui::Cursor*) override {
    const WebInputEvent& event = coalesced_event.Event();
    coalesced_event_count_ = coalesced_event.CoalescedEventSize();
    last_event_type_ = event.GetType();
    last_event_modifiers_ = event.GetModifiers();
    if (WebInputEvent::IsMouseEventType(event.GetType()) ||
        event.GetType() == WebInputEvent::Type::kMouseWheel) {
      const WebMouseEvent& mouse_event =
          static_cast<const WebMouseEvent&>(event);
      last_event_location_ = gfx::Point(mouse_event.PositionInWidget().x(),
                                        mouse_event.PositionInWidget().y());
    } else if (WebInputEvent::IsTouchEventType(event.GetType())) {
      const WebTouchEvent& touch_event =
          static_cast<const WebTouchEvent&>(event);
      if (touch_event.touches_length == 1) {
        last_event_location_ =
            gfx::Point(touch_event.touches[0].PositionInWidget().x(),
                       touch_event.touches[0].PositionInWidget().y());
      } else {
        last_event_location_ = gfx::Point();
      }
    }

    return WebInputEventResult::kHandledSystem;
  }
  WebInputEvent::Type GetLastInputEventType() { return last_event_type_; }

  gfx::Point GetLastEventLocation() { return last_event_location_; }

  int GetLastEventModifiers() { return last_event_modifiers_; }

  void ClearLastEventType() {
    last_event_type_ = WebInputEvent::Type::kUndefined;
  }

  size_t GetCoalescedEventCount() { return coalesced_event_count_; }

 private:
  ~EventTestPlugin() override = default;

  size_t coalesced_event_count_;
  WebInputEvent::Type last_event_type_;
  gfx::Point last_event_location_;
  int last_event_modifiers_;
};

TEST_F(WebPluginContainerTest, GestureLongPressReachesPlugin) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebGestureEvent event(WebInputEvent::Type::kGestureLongPress,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests(),
                        WebGestureDevice::kTouchscreen);

  // First, send an event that doesn't hit the plugin to verify that the
  // plugin doesn't receive it.
  event.SetPositionInWidget(gfx::PointF());

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::Type::kUndefined,
            test_plugin->GetLastInputEventType());

  // Next, send an event that does hit the plugin, and verify it does receive
  // it.
  gfx::Rect rect = plugin_container_one_element.BoundsInWidget();
  event.SetPositionInWidget(
      gfx::PointF(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2));

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::Type::kGestureLongPress,
            test_plugin->GetLastInputEventType());
}

TEST_F(WebPluginContainerTest, MouseEventButtons) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseEvent event = frame_test_helpers::CreateMouseEvent(
      WebMouseEvent::Type::kMouseMove, WebMouseEvent::Button::kNoButton,
      gfx::Point(30, 30),
      WebInputEvent::kMiddleButtonDown | WebInputEvent::kShiftKey);

  gfx::Rect rect = plugin_container_one_element.BoundsInWidget();
  event.SetPositionInWidget(rect.x() + rect.width() / 2,
                            rect.y() + rect.height() / 2);

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::Type::kMouseMove,
            test_plugin->GetLastInputEventType());
  EXPECT_EQ(WebInputEvent::kMiddleButtonDown | WebInputEvent::kShiftKey,
            test_plugin->GetLastEventModifiers());
}

TEST_F(WebPluginContainerTest, MouseWheelEventTranslated) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseWheelEvent event(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests());

  gfx::Rect rect = plugin_container_one_element.BoundsInWidget();
  event.SetPositionInWidget(rect.x() + rect.width() / 2,
                            rect.y() + rect.height() / 2);

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::Type::kMouseWheel,
            test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width() / 2, test_plugin->GetLastEventLocation().x());
  EXPECT_EQ(rect.height() / 2, test_plugin->GetLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, TouchEventScrolled) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));
  web_view->SmoothScroll(0, 200, base::TimeDelta());
  UpdateAllLifecyclePhases(web_view);
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  gfx::Rect rect = plugin_container_one_element.BoundsInWidget();
  WebPointerEvent event(
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                           WebPointerProperties::Button::kLeft,
                           gfx::PointF(rect.x() + rect.width() / 2,
                                       rect.y() + rect.height() / 2),
                           gfx::PointF(rect.x() + rect.width() / 2,
                                       rect.y() + rect.height() / 2)),
      1.0f, 1.0f);

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  web_view->MainFrameWidget()->DispatchBufferedTouchEvents();
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::Type::kTouchStart,
            test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width() / 2, test_plugin->GetLastEventLocation().x());
  EXPECT_EQ(rect.height() / 2, test_plugin->GetLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, TouchEventScrolledWithCoalescedTouches) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));
  web_view->SmoothScroll(0, 200, base::TimeDelta());
  UpdateAllLifecyclePhases(web_view);
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRawLowLatency);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  {
    gfx::Rect rect = plugin_container_one_element.BoundsInWidget();
    WebPointerEvent event(
        WebInputEvent::Type::kPointerDown,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             gfx::PointF(rect.x() + rect.width() / 2,
                                         rect.y() + rect.height() / 2),
                             gfx::PointF(rect.x() + rect.width() / 2,
                                         rect.y() + rect.height() / 2)),
        1.0f, 1.0f);

    WebCoalescedInputEvent coalesced_event(event, ui::LatencyInfo());

    web_view->MainFrameWidget()->HandleInputEvent(coalesced_event);
    web_view->MainFrameWidget()->DispatchBufferedTouchEvents();
    RunPendingTasks();

    EXPECT_EQ(static_cast<const size_t>(1),
              test_plugin->GetCoalescedEventCount());
    EXPECT_EQ(WebInputEvent::Type::kTouchStart,
              test_plugin->GetLastInputEventType());
    EXPECT_EQ(rect.width() / 2, test_plugin->GetLastEventLocation().x());
    EXPECT_EQ(rect.height() / 2, test_plugin->GetLastEventLocation().y());
  }

  {
    gfx::Rect rect = plugin_container_one_element.BoundsInWidget();
    WebPointerEvent event1(
        WebInputEvent::Type::kPointerMove,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             gfx::PointF(rect.x() + rect.width() / 2 + 1,
                                         rect.y() + rect.height() / 2 + 1),
                             gfx::PointF(rect.x() + rect.width() / 2 + 1,
                                         rect.y() + rect.height() / 2 + 1)),
        1.0f, 1.0f);

    WebCoalescedInputEvent coalesced_event(event1, ui::LatencyInfo());

    WebPointerEvent event2(
        WebInputEvent::Type::kPointerMove,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             gfx::PointF(rect.x() + rect.width() / 2 + 2,
                                         rect.y() + rect.height() / 2 + 2),
                             gfx::PointF(rect.x() + rect.width() / 2 + 2,
                                         rect.y() + rect.height() / 2 + 2)),
        1.0f, 1.0f);
    WebPointerEvent event3(
        WebInputEvent::Type::kPointerMove,
        WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                             WebPointerProperties::Button::kLeft,
                             gfx::PointF(rect.x() + rect.width() / 2 + 3,
                                         rect.y() + rect.height() / 2 + 3),
                             gfx::PointF(rect.x() + rect.width() / 2 + 3,
                                         rect.y() + rect.height() / 2 + 3)),
        1.0f, 1.0f);

    coalesced_event.AddCoalescedEvent(event2);
    coalesced_event.AddCoalescedEvent(event3);

    web_view->MainFrameWidget()->HandleInputEvent(coalesced_event);
    web_view->MainFrameWidget()->DispatchBufferedTouchEvents();
    RunPendingTasks();

    EXPECT_EQ(static_cast<const size_t>(3),
              test_plugin->GetCoalescedEventCount());
    EXPECT_EQ(WebInputEvent::Type::kTouchMove,
              test_plugin->GetLastInputEventType());
    EXPECT_EQ(rect.width() / 2 + 1, test_plugin->GetLastEventLocation().x());
    EXPECT_EQ(rect.height() / 2 + 1, test_plugin->GetLastEventLocation().y());
  }
}

TEST_F(WebPluginContainerTest, MouseWheelEventScrolled) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));
  web_view->SmoothScroll(0, 200, base::TimeDelta());
  UpdateAllLifecyclePhases(web_view);
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseWheelEvent event(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests());

  gfx::Rect rect = plugin_container_one_element.BoundsInWidget();
  event.SetPositionInWidget(rect.x() + rect.width() / 2,
                            rect.y() + rect.height() / 2);

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::Type::kMouseWheel,
            test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width() / 2, test_plugin->GetLastEventLocation().x());
  EXPECT_EQ(rect.height() / 2, test_plugin->GetLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, MouseEventScrolled) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));
  web_view->SmoothScroll(0, 200, base::TimeDelta());
  UpdateAllLifecyclePhases(web_view);
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseEvent event(WebInputEvent::Type::kMouseMove,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  gfx::Rect rect = plugin_container_one_element.BoundsInWidget();
  event.SetPositionInWidget(rect.x() + rect.width() / 2,
                            rect.y() + rect.height() / 2);

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  RunPendingTasks();

  EXPECT_EQ(WebInputEvent::Type::kMouseMove,
            test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width() / 2, test_plugin->GetLastEventLocation().x());
  EXPECT_EQ(rect.height() / 2, test_plugin->GetLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, MouseEventZoomed) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->GetSettings()->SetPluginsEnabled(true);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(300, 300));
  web_view->SetPageScaleFactor(2);
  web_view->SmoothScroll(0, 300, base::TimeDelta());
  UpdateAllLifecyclePhases(web_view);
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseEvent event(WebInputEvent::Type::kMouseMove,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  gfx::Rect rect = plugin_container_one_element.BoundsInWidget();
  event.SetPositionInWidget(rect.x() + rect.width() / 2,
                            rect.y() + rect.height() / 2);

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  RunPendingTasks();

  // rect.width/height divided by 4 because the rect is in viewport bounds and
  // there is a scale of 2 set.
  EXPECT_EQ(WebInputEvent::Type::kMouseMove,
            test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width() / 4, test_plugin->GetLastEventLocation().x());
  EXPECT_EQ(rect.height() / 4, test_plugin->GetLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, MouseWheelEventZoomed) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->GetSettings()->SetPluginsEnabled(true);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(300, 300));
  web_view->SetPageScaleFactor(2);
  web_view->SmoothScroll(0, 300, base::TimeDelta());
  UpdateAllLifecyclePhases(web_view);
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  WebMouseWheelEvent event(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests());

  gfx::Rect rect = plugin_container_one_element.BoundsInWidget();
  event.SetPositionInWidget(rect.x() + rect.width() / 2,
                            rect.y() + rect.height() / 2);

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  RunPendingTasks();

  // rect.width/height divided by 4 because the rect is in viewport bounds and
  // there is a scale of 2 set.
  EXPECT_EQ(WebInputEvent::Type::kMouseWheel,
            test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width() / 4, test_plugin->GetLastEventLocation().x());
  EXPECT_EQ(rect.height() / 4, test_plugin->GetLastEventLocation().y());
}

TEST_F(WebPluginContainerTest, TouchEventZoomed) {
  RegisterMockedURL("plugin_scroll.html");
  // Must outlive |web_view_helper|.
  CustomPluginWebFrameClient<EventTestPlugin> plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_scroll.html", &plugin_web_frame_client);
  DCHECK(web_view);
  web_view->GetSettings()->SetPluginsEnabled(true);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(300, 300));
  web_view->SetPageScaleFactor(2);
  web_view->SmoothScroll(0, 300, base::TimeDelta());
  UpdateAllLifecyclePhases(web_view);
  RunPendingTasks();

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("scrolled-plugin"));
  plugin_container_one_element.PluginContainer()->RequestTouchEventType(
      WebPluginContainer::kTouchEventRequestTypeRaw);
  WebPlugin* plugin = static_cast<WebPluginContainerImpl*>(
                          plugin_container_one_element.PluginContainer())
                          ->Plugin();
  EventTestPlugin* test_plugin = static_cast<EventTestPlugin*>(plugin);

  gfx::Rect rect = plugin_container_one_element.BoundsInWidget();
  WebPointerEvent event(
      WebInputEvent::Type::kPointerDown,
      WebPointerProperties(1, WebPointerProperties::PointerType::kTouch,
                           WebPointerProperties::Button::kLeft,
                           gfx::PointF(rect.x() + rect.width() / 2,
                                       rect.y() + rect.height() / 2),
                           gfx::PointF(rect.x() + rect.width() / 2,
                                       rect.y() + rect.height() / 2)),
      1.0f, 1.0f);

  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(event, ui::LatencyInfo()));
  web_view->MainFrameWidget()->DispatchBufferedTouchEvents();
  RunPendingTasks();

  // rect.width/height divided by 4 because the rect is in viewport bounds and
  // there is a scale of 2 set.
  EXPECT_EQ(WebInputEvent::Type::kTouchStart,
            test_plugin->GetLastInputEventType());
  EXPECT_EQ(rect.width() / 4, test_plugin->GetLastEventLocation().x());
  EXPECT_EQ(rect.height() / 4, test_plugin->GetLastEventLocation().y());
}

// Verify that isRectTopmost returns false when the document is detached.
TEST_F(WebPluginContainerTest, IsRectTopmostTest) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  auto* plugin_container_impl =
      To<WebPluginContainerImpl>(GetWebPluginContainer(
          web_view, WebString::FromUTF8("translated-plugin")));
  plugin_container_impl->SetFrameRect(gfx::Rect(0, 0, 300, 300));

  gfx::Rect rect = plugin_container_impl->GetElement().BoundsInWidget();
  EXPECT_TRUE(plugin_container_impl->IsRectTopmost(rect));

  // Cause the plugin's frame to be detached.
  web_view_helper.Reset();

  EXPECT_FALSE(plugin_container_impl->IsRectTopmost(rect));
}

// Verify that IsRectTopmost works with odd and even dimensions.
TEST_F(WebPluginContainerTest, IsRectTopmostTestWithOddAndEvenDimensions) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  auto* even_plugin_container_impl =
      To<WebPluginContainerImpl>(GetWebPluginContainer(
          web_view, WebString::FromUTF8("translated-plugin")));
  even_plugin_container_impl->SetFrameRect(gfx::Rect(0, 0, 300, 300));
  auto even_rect = even_plugin_container_impl->GetElement().BoundsInWidget();
  EXPECT_TRUE(even_plugin_container_impl->IsRectTopmost(even_rect));

  auto* odd_plugin_container_impl =
      To<WebPluginContainerImpl>(GetWebPluginContainer(
          web_view, WebString::FromUTF8("odd-dimensions-plugin")));
  odd_plugin_container_impl->SetFrameRect(gfx::Rect(0, 0, 300, 300));
  auto odd_rect = odd_plugin_container_impl->GetElement().BoundsInWidget();
  EXPECT_TRUE(odd_plugin_container_impl->IsRectTopmost(odd_rect));
}

TEST_F(WebPluginContainerTest, ClippedRectsForIframedElement) {
  RegisterMockedURL("plugin_container.html");
  RegisterMockedURL("plugin_containing_page.html");

  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebView* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_containing_page.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_element = web_view->MainFrame()
                                  ->FirstChild()
                                  ->ToWebLocalFrame()
                                  ->GetDocument()
                                  .GetElementById("translated-plugin");
  auto* plugin_container_impl =
      To<WebPluginContainerImpl>(plugin_element.PluginContainer());

  DCHECK(plugin_container_impl);

  gfx::Rect window_rect, clip_rect, unobscured_rect;
  CalculateGeometry(plugin_container_impl, window_rect, clip_rect,
                    unobscured_rect);
  EXPECT_EQ(gfx::Rect(20, 220, 40, 40), window_rect);
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40), clip_rect);
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40), unobscured_rect);

  // Cause the plugin's frame to be detached.
  web_view_helper.Reset();
}

TEST_F(WebPluginContainerTest, ClippedRectsForShiftedIframedElement) {
  RegisterMockedURL("plugin_hidden_before_scroll.html");
  RegisterMockedURL("shifted_plugin_containing_page.html");

  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "shifted_plugin_containing_page.html",
      &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));
  UpdateAllLifecyclePhases(web_view);
  WebLocalFrame* iframe =
      web_view->MainFrame()->FirstChild()->ToWebLocalFrame();
  WebElement plugin_element =
      iframe->GetDocument().GetElementById("plugin-hidden-before-scroll");
  auto* plugin_container_impl =
      To<WebPluginContainerImpl>(plugin_element.PluginContainer());

  DCHECK(plugin_container_impl);

  gfx::Size plugin_size(40, 40);
  gfx::Size iframe_size(40, 40);

  gfx::Vector2d iframe_offset_in_root_frame(0, 300);
  gfx::Vector2d plugin_offset_in_iframe(0, 40);

  auto compute_expected_values = [=](gfx::Point root_document_scroll_to,
                                     gfx::Point iframe_scroll_to) {
    gfx::Vector2d offset_in_iframe =
        plugin_offset_in_iframe - iframe_scroll_to.OffsetFromOrigin();
    gfx::Vector2d offset_in_root_document =
        iframe_offset_in_root_frame -
        root_document_scroll_to.OffsetFromOrigin();
    // window_rect is a plugin rectangle in the root frame coordinates.
    gfx::Rect expected_window_rect(
        gfx::PointAtOffsetFromOrigin(offset_in_root_document +
                                     offset_in_iframe),
        plugin_size);

    // unobscured_rect is the visible part of the plugin, inside the iframe.
    gfx::Rect expected_unobscured_rect(iframe_scroll_to, iframe_size);
    expected_unobscured_rect.Intersect(gfx::Rect(
        gfx::PointAtOffsetFromOrigin(plugin_offset_in_iframe), plugin_size));
    expected_unobscured_rect.Offset(-plugin_offset_in_iframe);

    // clip_rect is the visible part of the unobscured_rect, inside the
    // root_frame.
    gfx::Rect expected_clip_rect = expected_unobscured_rect;
    expected_clip_rect.Offset(expected_window_rect.OffsetFromOrigin());
    expected_clip_rect.Intersect(gfx::Rect(300, 300));
    expected_clip_rect.Offset(-expected_window_rect.OffsetFromOrigin());

    return std::make_tuple(expected_window_rect, expected_clip_rect,
                           expected_unobscured_rect);
  };

  gfx::Point root_document_scrolls_to[] = {
      gfx::Point(0, 0), gfx::Point(0, 20), gfx::Point(0, 300),
      gfx::Point(0, 320), gfx::Point(0, 340)};

  gfx::Point iframe_scrolls_to[] = {gfx::Point(0, 0), gfx::Point(0, 20),
                                    gfx::Point(0, 40), gfx::Point(0, 60),
                                    gfx::Point(0, 80)};

  for (auto& root_document_scroll_to : root_document_scrolls_to) {
    for (auto& iframe_scroll_to : iframe_scrolls_to) {
      web_view->SmoothScroll(root_document_scroll_to.x(),
                             root_document_scroll_to.y(), base::TimeDelta());
      iframe->SetScrollOffset(gfx::PointF(iframe_scroll_to));
      UpdateAllLifecyclePhases(web_view);
      RunPendingTasks();

      auto expected_values =
          compute_expected_values(root_document_scroll_to, iframe_scroll_to);

      gfx::Rect window_rect, clip_rect, unobscured_rect;
      CalculateGeometry(plugin_container_impl, window_rect, clip_rect,
                        unobscured_rect);

      EXPECT_EQ(std::get<0>(expected_values), window_rect);
      EXPECT_EQ(std::get<1>(expected_values), clip_rect);

      // It seems that CalculateGeometry calculates x and y values for empty
      // rectangles slightly differently, but these values are not important in
      // the empty case.
      if(std::get<2>(expected_values).IsEmpty())
        EXPECT_TRUE(unobscured_rect.IsEmpty());
      else
        EXPECT_EQ(std::get<2>(expected_values), unobscured_rect);
    }
  }

  // Cause the plugin's frame to be detached.
  web_view_helper.Reset();
}

TEST_F(WebPluginContainerTest, ClippedRectsForSubpixelPositionedPlugin) {
  RegisterMockedURL("plugin_container.html");

  // Must outlive |web_view_helper|.
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          "subpixel-positioned-plugin");
  auto* plugin_container_impl =
      To<WebPluginContainerImpl>(plugin_element.PluginContainer());

  DCHECK(plugin_container_impl);

  gfx::Rect window_rect, clip_rect, unobscured_rect;
  CalculateGeometry(plugin_container_impl, window_rect, clip_rect,
                    unobscured_rect);
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40), window_rect);
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40), clip_rect);
  EXPECT_EQ(gfx::Rect(0, 0, 40, 40), unobscured_rect);

  // Cause the plugin's frame to be detached.
  web_view_helper.Reset();
}

TEST_F(WebPluginContainerTest, TopmostAfterDetachTest) {
  static constexpr gfx::Rect kTopmostRect(10, 10, 40, 40);

  // Plugin that checks isRectTopmost in destroy().
  class TopmostPlugin : public FakeWebPlugin {
   public:
    explicit TopmostPlugin(const WebPluginParams& params)
        : FakeWebPlugin(params) {}

    bool IsRectTopmost() { return Container()->IsRectTopmost(kTopmostRect); }

    void Destroy() override {
      // In destroy, IsRectTopmost is no longer valid.
      EXPECT_FALSE(Container()->IsRectTopmost(kTopmostRect));
      FakeWebPlugin::Destroy();
    }

   private:
    ~TopmostPlugin() override = default;
  };

  RegisterMockedURL("plugin_container.html");
  // The client must outlive WebViewHelper.
  CustomPluginWebFrameClient<TopmostPlugin> plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  auto* plugin_container_impl =
      To<WebPluginContainerImpl>(GetWebPluginContainer(
          web_view, WebString::FromUTF8("translated-plugin")));
  plugin_container_impl->SetFrameRect(gfx::Rect(0, 0, 300, 300));

  EXPECT_TRUE(plugin_container_impl->IsRectTopmost(kTopmostRect));

  TopmostPlugin* test_plugin =
      static_cast<TopmostPlugin*>(plugin_container_impl->Plugin());
  EXPECT_TRUE(test_plugin->IsRectTopmost());

  // Cause the plugin's frame to be detached.
  web_view_helper.Reset();

  EXPECT_FALSE(plugin_container_impl->IsRectTopmost(kTopmostRect));
}

namespace {

class CompositedPlugin : public FakeWebPlugin {
 public:
  explicit CompositedPlugin(const WebPluginParams& params)
      : FakeWebPlugin(params), layer_(cc::Layer::Create()) {}

  cc::Layer* GetCcLayer() const { return layer_.get(); }

  // WebPlugin

  bool Initialize(WebPluginContainer* container) override {
    if (!FakeWebPlugin::Initialize(container))
      return false;
    container->SetCcLayer(layer_.get());
    return true;
  }

  void Destroy() override {
    Container()->SetCcLayer(nullptr);
    FakeWebPlugin::Destroy();
  }

 private:
  ~CompositedPlugin() override = default;

  scoped_refptr<cc::Layer> layer_;
};

}  // namespace

TEST_F(WebPluginContainerTest, CompositedPlugin) {
  RegisterMockedURL("plugin.html");
  // Must outlive |web_view_helper|
  CustomPluginWebFrameClient<CompositedPlugin> web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin.html", &web_frame_client);
  EnablePlugins(web_view, gfx::Size(800, 600));

  WebPluginContainerImpl* container = static_cast<WebPluginContainerImpl*>(
      GetWebPluginContainer(web_view, WebString::FromUTF8("plugin")));
  ASSERT_TRUE(container);
  const auto* plugin =
      static_cast<const CompositedPlugin*>(container->Plugin());

  PaintController paint_controller;
  paint_controller.UpdateCurrentPaintChunkProperties(PropertyTreeState::Root());
  GraphicsContext graphics_context(paint_controller);
  container->Paint(graphics_context, PaintFlag::kNoFlag,
                   CullRect(gfx::Rect(10, 10, 400, 300)), gfx::Vector2d());
  auto& paint_artifact = paint_controller.CommitNewDisplayItems();

  const auto& display_items = paint_artifact.GetDisplayItemList();
  ASSERT_EQ(1u, display_items.size());
  ASSERT_EQ(DisplayItem::kForeignLayerPlugin, display_items[0].GetType());
  const auto& foreign_layer_display_item =
      To<ForeignLayerDisplayItem>(display_items[0]);
  EXPECT_EQ(plugin->GetCcLayer(), foreign_layer_display_item.GetLayer());
}

TEST_F(WebPluginContainerTest, NeedsWheelEvents) {
  RegisterMockedURL("plugin_container.html");
  // Must outlive |web_view_helper|
  TestPluginWebFrameClient plugin_web_frame_client;
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.InitializeAndLoad(
      base_url_ + "plugin_container.html", &plugin_web_frame_client);
  EnablePlugins(web_view, gfx::Size(300, 300));

  WebElement plugin_container_one_element =
      web_view->MainFrameImpl()->GetDocument().GetElementById(
          WebString::FromUTF8("translated-plugin"));
  plugin_container_one_element.PluginContainer()->SetWantsWheelEvents(true);

  RunPendingTasks();
  EXPECT_TRUE(web_view->MainFrameImpl()
                  ->GetFrame()
                  ->GetEventHandlerRegistry()
                  .HasEventHandlers(EventHandlerRegistry::kWheelEventBlocking));
}

}  // namespace blink
