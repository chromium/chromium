// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/portal/portal.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/portal/portal.mojom.h"
#include "url/url_constants.h"
using testing::_;

namespace content {

// The PortalInterceptorForTesting can be used in tests to inspect Portal IPCs.
class PortalInterceptorForTesting final
    : public blink::mojom::PortalInterceptorForTesting {
 public:
  static PortalInterceptorForTesting* Create(
      RenderFrameHostImpl* render_frame_host_impl,
      blink::mojom::PortalRequest request);
  static PortalInterceptorForTesting* From(content::Portal* portal);

  void Init(InitCallback callback) override {
    portal_->Init(std::move(callback));

    // Init should be called only once.
    ASSERT_FALSE(portal_initialized_);
    portal_initialized_ = true;

    if (run_loop_) {
      run_loop_->Quit();
      run_loop_ = nullptr;
    }
  }

  void Activate(base::OnceCallback<void(blink::mojom::PortalActivationStatus)>
                    callback) override {
    portal_activated_ = true;

    if (run_loop_) {
      run_loop_->Quit();
      run_loop_ = nullptr;
    }

    // |this| can be destroyed after Activate() is called.
    portal_->Activate(std::move(callback));
  }

  void WaitForInit() {
    if (portal_initialized_)
      return;

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
  }

  void WaitForActivate() {
    if (portal_activated_)
      return;

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
  }

  // Test getters.
  content::Portal* GetPortal() { return portal_.get(); }
  WebContents* GetPortalContents() { return portal_->GetPortalContents(); }

 private:
  PortalInterceptorForTesting(RenderFrameHostImpl* render_frame_host_impl)
      : portal_(content::Portal::CreateForTesting(render_frame_host_impl)) {}

  blink::mojom::Portal* GetForwardingInterface() override {
    return portal_.get();
  }

  std::unique_ptr<content::Portal> portal_;
  bool portal_initialized_ = false;
  bool portal_activated_ = false;
  base::RunLoop* run_loop_ = nullptr;
};

// static
PortalInterceptorForTesting* PortalInterceptorForTesting::Create(
    RenderFrameHostImpl* render_frame_host_impl,
    blink::mojom::PortalRequest request) {
  auto test_portal_ptr =
      base::WrapUnique(new PortalInterceptorForTesting(render_frame_host_impl));
  PortalInterceptorForTesting* test_portal = test_portal_ptr.get();
  test_portal->GetPortal()->SetBindingForTesting(
      mojo::MakeStrongBinding(std::move(test_portal_ptr), std::move(request)));
  return test_portal;
}

// static
PortalInterceptorForTesting* PortalInterceptorForTesting::From(
    content::Portal* portal) {
  blink::mojom::Portal* impl = portal->GetBindingForTesting()->impl();
  auto* interceptor = static_cast<PortalInterceptorForTesting*>(impl);
  CHECK_NE(static_cast<blink::mojom::Portal*>(portal), impl);
  CHECK_EQ(interceptor->GetPortal(), portal);
  return interceptor;
}

class MockPortalWebContentsDelegate : public WebContentsDelegate {
 public:
  MockPortalWebContentsDelegate() {}
  ~MockPortalWebContentsDelegate() override {}

  MOCK_METHOD4(
      DoSwapWebContents,
      std::unique_ptr<WebContents>(WebContents*, WebContents*, bool, bool));
  std::unique_ptr<WebContents> SwapWebContents(
      WebContents* old_contents,
      std::unique_ptr<WebContents> new_contents,
      bool did_start_load,
      bool did_finish_load) override {
    DoSwapWebContents(old_contents, new_contents.get(), did_start_load,
                      did_finish_load);
    return new_contents;
  }
};

// The PortalCreatedObserver observes portal creations on
// |render_frame_host_impl|. This observer can be used to monitor for multiple
// Portal creations on the same RenderFrameHost, by repeatedly calling
// WaitUntilPortalCreated().
//
// The PortalCreatedObserver replaces the Portal interface in the
// RenderFrameHosts' BinderRegistry, so when the observer is destroyed the
// RenderFrameHost is left without an interface and attempts to create the
// interface will fail.
class PortalCreatedObserver {
 public:
  explicit PortalCreatedObserver(RenderFrameHostImpl* render_frame_host_impl)
      : render_frame_host_impl_(render_frame_host_impl) {
    service_manager::BinderRegistry& registry =
        render_frame_host_impl->BinderRegistryForTesting();

    registry.AddInterface(base::BindRepeating(
        [](PortalCreatedObserver* observer,
           RenderFrameHostImpl* render_frame_host_impl,
           blink::mojom::PortalRequest request) {
          observer->portal_ = PortalInterceptorForTesting::Create(
                                  render_frame_host_impl, std::move(request))
                                  ->GetPortal();
          if (observer->run_loop_)
            observer->run_loop_->Quit();
        },
        base::Unretained(this), base::Unretained(render_frame_host_impl)));
  }

  ~PortalCreatedObserver() {
    service_manager::BinderRegistry& registry =
        render_frame_host_impl_->BinderRegistryForTesting();

    registry.RemoveInterface<Portal>();
  }

  Portal* WaitUntilPortalCreated() {
    Portal* portal = portal_;
    if (portal) {
      portal_ = nullptr;
      return portal;
    }

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;

    portal = portal_;
    portal_ = nullptr;
    return portal;
  }

