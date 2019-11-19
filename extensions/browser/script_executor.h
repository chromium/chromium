// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SCRIPT_EXECUTOR_H_
#define EXTENSIONS_BROWSER_SCRIPT_EXECUTOR_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "extensions/common/constants.h"
#include "extensions/common/user_script.h"

class GURL;
struct ExtensionMsg_ExecuteCode_Params;

namespace base {
class ListValue;
}  // namespace base

namespace content {
class WebContents;
}

namespace extensions {

// Contains all extensions that are executing scripts, mapped to the paths for
// those scripts. The paths may be an empty set if the script has no path
// associated with it (e.g. in the case of tabs.executeScript), but there will
// still be an entry for the extension.
using ExecutingScriptsMap = std::map<std::string, std::set<std::string>>;

// Callback that ScriptExecutor uses to notify when content scripts and/or
// tabs.executeScript calls run on a page.
using ScriptsExecutedNotification = base::RepeatingCallback<
    void(content::WebContents*, const ExecutingScriptsMap&, const GURL&)>;

// Interface for executing extension content scripts (e.g. executeScript) as
// described by the ExtensionMsg_ExecuteCode_Params IPC, and notifying the
// caller when responded with ExtensionHostMsg_ExecuteCodeFinished.
class ScriptExecutor {
 public:
  explicit ScriptExecutor(content::WebContents* web_contents);
  ~ScriptExecutor();

  // The type of script being injected.
  enum ScriptType {
    JAVASCRIPT,
    CSS,
  };

  // The scope of the script injection across the frames.
  enum FrameScope {
    SINGLE_FRAME,
    INCLUDE_SUB_FRAMES,
  };

  // Whether to insert the script in about: frames when its origin matches
  // the extension's host permissions.
  enum MatchAboutBlank {
    DONT_MATCH_ABOUT_BLANK,
    MATCH_ABOUT_BLANK,
  };

  // The type of process the target is.
  enum ProcessType {
    DEFAULT_PROCESS,
    WEB_VIEW_PROCESS,
  };

  // The type of result the caller is interested in.
  enum ResultType {
    NO_RESULT,
    JSON_SERIALIZED_RESULT,
  };

  // Callback from ExecuteScript. The arguments are (error, on_url, result).
  // Success is implied by an empty error.
  typedef base::Callback<
      void(const std::string&, const GURL&, const base::ListValue&)>
      ScriptFinishedCallback;

  // Executes a script. The arguments match ExtensionMsg_ExecuteCode_Params in
  // extension_messages.h (request_id is populated automatically).
  //
  // The script will be executed in the frame identified by |frame_id| (which is
  // an extension API frame ID). If |frame_scope| is INCLUDE_SUB_FRAMES, then
  // the script will also be executed in all descendants of the frame.
  //
  // |callback| will always be called even if the IPC'd renderer is destroyed
  // before a response is received (in this case the callback will be with a
  // failure and appropriate error message).
  void ExecuteScript(const HostID& host_id,
                     ScriptType script_type,
                     const std::string& code,
                     FrameScope frame_scope,
                     int frame_id,
                     MatchAboutBlank match_about_blank,
                     UserScript::RunLocation run_at,
                     ProcessType process_type,
                     const GURL& webview_src,
                     const GURL& file_url,
                     bool user_gesture,
                     base::Optional<CSSOrigin> css_origin,
                     ResultType result_type,
                     const ScriptFinishedCallback& callback);

  // Set the observer for ScriptsExecutedNotification callbacks.
  void set_observer(ScriptsExecutedNotification observer) {
    observer_ = std::move(observer);
  }

 private:
  // The next value to use for request_id in ExtensionMsg_ExecuteCode_Params.
  int next_request_id_ = 0;

  content::WebContents* web_contents_;

  ScriptsExecutedNotification observer_;

  DISALLOW_COPY_AND_ASSIGN(ScriptExecutor);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SCRIPT_EXECUTOR_H_
