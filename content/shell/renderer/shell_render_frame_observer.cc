// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_render_frame_observer.h"

#include "base/command_line.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/shell/common/render_frame_test_helper.mojom.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/render_frame_test_helper.h"
#include "third_party/blink/public/web/web_testing_support.h"

namespace content {

ShellRenderFrameObserver::ShellRenderFrameObserver(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

ShellRenderFrameObserver::~ShellRenderFrameObserver() = default;

void ShellRenderFrameObserver::OnDestruct() {
  delete this;
}

void ShellRenderFrameObserver::DidClearWindowObject() {
  auto& cmd = *base::CommandLine::ForCurrentProcess();
  if (cmd.HasSwitch(switches::kExposeInternalsForTesting)) {
    blink::WebTestingSupport::InjectInternalsObject(
        render_frame()->GetWebFrame());
  }
}

void ShellRenderFrameObserver::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  if (interface_name == mojom::RenderFrameTestHelper::Name_) {
    RenderFrameTestHelper::Create(
        *render_frame(), mojo::PendingReceiver<mojom::RenderFrameTestHelper>(
                             std::move(*interface_pipe)));
    return;
  }
}

}  // namespace content
