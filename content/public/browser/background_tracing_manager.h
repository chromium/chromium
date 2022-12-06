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

namespace content {
class BackgroundTracingConfig;

// BackgroundTracingManager is used on the browser process to trigger the
// collection of trace data and upload the results. Only the browser UI thread
// is allowed to interact with the BackgroundTracingManager. All callbacks are
// called on the UI thread.
class BackgroundTracingManager {
 public:
  CONTENT_EXPORT static BackgroundTracingManager& GetInstance();

  CONTENT_EXPORT static const char kContentTriggerConfig[];

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
      base::RepeatingCallback<void(std::unique_ptr<std::string>,
                                   FinishedProcessingCallback)>;

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
  // `local_output` should be true if `receive_callback` saves the trace
  // locally (such as for testing), false if `receive_callback` uploads the
  // trace to a server.
  virtual bool SetActiveScenarioWithReceiveCallback(
      std::unique_ptr<BackgroundTracingConfig> config,
      ReceiveCallback receive_callback,
      DataFiltering data_filtering) = 0;

  // Notifies the caller when the manager is idle (not recording or uploading),
  // so that a call to SetActiveScenario() is likely to succeed.
  using IdleCallback = base::RepeatingCallback<void()>;
  virtual void WhenIdle(IdleCallback idle_callback) = 0;

  using StartedFinalizingCallback = base::OnceCallback<void(bool)>;
  using TriggerHandle = int;

  // Notifies that a manual trigger event has occurred, and we may need to
  // either begin recording or finalize the trace, depending on the config.
  // If the trigger specified isn't active in the config, this will do nothing.
  virtual void TriggerNamedEvent(
      TriggerHandle trigger_handle,
      StartedFinalizingCallback started_callback) = 0;

  // Registers a manual trigger handle, and returns a TriggerHandle which can
  // be passed to DidTriggerHappen().
  virtual TriggerHandle RegisterTriggerType(base::StringPiece trigger_name) = 0;

  // Returns the name associated with the given trigger handle.
  virtual const std::string& GetTriggerNameFromHandle(
      TriggerHandle trigger_handle) = 0;

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

  // Sets a callback to override the background tracing config for testing.
  virtual void SetConfigTextFilterForTesting(
      ConfigTextFilterForTesting predicate) = 0;

 protected:
  virtual ~BackgroundTracingManager() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACKGROUND_TRACING_MANAGER_H_
