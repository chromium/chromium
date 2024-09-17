// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/external_popup_menu.h"

#include <memory>

#include "content/test/test_blink_web_unit_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_popup_menu_info.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/popup_menu.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class ExternalPopupMenuDisplayNoneItemsTest : public PageTestBase {
 public:
  ExternalPopupMenuDisplayNoneItemsTest() = default;

 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    auto* element = MakeGarbageCollected<HTMLSelectElement>(GetDocument());
    // Set the 4th an 5th items to have "display: none" property
    element->setInnerHTML(
        "<option><option><option><option style='display:none;'><option "
        "style='display:none;'><option><option>");
    GetDocument().body()->AppendChild(element, ASSERT_NO_EXCEPTION);
    owner_element_ = element;
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  }

  Persistent<HTMLSelectElement> owner_element_;
};

TEST_F(ExternalPopupMenuDisplayNoneItemsTest, PopupMenuInfoSizeTest) {
  int32_t item_height;
  double font_size;
  int32_t selected_item;
  Vector<mojom::blink::MenuItemPtr> menu_items;
  bool right_aligned;
  bool allow_multiple_selection;
  ExternalPopupMenu::GetPopupMenuInfo(
      *owner_element_, &item_height, &font_size, &selected_item, &menu_items,
      &right_aligned, &allow_multiple_selection);
  EXPECT_EQ(5U, menu_items.size());
}

TEST_F(ExternalPopupMenuDisplayNoneItemsTest, IndexMappingTest) {
  // 6th indexed item in popupmenu would be the 4th item in ExternalPopupMenu,
  // and vice-versa.
  EXPECT_EQ(
      4, ExternalPopupMenu::ToExternalPopupMenuItemIndex(6, *owner_element_));
  EXPECT_EQ(6, ExternalPopupMenu::ToPopupMenuItemIndex(4, *owner_element_));

  // Invalid index, methods should return -1.
  EXPECT_EQ(
      -1, ExternalPopupMenu::ToExternalPopupMenuItemIndex(8, *owner_element_));
  EXPECT_EQ(-1, ExternalPopupMenu::ToPopupMenuItemIndex(8, *owner_element_));
}

class ExternalPopupMenuHrElementItemsTest : public PageTestBase {
 public:
  ExternalPopupMenuHrElementItemsTest() = default;

 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    auto* element = MakeGarbageCollected<HTMLSelectElement>(GetDocument());
    element->setInnerHTML(R"HTML(
      <option>zero</option>
      <option>one</option>
      <hr>
      <option>two or three</option>
    )HTML");
    GetDocument().body()->AppendChild(element, ASSERT_NO_EXCEPTION);
    owner_element_ = element;
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  }

  Persistent<HTMLSelectElement> owner_element_;
};

TEST_F(ExternalPopupMenuHrElementItemsTest, PopupMenuInfoSizeTest) {
  int32_t item_height;
  double font_size;
  int32_t selected_item;
  Vector<mojom::blink::MenuItemPtr> menu_items;
  bool right_aligned;
  bool allow_multiple_selection;
  ExternalPopupMenu::GetPopupMenuInfo(
      *owner_element_, &item_height, &font_size, &selected_item, &menu_items,
      &right_aligned, &allow_multiple_selection);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(3U, menu_items.size());
#else
  EXPECT_EQ(4U, menu_items.size());
#endif
}

TEST_F(ExternalPopupMenuHrElementItemsTest, IndexMappingTest) {
  EXPECT_EQ(
      0, ExternalPopupMenu::ToExternalPopupMenuItemIndex(0, *owner_element_));
  EXPECT_EQ(
      1, ExternalPopupMenu::ToExternalPopupMenuItemIndex(1, *owner_element_));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(
      -1, ExternalPopupMenu::ToExternalPopupMenuItemIndex(2, *owner_element_));
  EXPECT_EQ(
      2, ExternalPopupMenu::ToExternalPopupMenuItemIndex(3, *owner_element_));
#else
  EXPECT_EQ(
      2, ExternalPopupMenu::ToExternalPopupMenuItemIndex(2, *owner_element_));
  EXPECT_EQ(
      3, ExternalPopupMenu::ToExternalPopupMenuItemIndex(3, *owner_element_));
#endif

  EXPECT_EQ(0, ExternalPopupMenu::ToPopupMenuItemIndex(0, *owner_element_));
  EXPECT_EQ(1, ExternalPopupMenu::ToPopupMenuItemIndex(1, *owner_element_));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(3, ExternalPopupMenu::ToPopupMenuItemIndex(2, *owner_element_));
  EXPECT_EQ(-1, ExternalPopupMenu::ToPopupMenuItemIndex(3, *owner_element_));
#else
  EXPECT_EQ(2, ExternalPopupMenu::ToPopupMenuItemIndex(2, *owner_element_));
  EXPECT_EQ(3, ExternalPopupMenu::ToPopupMenuItemIndex(3, *owner_element_));
#endif
}

