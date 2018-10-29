// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_proxy_lookup_helper.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

constexpr char kTestURL[] = "http://foo/";

class PepperProxyLookupHelperTest : public testing::Test {
 public:
  PepperProxyLookupHelperTest() = default;
  ~PepperProxyLookupHelperTest() override = default;

  // Initializes |lookup_helper_| on the IO thread, and starts it there. Returns
  // once it has called into LookUpProxyForURLOnUIThread on the UI thread.
  void StartLookup() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    base::RunLoop run_loop;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&PepperProxyLookupHelperTest::StartLookupOnIOThread,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_TRUE(lookup_helper_);
    if (!fail_to_start_request_)
      EXPECT_TRUE(proxy_lookup_client_);
  }

  // Takes the |ProxyLookupClientPtr| passed by |lookup_helper_| to
  // LookUpProxyForURLOnUIThread(). May only be called after |lookup_helper_|
  // has successfully called into LookUpProxyForURLOnUIThread().
  network::mojom::ProxyLookupClientPtr ClaimProxyLookupClient() {
    EXPECT_TRUE(proxy_lookup_client_);
    return std::move(proxy_lookup_client_);
  }

  void DestroyLookupHelper() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::RunLoop run_loop;

    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &PepperProxyLookupHelperTest::DestroyLookupHelperOnIOThread,
            base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Waits for |lookup_helper_| to call into OnLookupCompleteOnIOThread(),
  // signally proxy lookup completion.
  void WaitForLookupCompletion() {
    EXPECT_TRUE(lookup_helper_);
    lookup_complete_run_loop_.Run();
  }

  // Get the proxy information passed into OnLookupCompleteOnIOThread().
  const base::Optional<net::ProxyInfo>& proxy_info() const {
    return proxy_info_;
  }

  // Setting this to true will make LookUpProxyForURLOnUIThread, the callback
  // invoked to start looking up the proxy, return false.
  void set_fail_to_start_request(bool fail_to_start_request) {
    fail_to_start_request_ = fail_to_start_request;
  }

 private:
  // Must be called on the IO thread. Initializes |lookup_helper_| and starts a
  // proxy lookup. Invokes |closure| on the UI thread once the |lookup_helper_|
  // has invoked LookUpProxyForURLOnUIThread on the UI thread.
  void StartLookupOnIOThread(base::OnceClosure closure) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(!lookup_helper_);

    lookup_helper_ = std::make_unique<PepperProxyLookupHelper>();
    lookup_helper_->Start(
        GURL(kTestURL),
        base::BindOnce(
            &PepperProxyLookupHelperTest::LookUpProxyForURLOnUIThread,
            base::Unretained(this), std::move(closure)),
        base::BindOnce(&PepperProxyLookupHelperTest::OnLookupCompleteOnIOThread,
                       base::Unretained(this)));
  }

  // Callback passed to |lookup_helper_| to start the proxy lookup.
  bool LookUpProxyForURLOnUIThread(
      base::OnceClosure closure,
      const GURL& url,
      network::mojom::ProxyLookupClientPtr proxy_lookup_client) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    std::move(closure).Run();

    if (fail_to_start_request_)
      return false;

    EXPECT_EQ(GURL(kTestURL), url);
    proxy_lookup_client_ = std::move(proxy_lookup_client);
    return true;
  }

  // Invoked by |lookup_helper_| on the IO thread once the proxy lookup has
  // completed.
  void OnLookupCompleteOnIOThread(base::Optional<net::ProxyInfo> proxy_info) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    proxy_info_ = std::move(proxy_info);
    lookup_helper_.reset();
    lookup_complete_run_loop_.Quit();
  }

  void DestroyLookupHelperOnIOThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    lookup_helper_.reset();
  }

  TestBrowserThreadBundle test_browser_thread_bundle_;

  bool fail_to_start_request_ = false;

  std::unique_ptr<PepperProxyLookupHelper> lookup_helper_;

  base::Optional<net::ProxyInfo> proxy_info_;
  network::mojom::ProxyLookupClientPtr proxy_lookup_client_;

  base::RunLoop lookup_complete_run_loop_;
};

TEST_F(PepperProxyLookupHelperTest, Success) {
  StartLookup();
  net::ProxyInfo proxy_info_response;
  proxy_info_response.UseNamedProxy("result:80");
  ClaimProxyLookupClient()->OnProxyLookupComplete(proxy_info_response);
  WaitForLookupCompletion();
  ASSERT_TRUE(proxy_info());
  EXPECT_EQ("PROXY result:80", proxy_info()->ToPacString());
}

// Basic failure case - an error is passed to the PepperProxyLookupHelper
// through the ProxyLookupClient API.
TEST_F(PepperProxyLookupHelperTest, Failure) {
  StartLookup();
  ClaimProxyLookupClient()->OnProxyLookupComplete(base::nullopt);
  WaitForLookupCompletion();
  EXPECT_FALSE(proxy_info());
}

// The mojo pipe is closed before the PepperProxyLookupHelper's callback is
// invoked.
TEST_F(PepperProxyLookupHelperTest, PipeClosed) {
  StartLookup();
  ClaimProxyLookupClient().reset();
  WaitForLookupCompletion();
  EXPECT_FALSE(proxy_info());
}

// The proxy lookup fails to start - instead, the callback to start the lookup
// returns false.
TEST_F(PepperProxyLookupHelperTest, FailToStartRequest) {
  set_fail_to_start_request(true);

  StartLookup();
  WaitForLookupCompletion();
  EXPECT_FALSE(proxy_info());
}

// Destroy the helper before it completes a lookup. Make sure it cancels the
// connection, and memory tools don't detect a leak.
TEST_F(PepperProxyLookupHelperTest, DestroyBeforeComplete) {
  StartLookup();
  base::RunLoop run_loop;
  network::mojom::ProxyLookupClientPtr proxy_lookup_client =
      ClaimProxyLookupClient();
  proxy_lookup_client.set_connection_error_handler(run_loop.QuitClosure());
  DestroyLookupHelper();
  run_loop.Run();
}

}  // namespace
}  // namespace content
