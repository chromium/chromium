// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_HANDLER_H_
#define CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "base/task/task_runner.h"
#include "base/token.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/trace_report/trace_report.mojom.h"
#include "content/browser/tracing/trace_report/trace_upload_list.h"
#include "content/common/content_export.h"
#include "content/public/browser/background_tracing_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
// Handles communication between the browser and chrome://traces-internals.
class CONTENT_EXPORT TraceReportHandler
    : public trace_report::mojom::PageHandler {
 public:
  TraceReportHandler(
      mojo::PendingReceiver<trace_report::mojom::PageHandler> receiver,
      mojo::PendingRemote<trace_report::mojom::Page> page);

  TraceReportHandler(
      mojo::PendingReceiver<trace_report::mojom::PageHandler> receiver,
      mojo::PendingRemote<trace_report::mojom::Page> page,
      TraceUploadList& trace_upload_list,
      BackgroundTracingManagerImpl& background_tracing_manager);

  TraceReportHandler(const TraceReportHandler&) = delete;
  TraceReportHandler& operator=(const TraceReportHandler&) = delete;
  ~TraceReportHandler() override;

  // trace_report::mojom::TraceReportHandler:
  // Get all the trace report currently stored locally
  void GetAllTraceReports(GetAllTraceReportsCallback callback) override;
  void DeleteSingleTrace(const base::Token& uuid,
                         DeleteSingleTraceCallback callback) override;
  void DeleteAllTraces(DeleteAllTracesCallback callback) override;
  void UserUploadSingleTrace(const base::Token& uuid,
                             UserUploadSingleTraceCallback callback) override;
  void DownloadTrace(const base::Token& uuid,
                     DownloadTraceCallback callback) override;
  void GetAllPresetScenarios(GetAllPresetScenariosCallback callback) override;
  void GetAllFieldScenarios(GetAllFieldScenariosCallback callback) override;
  void GetEnabledScenarios(GetEnabledScenariosCallback callback) override;
  void SetEnabledScenarios(const std::vector<std::string>& new_config,
                           SetEnabledScenariosCallback callback) override;

 private:
  void OnGetAllReportsTaskComplete(GetAllTraceReportsCallback callback,
                                   std::vector<ClientTraceReport> results);

  mojo::Receiver<trace_report::mojom::PageHandler> receiver_;
  mojo::PendingRemote<trace_report::mojom::Page> page_;

  // Used to perform actions with on a single trace_report_database instance.
  raw_ref<TraceUploadList> trace_upload_list_;
  raw_ref<BackgroundTracingManagerImpl> background_tracing_manager_;

  base::WeakPtrFactory<TraceReportHandler> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACE_REPORT_TRACE_REPORT_HANDLER_H_
