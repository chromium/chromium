// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_TEST_H_

#include <gtest/gtest.h>
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_network.h"
#include "third_party/blink/renderer/core/testing/sim/sim_page.h"
#include "third_party/blink/renderer/core/testing/sim/sim_web_frame_widget.h"

namespace blink {

class WebViewImpl;
class WebLocalFrameImpl;
class Document;
class LocalDOMWindow;

class SimTest : public testing::Test {
 protected:
  SimTest();
  ~SimTest() override;

  void SetUp() override;
  void TearDown() override;

  // Create a remote frame as the main frame and create a local child frame.
  void InitializeRemote();

  // Create a WebView with a main frame being a fenced frame root.
  void InitializeFencedFrameRoot(mojom::blink::FencedFrameMode mode);

  // Load URL in the local frame root.
  void LoadURL(const String& url);

  // WebView is created after SetUp to allow test to customize
  // web runtime features.
  // These methods should be accessed inside test body after a call to SetUp.
  LocalDOMWindow& Window();
  SimPage& GetPage();
  Document& GetDocument();
  WebViewImpl& WebView();
  WebLocalFrameImpl& MainFrame();
  WebLocalFrameImpl& LocalFrameRoot();
  frame_test_helpers::TestWebFrameClient& WebFrameClient();
  SimWebFrameWidget& GetWebFrameWidget();
  SimCompositor& Compositor();

  Vector<String>& ConsoleMessages();
  void ResizeView(const gfx::Size&);

  // Creates a SimWebFrameWidget. Subclasses can override this if the
  // wish to create their own.
  virtual SimWebFrameWidget* CreateSimWebFrameWidget(
      base::PassKey<WebLocalFrame> pass_key,
      CrossVariantMojoAssociatedRemote<
          mojom::blink::FrameWidgetHostInterfaceBase> frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const viz::FrameSinkId& frame_sink_id,
      bool hidden,
      bool never_composited,
      bool is_for_child_local_root,
      bool is_for_nested_main_frame,
      bool is_for_scalable_page,
      SimCompositor* compositor);

 private:
  frame_test_helpers::TestWebFrameWidget* CreateTestWebFrameWidget(
      base::PassKey<WebLocalFrame> pass_key,
      CrossVariantMojoAssociatedRemote<
          mojom::blink::FrameWidgetHostInterfaceBase> frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::blink::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::blink::WidgetInterfaceBase>
          widget,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const viz::FrameSinkId& frame_sink_id,
      bool hidden,
      bool never_composited,
      bool is_for_child_local_root,
      bool is_for_nested_main_frame,
      bool is_for_scalable_page);

  // These are unique_ptrs in order to destroy them in TearDown. Subclasses
  // may override Platform::Current() and these must shutdown before the
  // subclass destructor.
  std::unique_ptr<SimNetwork> network_;
  std::unique_ptr<SimCompositor> compositor_;
  std::unique_ptr<frame_test_helpers::TestWebFrameClient> web_frame_client_;
  std::unique_ptr<SimPage> page_;
  std::unique_ptr<frame_test_helpers::WebViewHelper> web_view_helper_;
  UntracedMember<WebLocalFrameImpl> local_frame_root_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_TEST_H_
