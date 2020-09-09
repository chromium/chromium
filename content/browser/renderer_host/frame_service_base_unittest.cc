// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/frame_service_base.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/echo.mojom.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

// Unit test for FrameServiceBase in content/public/browser.

namespace content {

namespace {

const char kFooOrigin[] = "https://foo.com";
const char kBarOrigin[] = "https://bar.com";

// Subclass of FrameServiceBase for test.
class EchoImpl final : public FrameServiceBase<mojom::Echo> {
 public:
  EchoImpl(RenderFrameHost* render_frame_host,
           mojo::PendingReceiver<mojom::Echo> receiver,
           base::OnceClosure destruction_cb)
      : FrameServiceBase(render_frame_host, std::move(receiver)),
        destruction_cb_(std::move(destruction_cb)) {}
  ~EchoImpl() final { std::move(destruction_cb_).Run(); }

  // mojom::Echo implementation
  void EchoString(const std::string& input, EchoStringCallback callback) final {
    std::move(callback).Run(input);
  }

 private:
  base::OnceClosure destruction_cb_;
};

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

class FrameServiceBaseTest : public RenderViewHostTestHarness {
 protected:
  void SetUp() final {
    RenderViewHostTestHarness::SetUp();
    Initialize();
  }

  void Initialize() {
    RenderFrameHost* main_rfh = web_contents()->GetMainFrame();
    RenderFrameHostTester::For(main_rfh)->InitializeRenderFrameIfNeeded();
    main_rfh_ = SimulateNavigation(main_rfh, GURL(kFooOrigin));
  }

  void CreateEchoImpl(RenderFrameHost* rfh) {
    DCHECK(!is_echo_impl_alive_);
    new EchoImpl(rfh, echo_remote_.BindNewPipeAndPassReceiver(),
                 base::BindOnce(&FrameServiceBaseTest::OnEchoImplDestructed,
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

  RenderFrameHost* main_rfh_ = nullptr;
  mojo::Remote<mojom::Echo> echo_remote_;
  bool is_echo_impl_alive_ = false;
};

TEST_F(FrameServiceBaseTest, ConnectionError) {
  CreateEchoImpl(main_rfh_);
  ResetConnection();
  EXPECT_FALSE(is_echo_impl_alive_);
}

TEST_F(FrameServiceBaseTest, RenderFrameDeleted) {
  // Needs to create a child frame so we can delete it using DetachFrame()
  // because it is not allowed to detach the main frame.
  RenderFrameHost* child_rfh = AddChildFrame(main_rfh_, GURL(kBarOrigin));
  CreateEchoImpl(child_rfh);
  DetachFrame(child_rfh);
  EXPECT_FALSE(is_echo_impl_alive_);
}

TEST_F(FrameServiceBaseTest, DidFinishNavigation) {
  // When a page enters the BackForwardCache, the RenderFrameHost is not
  // deleted.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      BackForwardCache::TEST_ASSUMES_NO_CACHING);
  CreateEchoImpl(main_rfh_);
  SimulateNavigation(main_rfh_, GURL(kBarOrigin));
  EXPECT_FALSE(is_echo_impl_alive_);
}

TEST_F(FrameServiceBaseTest, SameDocumentNavigation) {
  CreateEchoImpl(main_rfh_);

  // Must use the same origin to simulate same document navigation.
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kFooOrigin), main_rfh_);
  navigation_simulator->CommitSameDocument();
  DCHECK_EQ(main_rfh_, navigation_simulator->GetFinalRenderFrameHost());

  EXPECT_TRUE(is_echo_impl_alive_);
}

TEST_F(FrameServiceBaseTest, FailedNavigation) {
  CreateEchoImpl(main_rfh_);

  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(GURL(kFooOrigin), main_rfh_);
  navigation_simulator->Fail(net::ERR_TIMED_OUT);
  navigation_simulator->CommitErrorPage();

  EXPECT_FALSE(is_echo_impl_alive_);
}

TEST_F(FrameServiceBaseTest, DeleteContents) {
  CreateEchoImpl(main_rfh_);
  DeleteContents();
  EXPECT_FALSE(is_echo_impl_alive_);
}

}  // namespace content
