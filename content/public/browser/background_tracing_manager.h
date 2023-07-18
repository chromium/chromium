// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_MANAGER_H_

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "base/trace_event/trace_event_impl.h"
#include "content/common/content_export.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/scenario_config.gen.h"

namespace content {
class BackgroundTracingConfig;

// BackgroundTracingManager is used on the browser process to trigger the
// collection of trace data and upload the results. Only the browser UI thread
// is allowed to interact with the BackgroundTracingManager. All callbacks are
// called on the UI thread.
class BackgroundTracingManager {
 public:
  // Creates and return a global BackgroundTracingManager instance.
  CONTENT_EXPORT static std::unique_ptr<BackgroundTracingManager>
  CreateInstance();

  // Returns the global instance created with CreateInstance().
  CONTENT_EXPORT static BackgroundTracingManager& GetInstance();

  // Notifies that a manual trigger event has occurred. Returns true if the
  // trigger caused a scenario to either begin recording or finalize the trace
  // depending on the config, or false if the trigger had no effect. If the
  // trigger specified isn't active in the config, this will do nothing.
  CONTENT_EXPORT static bool EmitNamedTrigger(const std::string& trigger_name);

  CONTENT_EXPORT static const char kContentTriggerConfig[];

  // Enabled state observers get a callback when the state of background tracing
  // changes.
  class CONTENT_EXPORT EnabledStateTestObserver {
   public:
    // Called when |scenario_name| becomes active.
    virtual void OnScenarioActive(const std::string& scenario_name) {}
    // Called when |scenario_name| becomes idle again.
    virtual void OnScenarioIdle(const std::string& scenario_name) {}
    // Called when tracing is enabled on all processes because of an active
    // scenario.
    virtual void OnTraceStarted() {}
    // Called when tracing stopped and |proto_content| was received.
    virtual void OnTraceReceived(const std::string& proto_content) {}

   protected:
    ~EnabledStateTestObserver() = default;
  };

  virtual ~BackgroundTracingManager() = default;

  // If a ReceiveCallback is set it will be called on the UI thread every time
  // the BackgroundTracingManager finalizes a trace. The first parameter of
  // this callback is the trace data. The second is metadata that was generated
  // and embedded into the trace. The third is a callback to notify the
  // BackgroundTracingManager that you've finished processing the trace data
  // and whether we were successful or not.
  //
  // Example:
  //
  // void Upload(const scoped_refptr<base::RefCountedString>& data,
  //             FinishedProcessingCallback done_callback) {
  //   base::PostTaskAndReply(
  //       FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
  //       base::BindOnce(&DoUploadInBackground, data),
  //       std::move(done_callback));
  // }
  //
  using FinishedProcessingCallback = base::OnceCallback<void(bool success)>;
  using ReceiveCallback =
      base::RepeatingCallback<void(std::string, FinishedProcessingCallback)>;

  // Set the triggering rules for when to start recording.
  //
  // In preemptive mode, recording begins immediately and any calls to
  // TriggerNamedEvent() will potentially trigger the trace to finalize and get
  // uploaded. Once the trace has been uploaded, tracing will be enabled again.
  //
  // In reactive mode, recording begins when TriggerNamedEvent() is called, and
  // continues until either the next call to TriggerNamedEvent, or a timeout
  // occurs. Tracing will not be re-enabled after the trace is finalized and
  // uploaded.
  //
  // This function uploads traces through UMA using SetTraceToUpload /
  // GetLatestTraceToUpload. To specify a destination to upload to, use
  // SetActiveScenarioWithReceiveCallback.
  //
  // Calls to SetActiveScenario() with a config will fail if tracing is
  // currently on. Use WhenIdle to register a callback to get notified when
  // the manager is idle and a config can be set again.
  enum DataFiltering {
    NO_DATA_FILTERING,
    ANONYMIZE_DATA,
  };

  virtual bool SetActiveScenario(
      std::unique_ptr<BackgroundTracingConfig> config,
      DataFiltering data_filtering) = 0;

  // Identical to SetActiveScenario except that whenever a trace is finalized,
  // BackgroundTracingManager calls `receive_callback` to upload the trace.
  virtual bool SetActiveScenarioWithReceiveCallback(
      std::unique_ptr<BackgroundTracingConfig> config,
      ReceiveCallback receive_callback,
      DataFiltering data_filtering) = 0;

  // Initializes background tracing with a set of scenarios, each
  // associated with specific tracing configs. Scenarios are enrolled by
  // clients based on a set of start and stop rules that delimitate a
  // meaningful tracing interval, usually covering a user journey or a
  // guardian metric (e.g. FirstContentfulPaint). This can only be
  // called once.
  //
  // `receive_callback` is called whenever a trace is finalized.
  virtual bool InitializeScenarios(
      const perfetto::protos::gen::ChromeFieldTracingConfig& config,
      ReceiveCallback receive_callback,
      DataFiltering data_filtering) = 0;

  virtual bool HasActiveScenario() = 0;

  // Returns true whether a trace is ready to be uploaded.
  virtual bool HasTraceToUpload() = 0;

  // Returns the latest trace created for uploading in a serialized proto of
  // message type perfetto::Trace.
  // TODO(ssid): This should also return the trigger for the trace along with
  // the serialized trace proto.
  virtual std::string GetLatestTraceToUpload() = 0;

  // Returns background tracing configuration for the experiment |trial_name|.
  virtual std::unique_ptr<BackgroundTracingConfig> GetBackgroundTracingConfig(
      const std::string& trial_name) = 0;

  // For tests
  virtual void AbortScenarioForTesting() = 0;
  virtual void SetTraceToUploadForTesting(
      std::unique_ptr<std::string> trace_data) = 0;

  using ConfigTextFilterForTesting =
      base::RepeatingCallback<std::string(const std::string&)>;

 protected:
  // Sets the instance returns by GetInstance() globally to |tracing_manager|.
  CONTENT_EXPORT static void SetInstance(
      BackgroundTracingManager* tracing_manager);

  virtual bool DoEmitNamedTrigger(const std::string& trigger_name) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_MANAGER_H_
