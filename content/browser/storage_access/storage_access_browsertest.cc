// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "third_party/blink/public/mojom/storage_access/storage_access_handle.mojom.h"

namespace content {

class StorageAccessBrowserTest : public ContentBrowserTest {
 public:
  std::string AttemptToBindStorageAccessHandleWithPermissionAndReturnBadMessage(
      const blink::mojom::PermissionStatus& status) {
    // Setup message interceptor.
    std::string received_error;
    mojo::SetDefaultProcessErrorHandler(
        base::BindLambdaForTesting([&](const std::string& error) {
          ASSERT_EQ(received_error, "");
          received_error = error;
        }));

    // Load website.
    EXPECT_TRUE(embedded_test_server()->Start());
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL("/simple_page.html")));

    // Set permission to `status` for STORAGE_ACCESS_GRANT.
    static_cast<PermissionControllerImpl*>(
        host()->GetBrowserContext()->GetPermissionController())
        ->SetPermissionOverride(
            absl::nullopt, blink::PermissionType::STORAGE_ACCESS_GRANT, status);

    // We need access to the interface broker to test bad messages, so must
    // unbind the existing one and bind our own.
    EXPECT_TRUE(
        host()->browser_interface_broker_receiver_for_testing().Unbind());
    mojo::Remote<blink::mojom::BrowserInterfaceBroker> broker_remote;
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        broker_receiver = broker_remote.BindNewPipeAndPassReceiver();
    host()->BindBrowserInterfaceBrokerReceiver(std::move(broker_receiver));

    // Try to bind our StorageAccessHandle.
    mojo::Remote<blink::mojom::StorageAccessHandle> storage_remote;
    broker_remote->GetInterface(storage_remote.BindNewPipeAndPassReceiver());
    broker_remote.FlushForTesting();

    // Cleanup message interceptor.
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());

    return received_error;
  }

 private:
  RenderFrameHostImpl* host() {
    return static_cast<RenderFrameHostImpl*>(
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetPrimaryFrameTree()
            .root()
            ->current_frame_host());
  }
};

#if DCHECK_IS_ON()
IN_PROC_BROWSER_TEST_F(StorageAccessBrowserTest, HandleDenied) {
  EXPECT_EQ(AttemptToBindStorageAccessHandleWithPermissionAndReturnBadMessage(
                blink::mojom::PermissionStatus::DENIED),
            "Binding a StorageAccessHandle requires the STORAGE_ACCESS_GRANT "
            "permission.");
}
#endif

#if DCHECK_IS_ON()
IN_PROC_BROWSER_TEST_F(StorageAccessBrowserTest, HandleAsk) {
  EXPECT_EQ(AttemptToBindStorageAccessHandleWithPermissionAndReturnBadMessage(
                blink::mojom::PermissionStatus::ASK),
            "Binding a StorageAccessHandle requires the STORAGE_ACCESS_GRANT "
            "permission.");
}
#endif

IN_PROC_BROWSER_TEST_F(StorageAccessBrowserTest, HandleGranted) {
  EXPECT_EQ(AttemptToBindStorageAccessHandleWithPermissionAndReturnBadMessage(
                blink::mojom::PermissionStatus::GRANTED),
            "");
}

}  // namespace content
