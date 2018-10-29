// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_TEST_H_

#include <gtest/gtest.h>
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_network.h"
#include "third_party/blink/renderer/core/testing/sim/sim_page.h"
#include "third_party/blink/renderer/core/testing/sim/sim_web_frame_client.h"
#include "third_party/blink/renderer/core/testing/sim/sim_web_view_client.h"

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

  void LoadURL(const String& url);

  // WebView is created after SetUp to allow test to customize
  // web runtime features.
  // These methods should be accessed inside test body after a call to SetUp.
  LocalDOMWindow& Window();
  SimPage& Page();
  Document& GetDocument();
  WebViewImpl& WebView();
  WebLocalFrameImpl& MainFrame();
  const SimWebViewClient& WebViewClient() const;
  SimCompositor& Compositor();

  Vector<String>& ConsoleMessages() { return console_messages_; }

  void SetEffectiveConnectionTypeForTesting(WebEffectiveConnectionType);

 private:
  friend class SimWebFrameClient;

  void AddConsoleMessage(const String&);

  SimNetwork network_;
  SimCompositor compositor_;
  SimWebFrameClient web_frame_client_;
  SimWebViewClient web_view_client_;
  SimPage page_;
  frame_test_helpers::WebViewHelper web_view_helper_;

  Vector<String> console_messages_;
};

}  // namespace blink

#endif