class TestLocalFrameExternalPopupClient : public FakeLocalFrameHost {
 public:
  void ShowPopupMenu(
      mojo::PendingRemote<mojom::blink::PopupMenuClient> popup_client,
      const gfx::Rect& bounds,
      int32_t item_height,
      double font_size,
      int32_t selected_item,
      Vector<mojom::blink::MenuItemPtr> menu_items,
      bool right_aligned,
      bool allow_multiple_selection) override {
    Reset();

    bounds_ = bounds;
    selected_item_ = selected_item;
    menu_items_ = std::move(menu_items);
    popup_client_.Bind(std::move(popup_client));
    popup_client_.set_disconnect_handler(WTF::BindOnce(
        &TestLocalFrameExternalPopupClient::Reset, WTF::Unretained(this)));
    std::move(showed_callback_).Run();
  }

  void Reset() { popup_client_.reset(); }

  void WaitUntilShowedPopup() {
    base::RunLoop run_loop;
    showed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  mojom::blink::PopupMenuClient* PopupClient() {
    DCHECK(popup_client_);
    return popup_client_.get();
  }

  bool IsBound() const { return popup_client_.is_bound(); }

  const Vector<mojom::blink::MenuItemPtr>& MenuItems() const {
    return menu_items_;
  }

  int32_t SelectedItem() const { return selected_item_; }

  const gfx::Rect& ShownBounds() const { return bounds_; }

 private:
  base::OnceClosure showed_callback_;
  mojo::Remote<mojom::blink::PopupMenuClient> popup_client_;
  int32_t selected_item_;
  Vector<mojom::blink::MenuItemPtr> menu_items_;
  gfx::Rect bounds_;
};

class ExternalPopupMenuTest : public PageTestBase {
 public:
  ExternalPopupMenuTest() : base_url_("http://www.test.com") {}

