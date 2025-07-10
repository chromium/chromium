// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_HANDLER_H_
#define CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "base/task/task_runner.h"
#include "base/token.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/trace_upload_list.h"
#include "content/browser/tracing/traces_internals/traces_internals.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_tracing_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
// Handles communication between the browser and chrome://traces.
class CONTENT_EXPORT TracesInternalsHandler
    : public traces_internals::mojom::PageHandler {
 public:
  TracesInternalsHandler(
      mojo::PendingReceiver<traces_internals::mojom::PageHandler> receiver,
      mojo::PendingRemote<traces_internals::mojom::Page> page);

  TracesInternalsHandler(const TracesInternalsHandler&) = delete;
  TracesInternalsHandler& operator=(const TracesInternalsHandler&) = delete;
  ~TracesInternalsHandler() override;

  // trace_report::mojom::TracesInternalsHandler:
  // Get all the trace report currently stored locally
  void StartTraceSession(mojo_base::BigBuffer config_pb,
                         bool enable_privacy_filters,
                         StartTraceSessionCallback callback) override;
  void CloneTraceSession(CloneTraceSessionCallback callback) override;
  void StopTraceSession(StopTraceSessionCallback callback) override;
  void GetTrackEventCategories(
      GetTrackEventCategoriesCallback callback) override;
  void GetBufferUsage(GetBufferUsageCallback callback) override;
  void GetAllTraceReports(GetAllTraceReportsCallback callback) override;
  void DeleteSingleTrace(const base::Token& uuid,
                         DeleteSingleTraceCallback callback) override;
  void DeleteAllTraces(DeleteAllTracesCallback callback) override;
  void UserUploadSingleTrace(const base::Token& uuid,
                             UserUploadSingleTraceCallback callback) override;
  void DownloadTrace(const base::Token& uuid,
                     DownloadTraceCallback callback) override;
  void GetAllScenarios(GetAllScenariosCallback callback) override;
  void SetEnabledScenarios(const std::vector<std::string>& new_config,
                           SetEnabledScenariosCallback callback) override;
  void GetPrivacyFilterEnabled(
      GetPrivacyFilterEnabledCallback callback) override;
  void SetPrivacyFilterEnabled(bool enable) override;

  void SetScenariosConfigFromString(
      const std::string& config_string,
      SetScenariosConfigFromStringCallback callback) override;
  void SetScenariosConfigFromBuffer(
      mojo_base::BigBuffer config_pb,
      SetScenariosConfigFromBufferCallback callback) override;

#if BUILDFLAG(IS_WIN)
  void GetSystemTracingState(GetSystemTracingStateCallback callback) override;
  void GetSecurityShieldIconUrl(
      GetSecurityShieldIconUrlCallback callback) override;
  void EnableSystemTracing(EnableSystemTracingCallback callback) override;
  void DisableSystemTracing(DisableSystemTracingCallback callback) override;
#endif  // BUILDFLAG(IS_WIN)

 protected:
  TracesInternalsHandler(
      mojo::PendingReceiver<traces_internals::mojom::PageHandler> receiver,
      mojo::PendingRemote<traces_internals::mojom::Page> page,
      TraceUploadList& trace_upload_list,
      BackgroundTracingManagerImpl& background_tracing_manager,
      TracingDelegate* tracing_delegate);

  virtual std::unique_ptr<perfetto::TracingSession> CreateTracingSession();

 private:
  void OnGetAllReportsTaskComplete(GetAllTraceReportsCallback callback,
                                   std::vector<ClientTraceReport> results);
  bool SetScenariosConfig(
      const perfetto::protos::gen::ChromeFieldTracingConfig& config);
  void MaybeSetupPresetTracingFromFieldTrial();

  void OnTracingError(perfetto::TracingError error);
  void OnTracingStop();
  void OnTracingStart();
  void OnTraceComplete(std::optional<mojo_base::BigBuffer>,
                       const std::optional<base::Token>&);
  void OnBufferUsage(bool success, float percent_full, bool data_loss);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::Receiver<traces_internals::mojom::PageHandler> receiver_;
  mojo::Remote<traces_internals::mojom::Page> page_;

  // Used to perform actions with on a single trace_report_database instance.
  const raw_ref<TraceUploadList> trace_upload_list_;
  const raw_ref<BackgroundTracingManagerImpl> background_tracing_manager_;
  const raw_ptr<TracingDelegate> tracing_delegate_;

  base::UnguessableToken session_unguessable_name_;
  base::Token session_id_;
  std::unique_ptr<perfetto::TracingSession> tracing_session_;
  StartTraceSessionCallback start_callback_;
  StopTraceSessionCallback stop_callback_;
  GetTrackEventCategoriesCallback get_track_event_categories_callback_;
  GetBufferUsageCallback on_buffer_usage_callback_;

  base::WeakPtrFactory<TracesInternalsHandler> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACES_INTERNALS_TRACES_INTERNALS_HANDLER_H_