 private:
  RenderFrameHostImpl* render_frame_host_impl_;
  base::RunLoop* run_loop_ = nullptr;
  Portal* portal_ = nullptr;
};

class PortalBrowserTest : public ContentBrowserTest {
 protected:
  PortalBrowserTest() {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPortals);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the renderer can create a Portal.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, CreatePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  PortalCreatedObserver portal_created_observer(main_frame);
  EXPECT_TRUE(
      ExecJs(main_frame,
             "document.body.appendChild(document.createElement('portal'));"));
  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  EXPECT_NE(nullptr, portal);
}

// Tests the the renderer can navigate a Portal.
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, NavigatePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  PortalCreatedObserver portal_created_observer(main_frame);

  // Tests that a portal can navigate by setting its src before appending it to
  // the DOM.
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(
      ExecJs(main_frame,
             base::StringPrintf("var portal = document.createElement('portal');"
                                "portal.src = '%s';"
                                "document.body.appendChild(portal);",
                                a_url.spec().c_str())));

  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(
          portal_created_observer.WaitUntilPortalCreated());
  portal_interceptor->WaitForInit();
  WebContents* portal_contents = portal_interceptor->GetPortalContents();
  EXPECT_NE(nullptr, portal_contents);
  EXPECT_NE(portal_contents->GetLastCommittedURL(), a_url);

  // WaitForInit() above only waits for the Portal::Init call, which is when the
  // Portal's WebContents is created. Portal::Navigate is a diffent IPC, so the
  // portal should not have navigated yet, and we can observe the Portal's first
  // navigation.
  TestNavigationObserver navigation_observer(portal_contents);
  navigation_observer.Wait();
  EXPECT_EQ(navigation_observer.last_navigation_url(), a_url);
  EXPECT_EQ(portal_contents->GetLastCommittedURL(), a_url);

  // Tests that a portal can navigate by setting its src.
  {
    TestNavigationObserver navigation_observer(portal_contents);

    GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
    EXPECT_TRUE(ExecJs(
        main_frame,
        base::StringPrintf("document.querySelector('portal').src = '%s';",
                           b_url.spec().c_str())));
    navigation_observer.Wait();
    EXPECT_EQ(navigation_observer.last_navigation_url(), b_url);
    EXPECT_EQ(portal_contents->GetLastCommittedURL(), b_url);
  }

  // Tests that a portal can navigating by attribute.
  {
    TestNavigationObserver navigation_observer(portal_contents);

    GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
    EXPECT_TRUE(ExecJs(
        main_frame,
        base::StringPrintf(
            "document.querySelector('portal').setAttribute('src', '%s');",
            c_url.spec().c_str())));
    navigation_observer.Wait();
    EXPECT_EQ(navigation_observer.last_navigation_url(), c_url);
    EXPECT_EQ(portal_contents->GetLastCommittedURL(), c_url);
  }
}

// Tests that the WebContentsDelegate will receive a request to swap the
// WebContents when a portal is activated.
// Disabled due to flakiness on Android.  See https://crbug.com/892669.
#if defined(OS_ANDROID)
#define MAYBE_ActivatePortal DISABLED_ActivatePortal
#else
#define MAYBE_ActivatePortal ActivatePortal
#endif

IN_PROC_BROWSER_TEST_F(PortalBrowserTest, MAYBE_ActivatePortal) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  PortalCreatedObserver portal_created_observer(main_frame);
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(ExecJs(main_frame,
                     JsReplace("var portal = document.createElement('portal');"
                               "portal.src = $1;"
                               "document.body.appendChild(portal);",
                               a_url)));
  Portal* portal = portal_created_observer.WaitUntilPortalCreated();
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);
  portal_interceptor->WaitForInit();

  MockPortalWebContentsDelegate mock_delegate;
  shell()->web_contents()->SetDelegate(&mock_delegate);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate,
              DoSwapWebContents(shell()->web_contents(),
                                portal->GetPortalContents(), _, _))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit),
          testing::ReturnNull()));
  EXPECT_TRUE(
      ExecJs(main_frame, "document.querySelector('portal').activate();"));
  run_loop.Run();
}

// Tests that a portal can be activated in content_shell.
// Disabled due to flakiness on Android.  See https://crbug.com/892669.
#if defined(OS_ANDROID)
#define MAYBE_ActivatePortalInShell DISABLED_ActivatePortalInShell
#else
#define MAYBE_ActivatePortalInShell ActivatePortalInShell
#endif
IN_PROC_BROWSER_TEST_F(PortalBrowserTest, MAYBE_ActivatePortalInShell) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("portal.test", "/title1.html")));
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHostImpl* main_frame = web_contents_impl->GetMainFrame();

  Portal* portal = nullptr;
  {
    PortalCreatedObserver portal_created_observer(main_frame);
    GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    EXPECT_TRUE(ExecJs(
        main_frame, JsReplace("var portal = document.createElement('portal');"
                              "portal.src = $1;"
                              "document.body.appendChild(portal);",
                              a_url)));
    portal = portal_created_observer.WaitUntilPortalCreated();
  }
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::From(portal);
  portal_interceptor->WaitForInit();

  // Ensure that the portal WebContents exists and is different from the tab's
  // WebContents.
  WebContents* portal_contents = portal->GetPortalContents();
  EXPECT_NE(nullptr, portal_contents);
  EXPECT_NE(portal_contents, shell()->web_contents());

  ExecuteScriptAsync(main_frame,
                     "document.querySelector('portal').activate();");
  portal_interceptor->WaitForActivate();

  // After activation, the shell's WebContents should be the previous portal's
  // WebContents.
  EXPECT_EQ(portal_contents, shell()->web_contents());
}

}  // namespace content