 protected:
  void SetUp() override {
    frame_host_.Init(
        web_frame_client_.GetRemoteNavigationAssociatedInterfaces());
    helper_.Initialize(&web_frame_client_);
    WebView()->SetUseExternalPopupMenus(true);
  }
  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void RegisterMockedURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath("popup"),
        WebString::FromUTF8(file_name), WebString::FromUTF8("text/html"));
  }

  void LoadFrame(const std::string& file_name) {
    frame_test_helpers::LoadFrame(MainFrame(), base_url_ + file_name);
    WebView()->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
    WebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  WebViewImpl* WebView() const { return helper_.GetWebView(); }

  const Vector<mojom::blink::MenuItemPtr>& MenuItems() const {
    return frame_host_.MenuItems();
  }

  bool IsBound() const { return frame_host_.IsBound(); }

  int32_t SelectedItem() const { return frame_host_.SelectedItem(); }

  const gfx::Rect& ShownBounds() const { return frame_host_.ShownBounds(); }

  mojom::blink::PopupMenuClient* PopupClient() {
    return frame_host_.PopupClient();
  }

  void WaitUntilShowedPopup() { frame_host_.WaitUntilShowedPopup(); }

  WebLocalFrameImpl* MainFrame() const { return helper_.LocalMainFrame(); }

 private:
  TestLocalFrameExternalPopupClient frame_host_;
  frame_test_helpers::TestWebFrameClient web_frame_client_;
  std::string base_url_;
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(ExternalPopupMenuTest, PopupAccountsForVisualViewportTransform) {
  RegisterMockedURLLoad("select_mid_screen.html");
  LoadFrame("select_mid_screen.html");

  WebView()->MainFrameViewWidget()->Resize(gfx::Size(100, 100));
  WebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  auto* select = To<HTMLSelectElement>(
      MainFrame()->GetFrame()->GetDocument()->getElementById(
          AtomicString("select")));
  auto* layout_object = select->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  VisualViewport& visual_viewport = WebView()->GetPage()->GetVisualViewport();

  gfx::Rect rect_in_document = layout_object->AbsoluteBoundingBoxRect();

  constexpr int kScaleFactor = 2;
  ScrollOffset scroll_delta(20, 30);

  const int expected_x =
      (rect_in_document.x() - scroll_delta.x()) * kScaleFactor;
  const int expected_y =
      (rect_in_document.y() - scroll_delta.y()) * kScaleFactor;

  WebView()->SetPageScaleFactor(kScaleFactor);
  visual_viewport.Move(scroll_delta);
  select->ShowPopup();
  WaitUntilShowedPopup();

  EXPECT_EQ(expected_x, ShownBounds().x());
  EXPECT_EQ(expected_y, ShownBounds().y());
}

// Android doesn't use this position data and we don't adjust it for DPR there..
#ifdef OS_ANDROID
#define MAYBE_PopupAccountsForDeviceScaleFactor \
  DISABLED_PopupAccountsForDeviceScaleFactor
#else
#define MAYBE_PopupAccountsForDeviceScaleFactor \
  PopupAccountsForDeviceScaleFactor
#endif

TEST_F(ExternalPopupMenuTest, MAYBE_PopupAccountsForDeviceScaleFactor) {
  RegisterMockedURLLoad("select_mid_screen.html");
  LoadFrame("select_mid_screen.html");

  constexpr float kScaleFactor = 2.0f;
  WebView()->MainFrameWidget()->SetDeviceScaleFactorForTesting(kScaleFactor);
  WebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  auto* select = To<HTMLSelectElement>(
      MainFrame()->GetFrame()->GetDocument()->getElementById(
          AtomicString("select")));
  auto* layout_object = select->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  select->ShowPopup();
  WaitUntilShowedPopup();

  // The test file has no body margins but 50px of padding.
  EXPECT_EQ(50, ShownBounds().x());
  EXPECT_EQ(50, ShownBounds().y());
}

TEST_F(ExternalPopupMenuTest, DidAcceptIndex) {
  RegisterMockedURLLoad("select.html");
  LoadFrame("select.html");

  auto* select = To<HTMLSelectElement>(
      MainFrame()->GetFrame()->GetDocument()->getElementById(
          AtomicString("select")));
  auto* layout_object = select->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  select->ShowPopup();
  WaitUntilShowedPopup();

  ASSERT_TRUE(select->PopupIsVisible());

  PopupClient()->DidAcceptIndices({2});
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(select->PopupIsVisible());
  ASSERT_EQ("2", select->InnerElement().innerText().Utf8());
  EXPECT_EQ(2, select->selectedIndex());
}

TEST_F(ExternalPopupMenuTest, DidAcceptIndices) {
  RegisterMockedURLLoad("select.html");
  LoadFrame("select.html");

  auto* select = To<HTMLSelectElement>(
      MainFrame()->GetFrame()->GetDocument()->getElementById(
          AtomicString("select")));
  auto* layout_object = select->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  select->ShowPopup();
  WaitUntilShowedPopup();

  ASSERT_TRUE(select->PopupIsVisible());

  PopupClient()->DidAcceptIndices({2});
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(select->PopupIsVisible());
  EXPECT_EQ("2", select->InnerElement().innerText());
  EXPECT_EQ(2, select->selectedIndex());
}

TEST_F(ExternalPopupMenuTest, DidAcceptIndicesClearSelect) {
  RegisterMockedURLLoad("select.html");
  LoadFrame("select.html");

  auto* select = To<HTMLSelectElement>(
      MainFrame()->GetFrame()->GetDocument()->getElementById(
          AtomicString("select")));
  auto* layout_object = select->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  select->ShowPopup();
  WaitUntilShowedPopup();

  ASSERT_TRUE(select->PopupIsVisible());
  PopupClient()->DidAcceptIndices({});
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(select->PopupIsVisible());
  EXPECT_EQ(-1, select->selectedIndex());
}

// Normal case: test showing a select popup, canceling/selecting an item.
TEST_F(ExternalPopupMenuTest, NormalCase) {
  RegisterMockedURLLoad("select.html");
  LoadFrame("select.html");

  // Show the popup-menu.
  auto* select = To<HTMLSelectElement>(
      MainFrame()->GetFrame()->GetDocument()->getElementById(
          AtomicString("select")));
  auto* layout_object = select->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  select->ShowPopup();
  WaitUntilShowedPopup();

  ASSERT_TRUE(select->PopupIsVisible());
  ASSERT_EQ(3U, MenuItems().size());
  EXPECT_EQ(1, SelectedItem());

  // Simulate the user canceling the popup; the index should not have changed.
  PopupClient()->DidCancel();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, select->selectedIndex());

  // Show the pop-up again and this time make a selection.
  select->ShowPopup();
  WaitUntilShowedPopup();

  PopupClient()->DidAcceptIndices({0});
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, select->selectedIndex());

  // Show the pop-up again and make another selection.
  select->ShowPopup();
  WaitUntilShowedPopup();

  ASSERT_EQ(3U, MenuItems().size());
  EXPECT_EQ(0, SelectedItem());
}

