// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/visual_debugger_handler.h"

#include <string.h>
#include <algorithm>

#include "base/json/json_writer.h"
#include "base/task/bind_post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/devtools/protocol/base_string_adapter.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace content {
namespace protocol {

static const int sMaxJsonDepth = 1000;
VisualDebuggerHandler::VisualDebuggerHandler()
    : DevToolsDomainHandler(VisualDebugger::Metainfo::domainName) {}

VisualDebuggerHandler::~VisualDebuggerHandler() {
  StopStream();
}

void VisualDebuggerHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<VisualDebugger::Frontend>(dispatcher->channel());
  VisualDebugger::Dispatcher::wire(dispatcher, this);
}

DispatchResponse VisualDebuggerHandler::FilterStream(
    std::unique_ptr<protocol::DictionaryValue> in_filter) {
  std::unique_ptr<base::Value> dict =
      toBaseValue(in_filter.get(), sMaxJsonDepth);

  GpuProcessHost::CallOnIO(
      GPU_PROCESS_KIND_SANDBOXED,
      /*force_create=*/false,
      base::BindOnce(
          [](base::Value json, GpuProcessHost* host) {
            host->gpu_host()->FilterVisualDebugStream(std::move(json));
          },
          std::move(*dict.get())));

  return DispatchResponse::Success();
}

DispatchResponse VisualDebuggerHandler::StartStream() {
  enabled_ = true;
  GpuProcessHost::CallOnIO(
      GPU_PROCESS_KIND_SANDBOXED,
      /*force_create=*/false,
      base::BindOnce(
          [](base::RepeatingCallback<void(base::Value)> callback,
             GpuProcessHost* host) {
            host->gpu_host()->StartVisualDebugStream(callback);
          },
          base::BindPostTask(
              base::ThreadTaskRunnerHandle::Get(),
              base::BindRepeating(&VisualDebuggerHandler::OnFrameResponse,
                                  weak_ptr_factory_.GetWeakPtr()),
              FROM_HERE)));
  return DispatchResponse::Success();
}

void VisualDebuggerHandler::OnFrameResponse(base::Value json) {
  // This should be called via the 'BindPostTask' in 'StartStream' function
  // above and thus should be in the correct thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::unique_ptr<protocol::DictionaryValue> dict =
      protocol::DictionaryValue::cast(toProtocolValue(&json, sMaxJsonDepth));
  frontend_->FrameResponse(std::move(dict));
}

DispatchResponse VisualDebuggerHandler::StopStream() {
  if (enabled_) {
    GpuProcessHost::CallOnIO(GPU_PROCESS_KIND_SANDBOXED,
                             /*force_create=*/false,
                             base::BindOnce([](GpuProcessHost* host) {
                               host->gpu_host()->StopVisualDebugStream();
                             }));
  }
  enabled_ = false;
  return DispatchResponse::Success();
}
}  // namespace protocol
}  // namespace content
