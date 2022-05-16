// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/anchor_element_interaction_host.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/loader/anchor_element_interaction_tracker.h"
#include "third_party/blink/renderer/core/loader/anchor_element_listener.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

class MockAnchorElementInteractionHost
    : public mojom::blink::AnchorElementInteractionHost {
 public:
  explicit MockAnchorElementInteractionHost(
      mojo::PendingReceiver<mojom::blink::AnchorElementInteractionHost>
          pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  absl::optional<KURL> url_received_ = absl::nullopt;

 private:
  void OnPointerDown(const KURL& target) override { url_received_ = target; }

 private:
  mojo::Receiver<mojom::blink::AnchorElementInteractionHost> receiver_{this};
};

class AnchorElementInteractionTest : public SimTest {
 public:
 protected:
  void SetUp() override {
    SimTest::SetUp();

    feature_list_.InitAndEnableFeature(features::kAnchorElementInteraction);

    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::AnchorElementInteractionHost::Name_,
        WTF::BindRepeating(&AnchorElementInteractionTest::Bind,
                           WTF::Unretained(this)));
  }

  void TearDown() override {
    MainFrame().GetFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::AnchorElementInteractionHost::Name_, {});
    hosts_.clear();
    SimTest::TearDown();
  }

  void Bind(mojo::ScopedMessagePipeHandle message_pipe_handle) {
    auto host = std::make_unique<MockAnchorElementInteractionHost>(
        mojo::PendingReceiver<mojom::blink::AnchorElementInteractionHost>(
            std::move(message_pipe_handle)));
    hosts_.push_back(std::move(host));
  }

  base::test::ScopedFeatureList feature_list_;
  std::vector<std::unique_ptr<MockAnchorElementInteractionHost>> hosts_;
};

TEST_F(AnchorElementInteractionTest, InvalidHref) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a id='anchor1' href='about:blank'>example</a>
    <script>
      const a = document.getElementById('anchor1');
      var event = new PointerEvent('pointerdown', {isPrimary: true});
      a.dispatchEvent(event);
    </script>
  )HTML");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
}

TEST_F(AnchorElementInteractionTest, NonPointerEventType) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a id='anchor1' href='https://anchor1.com/'>example</a>
    <script>
      const a = document.getElementById('anchor1');
      var event = new Event('pointerdown');
      a.dispatchEvent(event);
    </script>
  )HTML");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
}

TEST_F(AnchorElementInteractionTest, NonPrimary) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a id='anchor1' href='https://anchor1.com/'>example</a>
    <script>
      const a = document.getElementById('anchor1');
      var event = new PointerEvent('pointerdown', {isPrimary: false});
      a.dispatchEvent(event);
    </script>
  )HTML");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
}

TEST_F(AnchorElementInteractionTest, RightClick) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a id='anchor1' href='https://anchor1.com/'>example</a>
    <script>
      const a = document.getElementById('anchor1');
      var event = new PointerEvent('pointerdown', {
        isPrimary: true,
        button: 2
      });
      a.dispatchEvent(event);
    </script>
  )HTML");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
}

TEST_F(AnchorElementInteractionTest, NestedAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a id='anchor1' href='https://anchor1.com/'><a id='anchor2'
      href='https://anchor2.com/'></a></a>
    <script>
      const a = document.getElementById('anchor2');
      var event = new PointerEvent('pointerdown', {isPrimary: true});
      a.dispatchEvent(event);
    </script>
  )HTML");
  base::RunLoop().RunUntilIdle();
  KURL expected_url = KURL("https://anchor2.com/");
  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_TRUE(url_received.has_value());
  EXPECT_EQ(expected_url, url_received);
}

TEST_F(AnchorElementInteractionTest, NestedDivAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a id='anchor1' href='https://anchor1.com/'><div
      id='div1id'></div></a>
    <script>
      const a = document.getElementById('div1id');
      var event = new PointerEvent('pointerdown', {isPrimary: true});
      a.dispatchEvent(event);
    </script>
  )HTML");
  base::RunLoop().RunUntilIdle();
  KURL expected_url = KURL("https://anchor1.com/");
  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_TRUE(url_received.has_value());
  EXPECT_EQ(expected_url, url_received);
}

TEST_F(AnchorElementInteractionTest, MultipleNestedAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a id='anchor1' href='https://anchor1.com/'><p id='paragraph1id'><div
      id='div1id'><div id='div2id'></div></div></p></a>
    <script>
      const a = document.getElementById('div2id');
      var event = new PointerEvent('pointerdown', {isPrimary: true});
      a.dispatchEvent(event);
    </script>
  )HTML");
  base::RunLoop().RunUntilIdle();
  KURL expected_url = KURL("https://anchor1.com/");
  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_TRUE(url_received.has_value());
  EXPECT_EQ(expected_url, url_received);
}

TEST_F(AnchorElementInteractionTest, NoAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <div id='div1id'></div>
    <script>
      const a = document.getElementById('div2id');
      var event = new PointerEvent('pointerdown', {isPrimary: true});
      a.dispatchEvent(event);
    </script>
  )HTML");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_FALSE(url_received.has_value());
}

TEST_F(AnchorElementInteractionTest, OneAnchorElementCheck) {
  String source("https://example.com/p1");
  SimRequest main_resource(source, "text/html");
  LoadURL(source);
  main_resource.Complete(R"HTML(
    <a id="anchor1" href="https://anchor1.com/">foo</a>
    <script>
      const a = document.getElementById('anchor1');
      var event = new PointerEvent('pointerdown', {isPrimary: true});
      a.dispatchEvent(event);
    </script>
  )HTML");
  base::RunLoop().RunUntilIdle();
  KURL expected_url = KURL("https://anchor1.com/");
  EXPECT_EQ(1u, hosts_.size());
  absl::optional<KURL> url_received = hosts_[0]->url_received_;
  EXPECT_TRUE(url_received.has_value());
  EXPECT_EQ(expected_url, url_received);
}

}  // namespace blink
