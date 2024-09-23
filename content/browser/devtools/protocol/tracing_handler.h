// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/tracing.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/tracing_controller.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace base {

namespace trace_event {
class TraceConfig;
}
class RepeatingTimer;
}  // namespace base

namespace media {
class VideoFrame;
}

namespace content {

class FrameTreeNode;
class DevToolsAgentHostImpl;
class DevToolsVideoConsumer;
class DevToolsIOContext;
class DevToolsSession;
class NavigationRequest;
class TracingProcessSetMonitor;

namespace protocol {

class TracingHandler : public DevToolsDomainHandler, public Tracing::Backend {
 public:
  CONTENT_EXPORT TracingHandler(DevToolsAgentHostImpl* host,
                                DevToolsIOContext* io_context,
                                DevToolsSession* root_session);

  TracingHandler(const TracingHandler&) = delete;
  TracingHandler& operator=(const TracingHandler&) = delete;

  CONTENT_EXPORT ~TracingHandler() override;

  static std::vector<TracingHandler*> ForAgentHost(DevToolsAgentHostImpl* host);

  // DevToolsDomainHandler implementation.
  void WillInitiatePrerender(FrameTreeNode* ftn);

  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;

  void OnTraceDataCollected(std::unique_ptr<std::string> trace_fragment);
  void OnTraceComplete();
  void OnTraceToStreamComplete(const std::string& stream_handle);

  // Protocol methods.
  void Start(Maybe<std::string> categories,
             Maybe<std::string> options,
             Maybe<double> buffer_usage_reporting_interval,
             Maybe<std::string> transfer_mode,
             Maybe<std::string> transfer_format,
             Maybe<std::string> transfer_compression,
             Maybe<Tracing::TraceConfig> config,
             Maybe<Binary> perfetto_config,
             Maybe<std::string> tracing_backend,
             std::unique_ptr<StartCallback> callback) override;
  Response End() override;
  void GetCategories(std::unique_ptr<GetCategoriesCallback> callback) override;
  void RequestMemoryDump(
      Maybe<bool> deterministic,
      Maybe<std::string> level_of_detail,
      std::unique_ptr<RequestMemoryDumpCallback> callback) override;
  Response RecordClockSyncMarker(const std::string& sync_id) override;

  bool did_initiate_recording() { return did_initiate_recording_; }
  void ReadyToCommitNavigation(NavigationRequest* navigation_request);
  void FrameDeleted(FrameTreeNodeId frame_tree_node_id);

 private:
  friend class TracingHandlerTest;

  class PerfettoTracingSession;

  struct TraceDataBufferState {
   public:
    std::string data;
    size_t pos = 0;
    int open_braces = 0;
    bool in_string = false;
    bool slashed = false;
    size_t offset = 0;
  };

  void OnRecordingEnabled(std::unique_ptr<StartCallback> callback,
                          const std::string& error_msg);
  void OnBufferUsage(bool success,
                     float percent_full,
                     size_t approximate_event_count);
  void OnCategoriesReceived(std::unique_ptr<GetCategoriesCallback> callback,
                            const std::set<std::string>& category_set);
  void OnMemoryDumpFinished(std::unique_ptr<RequestMemoryDumpCallback> callback,
                            bool success,
                            uint64_t dump_id);
  void OnFrameFromVideoConsumer(scoped_refptr<media::VideoFrame> frame);
  // Assuming that the input is a potentially incomplete string representation
  // of a comma separated list of JSON objects, return the longest prefix that
  // is a valid list and store the rest to be used in subsequent calls.
  CONTENT_EXPORT std::string UpdateTraceDataBuffer(
      const std::string& trace_fragment);

  void SetupTimer(double usage_reporting_interval);
  void UpdateBufferUsage();
  void StopTracing(
      const scoped_refptr<TracingController::TraceDataEndpoint>& endpoint);
  bool IsTracing() const;
  void EmitFrameTree();
  static bool IsStartupTracingActive();
  CONTENT_EXPORT static base::trace_event::TraceConfig
  GetTraceConfigFromDevToolsConfig(const base::Value& devtools_config);
  perfetto::TraceConfig CreatePerfettoConfiguration(
      const base::trace_event::TraceConfig& browser_config,
      bool return_as_stream,
      bool proto_format);
  void AttemptAdoptStartupSession(bool return_as_stream,
                                  bool gzip_compression,
                                  bool proto_format,
                                  perfetto::BackendType tracing_backend);

  // Adds an additional process to tracing configuration, if tracing is active.
  void AddProcessToFilter(base::ProcessId pid);

  std::unique_ptr<base::RepeatingTimer> buffer_usage_poll_timer_;

  std::unique_ptr<Tracing::Frontend> frontend_;
  const raw_ptr<DevToolsIOContext> io_context_;
  const raw_ptr<DevToolsAgentHostImpl> host_;  // Only null in unit tests.

  // Session is for use in process filter and is null in browser.
  const raw_ptr<DevToolsSession> session_for_process_filter_;
  bool did_initiate_recording_;
  bool return_as_stream_;
  bool gzip_compression_;
  bool proto_format_;
  double buffer_usage_reporting_interval_;
  TraceDataBufferState trace_data_buffer_state_;
  std::unique_ptr<DevToolsVideoConsumer> video_consumer_;
  int number_of_screenshots_from_video_consumer_ = 0;
  perfetto::TraceConfig trace_config_;
  std::unique_ptr<PerfettoTracingSession> session_;
  std::unique_ptr<TracingProcessSetMonitor> process_set_monitor_;
  base::WeakPtrFactory<TracingHandler> weak_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(TracingHandlerTest,
                           GetTraceConfigFromDevToolsConfig);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
