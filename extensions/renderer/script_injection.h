// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/script_injector.h"

struct HostID;

namespace content {
class RenderFrame;
}

namespace v8 {
class Value;
template <class T> class Local;
}

namespace extensions {
struct ScriptsRunInfo;

// A script wrapper which is aware of whether or not it is allowed to execute,
// and contains the implementation to do so.
class ScriptInjection {
 public:
  enum InjectionResult {
    INJECTION_FINISHED,
    INJECTION_BLOCKED,
    INJECTION_WAITING
  };

  using CompletionCallback = base::Callback<void(ScriptInjection*)>;

  // Return the id of the injection host associated with the given world.
  static std::string GetHostIdForIsolatedWorld(int world_id);

  // Remove the isolated world associated with the given injection host.
  static void RemoveIsolatedWorld(const std::string& host_id);

  ScriptInjection(std::unique_ptr<ScriptInjector> injector,
                  content::RenderFrame* render_frame,
                  std::unique_ptr<const InjectionHost> injection_host,
                  UserScript::RunLocation run_location,
                  bool log_activity);
  ~ScriptInjection();

  // Try to inject the script at the |current_location|. This returns
  // INJECTION_FINISHED if injection has injected or will never inject, returns
  // INJECTION_BLOCKED if injection is running asynchronously and has not
  // finished yet, returns INJECTION_WAITING if injections is delayed (either
  // for permission purposes or because |current_location| is not the designated
  // |run_location_|).
  // If INJECTION_BLOCKED is returned, |async_completion_callback| will be
  // called upon completion.
  InjectionResult TryToInject(
      UserScript::RunLocation current_location,
      ScriptsRunInfo* scripts_run_info,
      const CompletionCallback& async_completion_callback);

  // Called when permission for the given injection has been granted.
  // Returns INJECTION_FINISHED if injection has injected or will never inject,
  // returns INJECTION_BLOCKED if injection is ran asynchronously.
  InjectionResult OnPermissionGranted(ScriptsRunInfo* scripts_run_info);

  // Resets the pointer of the injection host when the host is gone.
  void OnHostRemoved();

  void invalidate_render_frame() { render_frame_ = nullptr; }

  // Accessors.
  content::RenderFrame* render_frame() const { return render_frame_; }
  const HostID& host_id() const { return injection_host_->id(); }
  int64_t request_id() const { return request_id_; }

  // Called when JS injection for the given frame has been completed or
  // cancelled.
  void OnJsInjectionCompleted(const std::vector<v8::Local<v8::Value>>& results,
                              base::Optional<base::TimeDelta> elapsed);

 private:
  class FrameWatcher;

  // Sends a message to the browser to request permission to inject.
  void RequestPermissionFromBrowser();

  // Injects the script. Returns INJECTION_FINISHED if injection has finished,
  // otherwise INJECTION_BLOCKED.
  InjectionResult Inject(ScriptsRunInfo* scripts_run_info);

  // Inject any JS scripts into the frame for the injection.
  void InjectJs(std::set<std::string>* executing_scripts,
                size_t* num_injected_js_scripts);

  // Inject any CSS source into the frame for the injection.
  void InjectCss(std::set<std::string>* injected_stylesheets,
                 size_t* num_injected_stylesheets);

  // Notify that we will not inject, and mark it as acknowledged.
  void NotifyWillNotInject(ScriptInjector::InjectFailureReason reason);

  // The injector for this injection.
  std::unique_ptr<ScriptInjector> injector_;

  // The RenderFrame into which this should inject the script.
  content::RenderFrame* render_frame_;

  // The associated injection host.
  std::unique_ptr<const InjectionHost> injection_host_;

  // The location in the document load at which we inject the script.
  UserScript::RunLocation run_location_;

  // This injection's request id. This will be -1 unless the injection is
  // currently waiting on permission.
  int64_t request_id_;

  // Whether or not the injection is complete, either via injecting the script
  // or because it will never complete.
  bool complete_;

  // Whether or not the injection successfully injected JS.
  bool did_inject_js_;

  // Whether or not we should log dom activity for this injection.
  bool log_activity_;

  // Results storage.
  std::unique_ptr<base::Value> execution_result_;

  // The callback to run upon completing asynchronously.
  CompletionCallback async_completion_callback_;

  // A helper class to hold the render frame and watch for its deletion.
  std::unique_ptr<FrameWatcher> frame_watcher_;

  base::WeakPtrFactory<ScriptInjection> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ScriptInjection);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_
