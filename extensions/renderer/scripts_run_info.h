// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPTS_RUN_INFO_H_
#define EXTENSIONS_RENDERER_SCRIPTS_RUN_INFO_H_

#include <stddef.h>

#include <map>
#include <set>
#include <string>

#include "base/timer/elapsed_timer.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/user_script.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
class RenderFrame;
}

namespace extensions {

// A struct containing information about a script run.
struct ScriptsRunInfo {
  // Map of extensions IDs to the executing script paths.
  typedef std::map<std::string, std::set<std::string> > ExecutingScriptsMap;

  ScriptsRunInfo(content::RenderFrame* render_frame,
                 mojom::RunLocation location);

  ScriptsRunInfo(const ScriptsRunInfo&) = delete;
  ScriptsRunInfo& operator=(const ScriptsRunInfo&) = delete;

  ~ScriptsRunInfo();

  // The number of CSS scripts injected.
  size_t num_css;
  // The number of JS scripts injected.
  size_t num_js;
  // The number of blocked JS scripts injected.
  size_t num_blocking_js;
  // A map of extension ids to executing script paths.
  ExecutingScriptsMap executing_scripts;
  // A map of extension ids to injected stylesheet paths.
  ExecutingScriptsMap injected_stylesheets;
  // The elapsed time since the ScriptsRunInfo was constructed.
  base::ElapsedTimer timer;

  // Log information about a given script run. If |send_script_activity| is
  // true, this also informs the browser of the script run.
  void LogRun(bool send_script_activity);

  static void LogLongInjectionTaskTime(mojom::RunLocation run_location,
                                       const base::TimeDelta& elapsed);

 private:
  // The frame token to use to notify the browser of any injections. Since the
  // frame may be deleted in injection, we don't hold on to a reference to it
  // directly.
  blink::LocalFrameToken frame_token_;

  // The run location at which injection is happening.
  mojom::RunLocation run_location_;

  // The url of the frame, preserved for the same reason as the routing id.
  GURL frame_url_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPTS_RUN_INFO_H_