// Page shows popup, then navigates away while popup showing, then select.
TEST_F(ExternalPopupMenuTest, ShowPopupThenNavigate) {
  RegisterMockedURLLoad("select.html");
  LoadFrame("select.html");

  // Show the popup-menu.
  auto* document = MainFrame()->GetFrame()->GetDocument();
  auto* select =
      To<HTMLSelectElement>(document->getElementById(AtomicString("select")));
  auto* layout_object = select->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  select->ShowPopup();
  WaitUntilShowedPopup();

  // Now we navigate to another pager.
  document->documentElement()->setInnerHTML("<blink>Awesome page!</blink>");
  document->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  base::RunLoop().RunUntilIdle();

  // Now HTMLSelectElement should be nullptr and mojo is disconnected.
  select =
      To<HTMLSelectElement>(document->getElementById(AtomicString("select")));
  EXPECT_FALSE(select);
  EXPECT_FALSE(IsBound());
}

// An empty select should not cause a crash when clicked.
// http://crbug.com/63774
TEST_F(ExternalPopupMenuTest, EmptySelect) {
  RegisterMockedURLLoad("select.html");
  LoadFrame("select.html");

  auto* select = To<HTMLSelectElement>(
      MainFrame()->GetFrame()->GetDocument()->getElementById(
          AtomicString("emptySelect")));
  EXPECT_TRUE(select);
  select->click();
}

// Tests that nothing bad happen when the page removes the select when it
// changes. (http://crbug.com/61997)
TEST_F(ExternalPopupMenuTest, RemoveOnChange) {
  RegisterMockedURLLoad("select_event_remove_on_change.html");
  LoadFrame("select_event_remove_on_change.html");

  // Show the popup-menu.
  auto* document = MainFrame()->GetFrame()->GetDocument();
  auto* select =
      To<HTMLSelectElement>(document->getElementById(AtomicString("s")));
  auto* layout_object = select->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  select->ShowPopup();
  WaitUntilShowedPopup();

  // Select something, it causes the select to be removed from the page.
  PopupClient()->DidAcceptIndices({1});
  base::RunLoop().RunUntilIdle();

  // Just to check the soundness of the test.
  // It should return nullptr as the select has been removed.
  select = To<HTMLSelectElement>(document->getElementById(AtomicString("s")));
  EXPECT_FALSE(select);
}

// crbug.com/912211
TEST_F(ExternalPopupMenuTest, RemoveFrameOnChange) {
  RegisterMockedURLLoad("select_event_remove_frame_on_change.html");
  LoadFrame("select_event_remove_frame_on_change.html");

  // Open a popup.
  auto* iframe = To<HTMLIFrameElement>(
      MainFrame()->GetFrame()->GetDocument()->QuerySelector(
          AtomicString("iframe")));
  auto* select = To<HTMLSelectElement>(
      iframe->contentDocument()->QuerySelector(AtomicString("select")));
  auto* layout_object = select->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  select->ShowPopup();

  // Select something on the sub-frame, it causes the frame to be removed from
  // the page.
  select->SelectOptionByPopup(1);
  // The test passes if the test didn't crash and ASAN didn't complain.
}

}  // namespace blink
