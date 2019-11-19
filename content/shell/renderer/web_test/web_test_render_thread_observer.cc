// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"

#include "content/public/common/content_client.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/test/web_test_support.h"
#include "content/shell/common/web_test/web_test_messages.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_test_runner.h"

namespace content {

namespace {
WebTestRenderThreadObserver* g_instance = nullptr;
}

// static
WebTestRenderThreadObserver* WebTestRenderThreadObserver::GetInstance() {
  return g_instance;
}

WebTestRenderThreadObserver::WebTestRenderThreadObserver() {
  CHECK(!g_instance);
  g_instance = this;
  RenderThread::Get()->AddObserver(this);
  EnableRendererWebTestMode();

  test_interfaces_.reset(new test_runner::WebTestInterfaces);
  test_interfaces_->ResetAll();
}

WebTestRenderThreadObserver::~WebTestRenderThreadObserver() {
  CHECK(g_instance == this);
  g_instance = nullptr;
}

bool WebTestRenderThreadObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebTestRenderThreadObserver, message)
    IPC_MESSAGE_HANDLER(WebTestMsg_ReplicateWebTestRuntimeFlagsChanges,
                        OnReplicateWebTestRuntimeFlagsChanges)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebTestRenderThreadObserver::OnReplicateWebTestRuntimeFlagsChanges(
    const base::DictionaryValue& changed_web_test_runtime_flags) {
  test_interfaces()->TestRunner()->ReplicateWebTestRuntimeFlagsChanges(
      changed_web_test_runtime_flags);
}

}  // namespace content
