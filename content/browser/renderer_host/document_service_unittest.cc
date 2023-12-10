// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/document_service.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/document_service_echo_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/echo.test-mojom.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

// Unit test for DocumentService in content/public/browser.

namespace content {

namespace {

const char kFooOrigin[] = "https://foo.com";
const char kBarOrigin[] = "https://bar.com";

// Help functions to manipulate RenderFrameHosts.

// Simulates navigation and returns the final RenderFrameHost.
RenderFrameHost* SimulateNavigation(RenderFrameHost* rfh, const GURL& url) {
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(url, rfh);
  navigation_simulator->Commit();
  return navigation_simulator->GetFinalRenderFrameHost();
}

RenderFrameHost* AddChildFrame(RenderFrameHost* rfh, const GURL& url) {
  RenderFrameHost* child_rfh = RenderFrameHostTester::For(rfh)->AppendChild("");
  RenderFrameHostTester::For(child_rfh)->InitializeRenderFrameIfNeeded();
  return SimulateNavigation(child_rfh, url);
}

void DetachFrame(RenderFrameHost* rfh) {
  RenderFrameHostTester::For(rfh)->Detach();
}

}  // namespace

class DocumentServiceTest : public RenderViewHostTestHarness {
 protected:
  void SetUp() final {
    RenderViewHostTestHarness::SetUp();
    Initialize();
  }

  void Initialize() {
    RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();
    RenderFrameHostTester::For(main_rfh)->InitializeRenderFrameIfNeeded();
    SimulateNavigation(main_rfh, GURL(kFooOrigin));
  }

  void CreateEchoImpl(RenderFrameHost& rfh) {
    DCHECK(!is_echo_impl_alive_);
    new DocumentServiceEchoImpl(
        rfh, echo_remote_.BindNewPipeAndPassReceiver(),
        base::BindOnce(&DocumentServiceTest::OnEchoImplDestructed,
                       base::Unretained(this)));
    is_echo_impl_alive_ = true;
  }

  void OnEchoImplDestructed() {
    DCHECK(is_echo_impl_alive_);
    is_echo_impl_alive_ = false;
  }

  void ResetConnection() {
    echo_remote_.reset();
    base::RunLoop().RunUntilIdle();
  }

  RenderFrameHost* main_rfh() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  mojo::Remote<mojom::Echo> echo_remote_;
  bool is_echo_impl_alive_ = false;
};

TEST_F(DocumentServiceTest, ConnectionError) {
  CreateEchoImpl(*main_rfh());
  ResetConnection();
  EXPECT_FALSE(is_echo_impl_alive_);
}

TEST_F(DocumentServiceTest, RenderFrameDeleted) {
  // Needs to create a child frame so we can delete it using DetachFrame()
  // because it is not allowed to detach the main frame.
  RenderFrameHost* child_rfh = AddChildFrame(main_rfh(), GURL(kBarOrigin));
  CreateEchoImpl(*child_rfh);
  DetachFrame(child_rfh);
  EXPECT_FALSE(is_echo_impl_alive_);
}

TEST_F(DocumentServiceTest, DidFinishNavigation) {
  // When a page enters the BackForwardCache, the RenderFrameHost is not
  // deleted.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_REQUIRES_NO_CACHING);
  CreateEchoImpl(*main_rfh());
  SimulateNavigation(main_rfh(), GURL(kBarOrigin));
  EXPECT_FALSE(is_echo_impl_alive_);
}

TEST_F(DocumentServiceTest, SameDocumentNavigation) {
  CreateEchoImpl(*main_rfh());

  RenderFrameHost* previous_main_rfh = main_rfh();
  // Must use the same origin to simulate same document navigation.
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL(kFooOrigin), main_rfh());
  navigation_simulator->CommitSameDocument();
  DCHECK_EQ(previous_main_rfh, navigation_simulator->GetFinalRenderFrameHost());

  EXPECT_TRUE(is_echo_impl_alive_);
}

TEST_F(DocumentServiceTest, FailedNavigation) {
  CreateEchoImpl(*main_rfh());

  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL(kFooOrigin), main_rfh());
  navigation_simulator->Fail(net::ERR_TIMED_OUT);
  navigation_simulator->CommitErrorPage();

  EXPECT_FALSE(is_echo_impl_alive_);
}

TEST_F(DocumentServiceTest, DeleteContents) {
  CreateEchoImpl(*main_rfh());
  DeleteContents();
  EXPECT_FALSE(is_echo_impl_alive_);
}

}  // namespace content
