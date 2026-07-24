// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_ui_extension_data.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"

namespace content {

// static
void WebUIExtensionData::Create(
    RenderFrame* render_frame,
    mojo::PendingAssociatedReceiver<mojom::WebUI> receiver,
    mojo::PendingAssociatedRemote<mojom::WebUIHost> remote) {
  scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr;
  // Binding to `kInternalNavigationAssociated` aligns the `mojom::WebUI`
  // receiver with `FrameBindingsControl` and navigation commit tasks, ensuring
  // properties set via `RenderFrameHost::SetWebUIProperty()` populate
  // `variable_map_` before document parsing and JS custom element constructors
  // evaluate. The default `nullptr` task runner uses
  // `kMainThreadTaskQueueDefault`, causing Mojo task-hopping delays and
  // startup timing races.
  //
  // `BindWebUI()` executes before navigation commit when frame URL state is
  // unknown. Because `SetProperty()` is a passive map insertion without JS or
  // DOM side effects, using this task runner for all WebUIs is safe and
  // eliminates timing races for property consumers without affecting
  // non-consumers.
  if (base::FeatureList::IsEnabled(blink::features::kInitialWebUISurfaceSync)) {
    task_runner = render_frame->GetTaskRunner(
        blink::TaskType::kInternalNavigationAssociated);
  }
  mojo::MakeSelfOwnedAssociatedReceiver(
      base::WrapUnique(new WebUIExtensionData(render_frame, std::move(remote))),
      std::move(receiver), std::move(task_runner));
}

WebUIExtensionData::WebUIExtensionData(
    RenderFrame* render_frame,
    mojo::PendingAssociatedRemote<mojom::WebUIHost> remote)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<WebUIExtensionData>(render_frame),
      remote_(std::move(remote)) {}

WebUIExtensionData::~WebUIExtensionData() = default;

std::string WebUIExtensionData::GetValue(const std::string& key) const {
  auto it = variable_map_.find(key);
  if (it == variable_map_.end())
    return std::string();
  return it->second;
}

void WebUIExtensionData::SendMessage(const std::string& message,
                                     base::ListValue args) {
  remote_->Send(message, std::move(args));
}

void WebUIExtensionData::SetProperty(const std::string& name,
                                     const std::string& value) {
  variable_map_[name] = value;
}

void WebUIExtensionData::OnDestruct() {}

}  // namespace content
