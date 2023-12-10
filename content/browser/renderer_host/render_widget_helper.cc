// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_helper.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

typedef std::map<int, RenderWidgetHelper*> WidgetHelperMap;
base::LazyInstance<WidgetHelperMap>::DestructorAtExit g_widget_helpers =
    LAZY_INSTANCE_INITIALIZER;

void AddWidgetHelper(int render_process_id,
                     const scoped_refptr<RenderWidgetHelper>& widget_helper) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // We don't care if RenderWidgetHelpers overwrite an existing process_id. Just
  // want this to be up to date.
  g_widget_helpers.Get()[render_process_id] = widget_helper.get();
}

}  // namespace

RenderWidgetHelper::FrameTokens::FrameTokens(
    int32_t routing_id,
    const base::UnguessableToken& devtools_frame_token,
    const blink::DocumentToken& document_token)
    : routing_id(routing_id),
      devtools_frame_token(devtools_frame_token),
      document_token(document_token) {}

RenderWidgetHelper::FrameTokens::FrameTokens(const FrameTokens& other) =
    default;

RenderWidgetHelper::FrameTokens& RenderWidgetHelper::FrameTokens::operator=(
    const FrameTokens& other) = default;

RenderWidgetHelper::FrameTokens::~FrameTokens() = default;

RenderWidgetHelper::RenderWidgetHelper() : render_process_id_(-1) {}

RenderWidgetHelper::~RenderWidgetHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Delete this RWH from the map if it is found.
  WidgetHelperMap& widget_map = g_widget_helpers.Get();
  auto it = widget_map.find(render_process_id_);
  if (it != widget_map.end() && it->second == this)
    widget_map.erase(it);
}

void RenderWidgetHelper::Init(int render_process_id) {
  render_process_id_ = render_process_id;

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&AddWidgetHelper, render_process_id_,
                                base::WrapRefCounted(this)));
}

int RenderWidgetHelper::GetNextRoutingID() {
  int next_routing_id = next_routing_id_.GetNext();
  // Routing IDs are also used for `FrameSinkId` values from the browser.
  // The must be in the range of [0, INT_MAX] as the renderer generates
  // the rest of the range.
  CHECK_LT(next_routing_id, std::numeric_limits<int32_t>::max());
  return next_routing_id + 1;
}

bool RenderWidgetHelper::TakeStoredDataForFrameToken(
    const blink::LocalFrameToken& frame_token,
    int32_t& routing_id,
    base::UnguessableToken& devtools_frame_token,
    blink::DocumentToken& document_token) {
  base::AutoLock lock(frame_token_map_lock_);
  auto iter = frame_storage_map_.find(frame_token);
  if (iter == frame_storage_map_.end()) {
    return false;
  }
  routing_id = iter->second.routing_id;
  devtools_frame_token = iter->second.devtools_frame_token;
  document_token = iter->second.document_token;
  frame_storage_map_.erase(iter);
  return true;
}

void RenderWidgetHelper::StoreNextFrameRoutingID(
    int32_t routing_id,
    const blink::LocalFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const blink::DocumentToken& document_token) {
  base::AutoLock lock(frame_token_map_lock_);
  bool result =
      frame_storage_map_
          .emplace(frame_token, FrameTokens(routing_id, devtools_frame_token,
                                            document_token))
          .second;
  DCHECK(result);
}

}  // namespace content
