// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/eye_dropper_chooser_impl.h"

#include <memory>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace content {
namespace {

// A stand-in for a real eye dropper window.
class FakeEyeDropper : public EyeDropper {};

// Records eye dropper open requests and hands back fake eye droppers, capturing
// the listener so a test can simulate the user finishing a selection.
class FakeEyeDropperDelegate : public WebContentsDelegate {
 public:
  std::unique_ptr<EyeDropper> OpenEyeDropper(
      RenderFrameHost* frame,
      EyeDropperListener* listener) override {
    ++open_count_;
    last_listener_ = listener;
    return std::make_unique<FakeEyeDropper>();
  }

  int open_count() const { return open_count_; }
  EyeDropperListener* last_listener() { return last_listener_; }
  void clear_last_listener() { last_listener_ = nullptr; }

 private:
  int open_count_ = 0;
  raw_ptr<EyeDropperListener> last_listener_ = nullptr;
};

using ChooseFuture = base::test::TestFuture<bool, uint32_t>;

}  // namespace

class EyeDropperChooserImplTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    web_contents()->SetDelegate(&delegate_);
    NavigateAndCommit(GURL("https://example.test"));
  }

  void TearDown() override {
    if (web_contents()) {
      web_contents()->SetDelegate(nullptr);
    }
    // The delegate holds a back-pointer to the listener (an
    // EyeDropperChooserImpl DocumentService). Clear it before the WebContents
    // teardown destroys those services, so the delegate's raw_ptr does not
    // dangle.
    delegate_.clear_last_listener();
    RenderViewHostTestHarness::TearDown();
  }

  // Binds a new EyeDropperChooser, first granting the transient user activation
  // that EyeDropperChooserImpl::Create consumes.
  mojo::Remote<blink::mojom::EyeDropperChooser> CreateChooser() {
    std::ignore =
        static_cast<RenderFrameHostImpl*>(main_rfh())
            ->frame_tree_node()
            ->UpdateUserActivationState(
                blink::mojom::UserActivationUpdateType::kNotifyActivation,
                blink::mojom::UserActivationNotificationType::kTest);
    mojo::Remote<blink::mojom::EyeDropperChooser> chooser;
    EyeDropperChooserImpl::Create(main_rfh(),
                                  chooser.BindNewPipeAndPassReceiver());
    return chooser;
  }

 protected:
  FakeEyeDropperDelegate delegate_;
};

// A second eye dropper requested while one is already open must be rejected
// rather than stacked. https://crbug.com/40280878
TEST_F(EyeDropperChooserImplTest, SecondConcurrentEyeDropperIsRejected) {
  mojo::Remote<blink::mojom::EyeDropperChooser> first = CreateChooser();
  ChooseFuture first_future;
  first->Choose(first_future.GetCallback());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return delegate_.open_count() == 1; }));

  // The first eye dropper opened and is still waiting for a color.
  EXPECT_FALSE(first_future.IsReady());

  mojo::Remote<blink::mojom::EyeDropperChooser> second = CreateChooser();
  ChooseFuture second_future;
  second->Choose(second_future.GetCallback());

  // The second request is rejected, and no additional eye dropper is opened.
  EXPECT_TRUE(second_future.Wait());
  EXPECT_FALSE(second_future.Get<0>());
  EXPECT_EQ(delegate_.open_count(), 1);
  EXPECT_FALSE(first_future.IsReady());
}

// Once the active eye dropper finishes, a new one may be opened.
TEST_F(EyeDropperChooserImplTest, EyeDropperCanReopenAfterClose) {
  mojo::Remote<blink::mojom::EyeDropperChooser> first = CreateChooser();
  ChooseFuture first_future;
  first->Choose(first_future.GetCallback());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return delegate_.open_count() == 1; }));

  // Simulate the user picking a color, which closes the active eye dropper.
  delegate_.last_listener()->ColorSelected(SK_ColorRED);
  EXPECT_TRUE(first_future.Wait());
  EXPECT_TRUE(first_future.Get<0>());

  // A subsequent eye dropper can now open.
  mojo::Remote<blink::mojom::EyeDropperChooser> second = CreateChooser();
  ChooseFuture second_future;
  second->Choose(second_future.GetCallback());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return delegate_.open_count() == 2; }));
  EXPECT_FALSE(second_future.IsReady());
}

// A second Choose() on the same pipe is renderer misbehavior so it must be
// reported as a bad message. https://crbug.com/40280878
TEST_F(EyeDropperChooserImplTest, ReentrantChooseOnSameChooserIsBadMessage) {
  mojo::Remote<blink::mojom::EyeDropperChooser> chooser = CreateChooser();
  ChooseFuture first_future;
  chooser->Choose(first_future.GetCallback());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return delegate_.open_count() == 1; }));

  // The reentrant Choose() below deletes the chooser (the listener), so drop
  // the delegate's back-pointer now, while it is still valid.
  delegate_.clear_last_listener();

  mojo::test::BadMessageObserver bad_message_observer;
  ChooseFuture second_future;
  chooser->Choose(second_future.GetCallback());
  EXPECT_EQ(
      "EyeDropperChooser::Choose() called while a selection was already in "
      "progress.",
      bad_message_observer.WaitForBadMessage());
}

// Inactive (e.g. bfcached or pending-deletion) documents must not bind an
// EyeDropperChooser or consume the active document's transient user activation.
TEST_F(EyeDropperChooserImplTest, InactiveDocumentIsRejected) {
  // 1. Start with the initial active document.
  RenderFrameHostWrapper old_rfh(main_rfh());
  // 2. Navigate away. The old document goes into the Back/Forward Cache
  //    (or becomes pending-deletion), becoming inactive.
  NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example2.test"), main_rfh());

  // Verify the old document survived the navigation but is no longer active.
  ASSERT_FALSE(old_rfh.IsDestroyed());
  EXPECT_FALSE(old_rfh->IsActive());
  // 3. The NEW active document gets transient user activation.
  auto* new_rfh = static_cast<RenderFrameHostImpl*>(main_rfh());
  EXPECT_NE(old_rfh.get(), new_rfh);
  EXPECT_TRUE(new_rfh->IsActive());

  std::ignore = new_rfh->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(new_rfh->HasTransientUserActivation());
  // 4. The malicious inactive document attempts to open the chooser.
  mojo::Remote<blink::mojom::EyeDropperChooser> chooser;
  EyeDropperChooserImpl::Create(old_rfh.get(),
                                chooser.BindNewPipeAndPassReceiver());
  // 5. The receiver is dropped because the calling frame is inactive.
  base::test::TestFuture<void> disconnect_future;
  chooser.set_disconnect_handler(disconnect_future.GetCallback());
  EXPECT_TRUE(disconnect_future.Wait());
  EXPECT_FALSE(chooser.is_connected());
  EXPECT_EQ(delegate_.open_count(), 0);
  // 6. The new active document's transient user activation must NOT have been
  // consumed by the inactive document's request.
  EXPECT_TRUE(new_rfh->HasTransientUserActivation());
}

}  // namespace content
