// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/scripts_run_info.h"

#include "base/metrics/histogram_macros.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

ScriptsRunInfo::ScriptsRunInfo(content::RenderFrame* render_frame,
                               mojom::RunLocation location)
    : num_css(0u),
      num_js(0u),
      num_blocking_js(0u),
      frame_token_(render_frame->GetWebFrame()->GetLocalFrameToken()),
      run_location_(location),
      frame_url_(ScriptContext::GetDocumentLoaderURLForFrame(
          render_frame->GetWebFrame())) {}

ScriptsRunInfo::~ScriptsRunInfo() {
}

void ScriptsRunInfo::LogRun(bool send_script_activity) {
  // Notify the browser if any extensions are now executing scripts.
  if (!executing_scripts.empty() && send_script_activity) {
    content::RenderFrame* frame = nullptr;
    if (auto* web_frame = blink::WebLocalFrame::FromFrameToken(frame_token_)) {
      frame = content::RenderFrame::FromWebFrame(web_frame);
    }
    if (frame) {
      // We can't convert a map of sets into a flat_map of vectors with mojo
      // bindings so we need to do it manually. The set property is useful for
      // this class so we can't convert the class storage type.

      std::vector<std::pair<std::string, std::vector<std::string>>>
          scripts_vector;
      for (auto& script : executing_scripts) {
        scripts_vector.emplace_back(
            script.first, std::vector<std::string>(script.second.begin(),
                                                   script.second.end()));
      }
      base::flat_map<std::string, std::vector<std::string>> mojo_scripts(
          std::move(scripts_vector));
      ExtensionFrameHelper::Get(frame)
          ->GetLocalFrameHost()
          ->ContentScriptsExecuting(mojo_scripts, frame_url_);
    }
  }

  base::TimeDelta elapsed = timer.Elapsed();

  switch (run_location_) {
    case mojom::RunLocation::kDocumentStart:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_CssCount", num_css);
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_ScriptCount", num_js);
      if (num_blocking_js) {
        UMA_HISTOGRAM_COUNTS_100("Extensions.InjectStart_BlockingScriptCount",
                                 num_blocking_js);
      } else if (num_css || num_js) {
        UMA_HISTOGRAM_TIMES("Extensions.InjectStart_Time", elapsed);
      }
      break;
    case mojom::RunLocation::kDocumentEnd:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectEnd_ScriptCount", num_js);
      if (num_blocking_js) {
        UMA_HISTOGRAM_COUNTS_100("Extensions.InjectEnd_BlockingScriptCount",
                                 num_blocking_js);
      } else if (num_js) {
        UMA_HISTOGRAM_TIMES("Extensions.InjectEnd_Time", elapsed);
      }
      break;
    case mojom::RunLocation::kDocumentIdle:
      UMA_HISTOGRAM_COUNTS_100("Extensions.InjectIdle_ScriptCount", num_js);
      if (num_blocking_js) {
        UMA_HISTOGRAM_COUNTS_100("Extensions.InjectIdle_BlockingScriptCount",
                                 num_blocking_js);
      } else if (num_js) {
        UMA_HISTOGRAM_TIMES("Extensions.InjectIdle_Time", elapsed);
      }
      break;
    case mojom::RunLocation::kRunDeferred:
    case mojom::RunLocation::kBrowserDriven:
      // TODO(rdevlin.cronin): Add histograms.
      break;
    case mojom::RunLocation::kUndefined:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace extensions
