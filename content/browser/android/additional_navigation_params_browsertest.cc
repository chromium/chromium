// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/additional_navigation_params.h"

#include "base/android/jni_android.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/android/additional_navigation_params_android.h"
#include "content/public/browser/initiator_navigation_state.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_id.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace content {

class AdditionalNavigationParamsBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  WebContents* web_contents() { return shell()->web_contents(); }
};

// AdditionalNavigationParams carries the initiator frame token across an
// arbitrary delay before it is consumed by NavigationController::LoadUrl. The
// initiator frame may be detached during that delay, so creating the params
// must keep the frame's PolicyContainerHost reachable for the lifetime of the
// returned keep-alive handle.
IN_PROC_BROWSER_TEST_F(AdditionalNavigationParamsBrowserTest,
                       InitiatorPolicyContainerHostKeptAliveAfterFrameDetach) {
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a.com(b.com)")));

  RenderFrameHostImpl* child_rfh =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(web_contents(), 0));
  ASSERT_TRUE(child_rfh);

  const blink::LocalFrameToken child_token = child_rfh->GetFrameToken();
  const ChildProcessId child_process_id = child_rfh->GetProcess()->GetID();

  base::android::ScopedJavaLocalRef<jobject> java_params =
      CreateJavaAdditionalNavigationParams(
          base::android::AttachCurrentThread(), *child_rfh,
          /*attribution_src_token=*/std::nullopt);
  ASSERT_TRUE(java_params);

  // Detach the initiator frame while still holding the params.
  RenderFrameDeletedObserver deleted_observer(child_rfh);
  ASSERT_TRUE(
      ExecJs(web_contents(), "document.querySelector('iframe').remove();"));
  ASSERT_TRUE(deleted_observer.WaitUntilDeleted());
  ASSERT_FALSE(
      RenderFrameHostImpl::FromFrameToken(child_process_id, child_token));

  // Verify round-trip extraction of frame token and process ID even after frame
  // detachment.
  std::optional<blink::LocalFrameToken> extracted_frame_token =
      GetInitiatorFrameTokenFromJavaAdditionalNavigationParams(
          base::android::AttachCurrentThread(), java_params);
  EXPECT_TRUE(extracted_frame_token.has_value());
  EXPECT_EQ(extracted_frame_token.value(), child_token);

  ChildProcessId extracted_process_id =
      GetInitiatorProcessIdFromJavaAdditionalNavigationParams(
          base::android::AttachCurrentThread(), java_params);
  EXPECT_FALSE(extracted_process_id.is_null());
  EXPECT_EQ(extracted_process_id, child_process_id);

  // We can get the state from the Java object.
  scoped_refptr<InitiatorNavigationState> taken_state =
      TakeNativeStateFromJavaAdditionalNavigationParams(
          base::android::AttachCurrentThread(), java_params);
  EXPECT_TRUE(taken_state);

  // Clean up the Java object to satisfy LifetimeAssert and release native
  // state.
  DestroyJavaAdditionalNavigationParams(base::android::AttachCurrentThread(),
                                        java_params);
}

}  // namespace content
