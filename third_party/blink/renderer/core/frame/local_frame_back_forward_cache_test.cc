// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom-blink.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/fake_local_frame_host.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class TestLocalFrameBackForwardCacheClient
    : public mojom::blink::BackForwardCacheControllerHost {
 public:
  explicit TestLocalFrameBackForwardCacheClient(
      blink::AssociatedInterfaceProvider* provider) {
    provider->OverrideBinderForTesting(
        mojom::blink::BackForwardCacheControllerHost::Name_,
        WTF::BindRepeating(
            [](TestLocalFrameBackForwardCacheClient* parent,
               mojo::ScopedInterfaceEndpointHandle handle) {
              parent->receiver_.Bind(
                  mojo::PendingAssociatedReceiver<
                      mojom::blink::BackForwardCacheControllerHost>(
                      std::move(handle)));
            },
            base::Unretained(this)));
    fake_local_frame_host_.Init(provider);
  }

  ~TestLocalFrameBackForwardCacheClient() override = default;

  void EvictFromBackForwardCache(
      mojom::blink::RendererEvictionReason,
      mojom::blink::ScriptSourceLocationPtr) override {
    quit_closure_.Run();
  }

  void DidChangeBackForwardCacheDisablingFeatures(
      Vector<mojom::blink::BlockingDetailsPtr> details) override {}

  void WaitUntilEvictedFromBackForwardCache() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<
            mojom::blink::BackForwardCacheControllerHost>(std::move(handle)));
  }
  FakeLocalFrameHost fake_local_frame_host_;
  mojo::AssociatedReceiver<mojom::blink::BackForwardCacheControllerHost>
      receiver_{this};
  base::RepeatingClosure quit_closure_;
};

class LocalFrameBackForwardCacheTest : public testing::Test,
                                       private ScopedBackForwardCacheForTest {
 public:
  LocalFrameBackForwardCacheTest() : ScopedBackForwardCacheForTest(true) {}

 private:
  test::TaskEnvironment task_environment_;
};

// Tests a frame in the back-forward cache (a.k.a. bfcache) is evicted on
// JavaScript execution at a microtask. Eviction is necessary to ensure that the
// frame state is immutable when the frame is in the bfcache.
// (https://www.chromestatus.com/feature/5815270035685376).
TEST_F(LocalFrameBackForwardCacheTest, EvictionOnV8ExecutionAtMicrotask) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kBackForwardCacheDWCOnJavaScriptExecution);
  frame_test_helpers::TestWebFrameClient web_frame_client;
  TestLocalFrameBackForwardCacheClient frame_host(
      web_frame_client.GetRemoteNavigationAssociatedInterfaces());
  frame_test_helpers::WebViewHelper web_view_helper;
  web_view_helper.Initialize(
      &web_frame_client, nullptr,
      [](WebSettings* settings) { settings->SetJavaScriptEnabled(true); });
  web_view_helper.Resize(gfx::Size(640, 480));

  LocalFrame* frame = web_view_helper.GetWebView()->MainFrameImpl()->GetFrame();

  // Freeze the frame and hook eviction.
  frame->GetPage()->GetPageScheduler()->SetPageVisible(false);
  frame->GetPage()->GetPageScheduler()->SetPageFrozen(true);
  frame->GetPage()->GetPageScheduler()->SetPageBackForwardCached(true);
  frame->HookBackForwardCacheEviction();

  auto* script_state = ToScriptStateForMainWorld(frame);
  ScriptState::Scope scope(script_state);

  // There are two types of microtasks:
  //   1) V8 function
  //   2) C++ closure
  // The case 1) should never happen when the frame is in bfcache. On the other
  // hand, the case 2) can happen. See https://crbug.com/994169
  frame->DomWindow()->GetAgent()->event_loop()->EnqueueMicrotask(WTF::BindOnce(
      [](LocalFrame* frame) {
        ClassicScript::CreateUnspecifiedScript("console.log('hi');")
            ->RunScript(frame->DomWindow());
      },
      WrapWeakPersistent(frame)));
  frame_host.WaitUntilEvictedFromBackForwardCache();
}

}  // namespace blink
