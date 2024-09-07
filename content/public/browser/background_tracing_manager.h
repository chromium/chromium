// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_MANAGER_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/token.h"
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
    // Called when the trace content is saved.
    virtual void OnTraceSaved() {}

   protected:
    ~EnabledStateTestObserver() = default;
  };

  virtual ~BackgroundTracingManager() = default;

  // If a ReceiveCallback is set it will be called on the UI thread every time
  // the BackgroundTracingManager finalizes a trace. The first parameter of
  // this callback is the trace data. The second is a callback to notify the
  // BackgroundTracingManager that you've finished processing the trace data
  // and whether we were successful or not.
  //
  // Example:
  //
  // void Upload(std::string data,
  //             FinishedProcessingCallback done_callback) {
  //   base::PostTaskAndReply(
  //       FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
  //       base::BindOnce(&DoUploadInBackground, std::move(data)),
  //       std::move(done_callback));
  // }
  //
  using FinishedProcessingCallback = base::OnceCallback<void(bool success)>;
  using ReceiveCallback = base::RepeatingCallback<
      void(const std::string&, std::string, FinishedProcessingCallback)>;

  virtual void SetReceiveCallback(ReceiveCallback receive_callback) = 0;

  enum DataFiltering {
    NO_DATA_FILTERING,
    ANONYMIZE_DATA,
    ANONYMIZE_DATA_AND_FILTER_PACKAGE_NAME,
  };

  // Set the triggering rules for when to start recording.
  //
  // In preemptive mode, recording begins immediately and any calls to
  // TriggerNamedEvent() will potentially trigger the trace to finalize
  // and get uploaded. Once the trace has been uploaded, tracing will be
  // enabled again.
  //
  // In reactive mode, recording begins when TriggerNamedEvent() is
  // called, and continues until either the next call to
  // TriggerNamedEvent, or a timeout occurs. Tracing will not be
  // re-enabled after the trace is finalized and uploaded.
  //
  // This function uploads traces through UMA using GetTraceToUpload.
  //
  // Calls to SetActiveScenario() with a config will fail if tracing is
  // currently on.
  virtual bool SetActiveScenario(
      std::unique_ptr<BackgroundTracingConfig> config,
      DataFiltering data_filtering) = 0;

  // Initializes a list of triggers from `config` to be forwarded to
  // perfetto. This is useful when system tracing is running. This will
  // fail and return false if any scenario was previously enabled,
  // either with InitializeFieldScenarios() or SetEnabledScenarios().
  // This shouldn't be called if SetActiveScenario() was previously
  // called.
  virtual bool InitializePerfettoTriggerRules(
      const perfetto::protos::gen::TracingTriggerRulesConfig& config) = 0;

  // Tracing Scenarios are enrolled by clients based on a set of start and
  // stop rules that delimitate a meaningful tracing interval, usually covering
  // a user journey or a guardian metric (e.g. FirstContentfulPaint). This can
  // only be called once.
  // Field scenarios are enabled automatically for a subset of the population.
  // Preset scenarios are predefined and enabled locally by manual action from
  // a user. When enabled, they take precedence over field scenarios if any.

  // Saves and enables a set of field scenarios, each associated with specific
  // tracing configs. Returns true if all scenarios were successfully
  // initialized. This will fail and return false if any scenario was previously
  // enabled, either with InitializeFieldScenarios() or SetEnabledScenarios().
  // This shouldn't be called if SetActiveScenario() was previously called.
  virtual bool InitializeFieldScenarios(
      const perfetto::protos::gen::ChromeFieldTracingConfig& config,
      DataFiltering data_filtering) = 0;

  // Saves a set of preset scenarios, each associated with specific tracing
  // configs, without enabling them. These scenarios can be enabled with
  // SetEnabledScenarios(). Returns the list of scenario hashes that were
  // successfully added. This can be called multiple times.
  virtual std::vector<std::string> AddPresetScenarios(
      const perfetto::protos::gen::ChromeFieldTracingConfig& config,
      DataFiltering data_filtering) = 0;

  // Enables a list of preset scenarios identified by their hashes. This
  // disables all previously enabled scenarios and aborts the current background
  // tracing session if any. Since InitializeFieldScenarios() above fails if
  // scenarios are enabled, field scenarios can't be re-enabled after calling
  // this.
  virtual bool SetEnabledScenarios(
      std::vector<std::string> enabled_preset_scenario_hashes) = 0;

  virtual bool HasActiveScenario() = 0;

  // Returns true whether a trace is ready to be uploaded.
  virtual bool HasTraceToUpload() = 0;

  // TODO(b/295312118): Move this method to trace_report_list.h when it has
  // landed.
  virtual void DeleteTracesInDateRange(base::Time start, base::Time end) = 0;

  // Loads the content of the next trace saved for uploading and returns
  // it through |callback| in a gzip of a serialized proto of message
  // type perfetto::Trace. |callback| may be invoked either synchronously or
  // on a thread pool task runner.
  virtual void GetTraceToUpload(
      base::OnceCallback<void(std::optional<std::string> /*trace_content*/,
                              std::optional<std::string> /*system_profile*/)>
          callback) = 0;

  // Returns background tracing configuration for the experiment |trial_name|.
  virtual std::unique_ptr<BackgroundTracingConfig> GetBackgroundTracingConfig(
      const std::string& trial_name) = 0;

  // Sets a callback that records `SystemProfileProto` when a trace is
  // collected.
  virtual void SetSystemProfileRecorder(
      base::RepeatingCallback<std::string()> recorder) = 0;

  // For tests
  virtual void AbortScenarioForTesting() = 0;
  virtual void SaveTraceForTesting(std::string&& trace_data,
                                   const std::string& scenario_name,
                                   const std::string& rule_name,
                                   const base::Token& uuid) = 0;

  using ConfigTextFilterForTesting =
      base::RepeatingCallback<std::string(const std::string&)>;

 protected:
  // Sets the instance returns by GetInstance() globally to |tracing_manager|.
  CONTENT_EXPORT static void SetInstance(
      BackgroundTracingManager* tracing_manager);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_MANAGER_H_
