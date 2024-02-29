// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/mojom/host_id.mojom-forward.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/injection_host.h"
#include "extensions/renderer/script_injector.h"
#include "v8/include/v8-forward.h"

namespace content {
class RenderFrame;
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

  // Represents the purpose of calling StatusUpdatedCallback.
  enum class InjectionStatus {
    kPermitted,
    kFinished,
  };

  using StatusUpdatedCallback =
      base::OnceCallback<void(InjectionStatus, ScriptInjection*)>;

  // Return the id of the injection host associated with the given world.
  static std::string GetHostIdForIsolatedWorld(int world_id);

  // Remove the isolated world associated with the given injection host.
  static void RemoveIsolatedWorld(const std::string& host_id);

  ScriptInjection(std::unique_ptr<ScriptInjector> injector,
                  content::RenderFrame* render_frame,
                  std::unique_ptr<const InjectionHost> injection_host,
                  mojom::RunLocation run_location,
                  bool log_activity);

  ScriptInjection(const ScriptInjection&) = delete;
  ScriptInjection& operator=(const ScriptInjection&) = delete;

  ~ScriptInjection();

  // Try to inject the script at the |current_location|. This returns
  // INJECTION_FINISHED if injection has injected or will never inject, returns
  // INJECTION_BLOCKED if injection is running asynchronously and has not
  // finished yet, returns INJECTION_WAITING if injections is delayed (either
  // for permission purposes or because |current_location| is not the designated
  // |run_location_|).
  // If INJECTION_BLOCKED or INJECTION_WAITING is returned,
  // |async_updated_callback| will be called upon the status updated.
  InjectionResult TryToInject(mojom::RunLocation current_location,
                              ScriptsRunInfo* scripts_run_info,
                              StatusUpdatedCallback async_updated_callback);

  // Called when permission for the given injection has been granted.
  // Returns INJECTION_FINISHED if injection has injected or will never inject,
  // returns INJECTION_BLOCKED if injection is ran asynchronously.
  InjectionResult OnPermissionGranted(ScriptsRunInfo* scripts_run_info);

  // Resets the pointer of the injection host when the host is gone.
  void OnHostRemoved();

  void invalidate_render_frame() { render_frame_ = nullptr; }

  // Accessors.
  content::RenderFrame* render_frame() const { return render_frame_; }
  const mojom::HostID& host_id() const { return injection_host_->id(); }
  int64_t request_id() const { return request_id_; }

  // Called when JS injection for the given frame has been completed or
  // cancelled.
  void OnJsInjectionCompleted(std::optional<base::Value> value,
                              base::TimeTicks start_time);

 private:
  class FrameWatcher;

  // Sends a message to the browser to request permission to inject.
  // |async_updated_callback| should be called if the permission is handled.
  void RequestPermissionFromBrowser(
      StatusUpdatedCallback async_updated_callback);

  // Handles the injection permission calling |async_updated_callback| if
  // |granted| is true.
  void HandlePermission(StatusUpdatedCallback async_updated_callback,
                        bool granted);

  // Injects the script. Returns INJECTION_FINISHED if injection has finished,
  // otherwise INJECTION_BLOCKED.
  InjectionResult Inject(ScriptsRunInfo* scripts_run_info);

  // Inject any JS scripts into the frame for the injection.
  void InjectJs(std::set<std::string>* executing_scripts,
                size_t* num_injected_js_scripts);

  // Inject or remove any CSS source into the frame for the injection.
  void InjectOrRemoveCss(std::set<std::string>* injected_stylesheets,
                         size_t* num_injected_stylesheets);

  // Notify that we will not inject, and mark it as acknowledged.
  void NotifyWillNotInject(ScriptInjector::InjectFailureReason reason);

  // The injector for this injection.
  std::unique_ptr<ScriptInjector> injector_;

  // The RenderFrame into which this should inject the script.
  raw_ptr<content::RenderFrame, DanglingUntriaged> render_frame_;

  // The associated injection host.
  std::unique_ptr<const InjectionHost> injection_host_;

  // The location in the document load at which we inject the script.
  mojom::RunLocation run_location_;

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
  std::optional<base::Value> execution_result_;

  // The callback to run upon the status updated asynchronously. It's used for
  // the reply of the permission handling or script injection completion.
  StatusUpdatedCallback async_completion_callback_;

  // A helper class to hold the render frame and watch for its deletion.
  std::unique_ptr<FrameWatcher> frame_watcher_;

  base::WeakPtrFactory<ScriptInjection> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_INJECTION_H_
