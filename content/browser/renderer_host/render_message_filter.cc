// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include <errno.h>
#include <string.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
namespace {

void GotHasGpuProcess(RenderMessageFilter::HasGpuProcessCallback callback,
                      bool has_gpu) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), has_gpu));
}

void GetHasGpuProcess(RenderMessageFilter::HasGpuProcessCallback callback) {
  GpuProcessHost::GetHasGpuProcess(
      base::BindOnce(GotHasGpuProcess, std::move(callback)));
}

}  // namespace

RenderMessageFilter::RenderMessageFilter(
    int render_process_id,
    BrowserContext* browser_context,
    RenderWidgetHelper* render_widget_helper)
    : BrowserAssociatedInterface<mojom::RenderMessageFilter>(this),
      render_widget_helper_(render_widget_helper),
      render_process_id_(render_process_id) {
  if (render_widget_helper)
    render_widget_helper_->Init(render_process_id_);
}

RenderMessageFilter::~RenderMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

bool RenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void RenderMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void RenderMessageFilter::GenerateRoutingID(
    GenerateRoutingIDCallback callback) {
  std::move(callback).Run(render_widget_helper_->GetNextRoutingID());
}

void RenderMessageFilter::GenerateFrameRoutingID(
    GenerateFrameRoutingIDCallback callback) {
  int32_t routing_id = render_widget_helper_->GetNextRoutingID();
  auto frame_token = blink::LocalFrameToken();
  auto devtools_frame_token = base::UnguessableToken::Create();
  auto document_token = blink::DocumentToken();
  render_widget_helper_->StoreNextFrameRoutingID(
      routing_id, frame_token, devtools_frame_token, document_token);
  std::move(callback).Run(routing_id, frame_token, devtools_frame_token,
                          document_token);
}

void RenderMessageFilter::HasGpuProcess(HasGpuProcessCallback callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(GetHasGpuProcess, std::move(callback)));
}

}  // namespace content
