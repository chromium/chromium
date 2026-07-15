// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/select_popup.h"

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "url/gurl.h"

namespace content {

class DisallowSystemUiPopupsContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  bool ShouldAllowSystemUiPopups(WebContents* web_contents) override {
    return false;
  }
};

class SelectPopupBrowserTest : public ContentBrowserTest {};

class MockPopupMenuClient : public blink::mojom::PopupMenuClient {
 public:
  void DidAcceptIndices(const std::vector<int32_t>& indices) override {}
  void DidCancel() override {}
};

IN_PROC_BROWSER_TEST_F(SelectPopupBrowserTest, DisallowSystemUiPopups) {
  DisallowSystemUiPopupsContentBrowserClient test_client;
  const GURL test_url(
      "data:text/html,<!DOCTYPE html><select><option>1</option></select>");
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  auto* web_contents = static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* view = static_cast<WebContentsViewAndroid*>(web_contents->GetView());

  mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client;
  auto receiver = popup_client.InitWithNewPipeAndPassReceiver();

  view->ShowPopupMenu(web_contents->GetPrimaryMainFrame(),
                      std::move(popup_client), /*bounds=*/gfx::Rect(),
                      /*item_font_size=*/12.0, /*selected_item=*/0,
                      /*menu_items=*/{}, /*right_aligned=*/false,
                      /*allow_multiple_selection=*/false);

  // SelectPopup is created and stored in view. But since
  // ShouldAllowSystemUiPopups returns false, ShowMenu will return early. The
  // popup_client will be dropped immediately.

  MockPopupMenuClient mock_client;
  mojo::Receiver<blink::mojom::PopupMenuClient> bound_receiver(
      &mock_client, std::move(receiver));

  base::test::TestFuture<void> future;
  bound_receiver.set_disconnect_handler(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

}  // namespace content
