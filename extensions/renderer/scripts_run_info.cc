// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/scripts_run_info.h"

#include "base/metrics/histogram_macros.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

ScriptsRunInfo::ScriptsRunInfo(content::RenderFrame* render_frame,
                               UserScript::RunLocation location)
    : num_css(0u),
      num_js(0u),
      num_blocking_js(0u),
      routing_id_(render_frame->GetRoutingID()),
      run_location_(location),
      frame_url_(ScriptContext::GetDocumentLoaderURLForFrame(
          render_frame->GetWebFrame())) {}

ScriptsRunInfo::~ScriptsRunInfo() {
}

void ScriptsRunInfo::LogRun(bool send_script_activity) {
  // Notify the browser if any extensions are now executing scripts.
  if (!executing_scripts.empty() && send_script_activity) {
    content::RenderThread::Get()->Send(
        new ExtensionHostMsg_ContentScriptsExecuting(
            routing_id_, executing_scripts, frame_url_));
  }

  base::TimeDelta elapsed = timer.Elapsed();

  switch (run_location_) {
    case UserScript::DOCUMENT_START:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_CssCount", num_css);
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_ScriptCount", num_js);
      if (num_blocking_js) {
        UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_BlockingScriptCount",
                                 num_blocking_js);
      } else if (num_css || num_js) {
        UMA_HISTOGRAM_TIMES("Extensions.InjectStart_Time", elapsed);
      }
      break;
    case UserScript::DOCUMENT_END:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectEnd_ScriptCount", num_js);
      if (num_blocking_js) {
        UMA_HISTOGRAM_COUNTS_100("Extensions.InjectEnd_BlockingScriptCount",
                                 num_blocking_js);
      } else if (num_js) {
        UMA_HISTOGRAM_TIMES("Extensions.InjectEnd_Time", elapsed);
      }
      break;
    case UserScript::DOCUMENT_IDLE:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectIdle_ScriptCount", num_js);
      if (num_blocking_js) {
        UMA_HISTOGRAM_COUNTS_100("Extensions.InjectIdle_BlockingScriptCount",
                                 num_blocking_js);
      } else if (num_js) {
        UMA_HISTOGRAM_TIMES("Extensions.InjectIdle_Time", elapsed);
      }
      break;
    case UserScript::RUN_DEFERRED:
    case UserScript::BROWSER_DRIVEN:
      // TODO(rdevlin.cronin): Add histograms.
      break;
    case UserScript::UNDEFINED:
    case UserScript::RUN_LOCATION_LAST:
      NOTREACHED();
  }
}

}  // namespace extensions
