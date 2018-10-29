// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/coordinator.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/service_context_ref.h"
#include "services/tracing/agent_registry.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/public/mojom/tracing.mojom.h"
#include "services/tracing/recorder.h"

namespace {

const char kMetadataTraceLabel[] = "metadata";

const char kGetCategoriesClosureName[] = "GetCategoriesClosure";
const char kRequestBufferUsageClosureName[] = "RequestBufferUsageClosure";
const char kRequestClockSyncMarkerClosureName[] =
    "RequestClockSyncMarkerClosure";
const char kStartTracingClosureName[] = "StartTracingClosure";

}  // namespace

namespace tracing {

class Coordinator::TraceStreamer : public base::SupportsWeakPtr<TraceStreamer> {
 public:
  // Constructed on |main_task_runner_|.
  TraceStreamer(
      mojo::ScopedDataPipeProducerHandle stream,
      const std::string& agent_label,
      const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
      base::WeakPtr<Coordinator> coordinator)
      : stream_(std::move(stream)),
        agent_label_(agent_label),
        main_task_runner_(main_task_runner),
        coordinator_(coordinator),
        metadata_(new base::DictionaryValue()),
        stream_is_empty_(true),
        json_field_name_written_(false) {}

  // Destroyed on |backend_task_runner_|.
  ~TraceStreamer() = default;

  // Called from |backend_task_runner_|.
  void CreateAndSendRecorder(
      const std::string& label,
      mojom::TraceDataType type,
      base::WeakPtr<AgentRegistry::AgentEntry> agent_entry) {
    mojom::RecorderPtr ptr;
    auto recorder = std::make_unique<Recorder>(
        MakeRequest(&ptr), type,
        base::BindRepeating(&Coordinator::TraceStreamer::OnRecorderDataChange,
                            AsWeakPtr(), label));
    recorders_[label].insert(std::move(recorder));
    DCHECK(type != mojom::TraceDataType::STRING ||
           recorders_[label].size() == 1);

    // Tracing agent proxies are bound on the main thread and should be called
    // from the main thread.
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Coordinator::SendRecorder, coordinator_,
                                  agent_entry, std::move(ptr)));
  }

  // Called from |backend_task_runner_| to close the recorder proxy on the
  // correct task runner.
  void CloseRecorder(mojom::RecorderPtr recorder) {}

  // Called from |main_task_runner_| either after flushing is complete or at
  // shutdown. We either will not write to the stream afterwards or do not care
  // what happens to what we try to write.
  void CloseStream() {
    DCHECK(stream_.is_valid());
    stream_.reset();
  }

  // Called from |main_task_runner_| after flushing is completed. So we are sure
  // there is no race in accessing metadata_.
  std::unique_ptr<base::DictionaryValue> GetMetadata() {
    return std::move(metadata_);
  }

 private:
  // Handles synchronize writes to |stream_|, if the stream is not already
  // closed.
  void WriteToStream(const std::string& data) {
    if (stream_.is_valid())
      mojo::BlockingCopyFromString(data, stream_);
  }

  // Called from |backend_task_runner_|.
  void OnRecorderDataChange(const std::string& label) {
    // Bail out if we are in the middle of writing events for another label to
    // the stream, since we do not want to interleave chunks for different
    // fields. For example, we do not want to mix |traceEvent| chunks with
    // |systrace| chunks.
    //
    // If we receive a |systemTraceEvents| chunk from an agent while writing
    // |traceEvent| chunks to the stream, we wait until all agents that send
    // |traceEvent| chunks are done, and then, we start writing
    // |systemTraceEvents| chunks.
    if (!streaming_label_.empty() && streaming_label_ != label)
      return;

    while (streaming_label_.empty() || !StreamEventsForCurrentLabel()) {
      // We are not waiting for data from any particular label now. So, we look
      // at the recorders that have some data available and select the next
      // label to stream.
      streaming_label_.clear();
      bool all_finished = true;
      for (const auto& key_value : recorders_) {
        for (const auto& recorder : key_value.second) {
          all_finished &= !recorder->is_recording();
          if (!recorder->data().empty()) {
            streaming_label_ = key_value.first;
            json_field_name_written_ = false;
            break;
          }
        }
        if (!streaming_label_.empty())
          break;
      }

      if (streaming_label_.empty()) {
        // No recorder has any data for us, right now.
        if (all_finished) {
          StreamMetadata();
          if (!stream_is_empty_ && agent_label_.empty()) {
            WriteToStream("}");
            stream_is_empty_ = false;
          }
          // Recorder connections should be closed on their binding thread.
          main_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&Coordinator::OnFlushDone, coordinator_));
        }
        return;
      }
    }
  }

  // Called from |backend_task_runner_|.
  bool StreamEventsForCurrentLabel() {
    bool waiting_for_agents = false;
    mojom::TraceDataType data_type =
        (*recorders_[streaming_label_].begin())->data_type();
    for (const auto& recorder : recorders_[streaming_label_]) {
      waiting_for_agents |= recorder->is_recording();
      if (!agent_label_.empty() && streaming_label_ != agent_label_)
        recorder->clear_data();
      if (recorder->data().empty())
        continue;

      std::string prefix;
      if (!json_field_name_written_ && agent_label_.empty()) {
        prefix = (stream_is_empty_ ? "{\"" : ",\"") + streaming_label_ + "\":";
        switch (data_type) {
          case mojom::TraceDataType::ARRAY:
            prefix += "[";
            break;
          case mojom::TraceDataType::OBJECT:
            prefix += "{";
            break;
          case mojom::TraceDataType::STRING:
            prefix += "\"";
            break;
          default:
            NOTREACHED();
        }
        json_field_name_written_ = true;
      }
      if (data_type == mojom::TraceDataType::STRING) {
        // Escape characters if needed for string data.
        std::string escaped;
        base::EscapeJSONString(recorder->data(), false /* put_in_quotes */,
                               &escaped);
        WriteToStream(prefix + escaped);
      } else {
        if (prefix.empty() && !stream_is_empty_)
          prefix = ",";
        WriteToStream(prefix + recorder->data());
      }
      stream_is_empty_ = false;
      recorder->clear_data();
    }
    if (!waiting_for_agents) {
      if (json_field_name_written_) {
        switch (data_type) {
          case mojom::TraceDataType::ARRAY:
            WriteToStream("]");
            break;
          case mojom::TraceDataType::OBJECT:
            WriteToStream("}");
            break;
          case mojom::TraceDataType::STRING:
            WriteToStream("\"");
            break;
          default:
            NOTREACHED();
        }
        stream_is_empty_ = false;
      }
    }
    return waiting_for_agents;
  }

  // Called from |backend_task_runner_|.
  void StreamMetadata() {
    if (!agent_label_.empty())
      return;

    for (const auto& key_value : recorders_) {
      for (const auto& recorder : key_value.second) {
        metadata_->MergeDictionary(&(recorder->metadata()));
      }
    }

    std::string metadataJSON;
    if (!metadata_->empty() &&
        base::JSONWriter::Write(*metadata_, &metadataJSON)) {
      std::string prefix = stream_is_empty_ ? "{\"" : ",\"";
      WriteToStream(prefix + std::string(kMetadataTraceLabel) +
                    "\":" + metadataJSON);
      stream_is_empty_ = false;
    }
  }

  // The stream to which trace events from different agents should be
  // serialized, eventually. This is set when tracing is stopped.
  mojo::ScopedDataPipeProducerHandle stream_;
  std::string agent_label_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  base::WeakPtr<Coordinator> coordinator_;

  std::map<std::string, std::set<std::unique_ptr<Recorder>>> recorders_;

  // If |streaming_label_| is not empty, it shows the label for which we are
  // writing chunks to the output stream.
  std::string streaming_label_;
  std::unique_ptr<base::DictionaryValue> metadata_;
  bool stream_is_empty_;
  bool json_field_name_written_;

  DISALLOW_COPY_AND_ASSIGN(TraceStreamer);
};

Coordinator::Coordinator(AgentRegistry* agent_registry)
    : binding_(this),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      // USER_VISIBLE because the task posted from StopAndFlushAfterClockSync()
      // is required to stop tracing from the UI.
      // TODO(fdoray): Once we have support for dynamic priorities
      // (https://crbug.com/889029), use BEST_EFFORT initially and increase the
      // priority only when blocking the tracing UI.
      backend_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::WithBaseSyncPrimitives()})),
      agent_registry_(agent_registry),
      weak_ptr_factory_(this) {
  DCHECK(agent_registry_);
}

Coordinator::~Coordinator() {
  if (!stop_and_flush_callback_.is_null()) {
    base::ResetAndReturn(&stop_and_flush_callback_)
        .Run(base::Value(base::Value::Type::DICTIONARY));
  }
  if (!start_tracing_callback_.is_null())
    base::ResetAndReturn(&start_tracing_callback_).Run(false);
  if (!request_buffer_usage_callback_.is_null())
    base::ResetAndReturn(&request_buffer_usage_callback_).Run(false, 0, 0);
  if (!get_categories_callback_.is_null())
    base::ResetAndReturn(&get_categories_callback_).Run(false, "");

  if (trace_streamer_) {
    // We are in the middle of flushing trace data. We need to
    // 1- Close the stream so that the TraceStreamer does not block on writing
    //    to it.
    // 2- Delete the TraceStreamer on the backend task runner; it owns recorders
    //    that should be destructed on the backend task runner because they are
    //    bound on the backend task runner.
    trace_streamer_->CloseStream();
    backend_task_runner_->DeleteSoon(FROM_HERE, trace_streamer_.release());
  }
}

void Coordinator::BindCoordinatorRequest(
    mojom::CoordinatorRequest request,
    const service_manager::BindSourceInfo& source_info) {
  binding_.Bind(std::move(request));
}

void Coordinator::StartTracing(const std::string& config,
                               StartTracingCallback callback) {
  bool is_initializing = !start_tracing_callback_.is_null();
  if (is_initializing || (is_tracing_ && config == config_)) {
    std::move(callback).Run(config == config_);
    return;
  }

  is_tracing_ = true;
  config_ = config;
  parsed_config_ = base::trace_event::TraceConfig(config);
  agent_registry_->SetAgentInitializationCallback(base::BindRepeating(
      &Coordinator::SendStartTracingToAgent, weak_ptr_factory_.GetWeakPtr()));
  if (!agent_registry_->HasDisconnectClosure(&kStartTracingClosureName)) {
    std::move(callback).Run(true);
    return;
  }
  start_tracing_callback_ = std::move(callback);
}

void Coordinator::SendStartTracingToAgent(
    AgentRegistry::AgentEntry* agent_entry) {
  if (agent_entry->is_tracing())
    return;
  if (agent_entry->HasDisconnectClosure(&kStartTracingClosureName))
    return;
  if (!parsed_config_.process_filter_config().IsEnabled(agent_entry->pid()))
    return;
  agent_entry->AddDisconnectClosure(
      &kStartTracingClosureName,
      base::BindOnce(&Coordinator::OnTracingStarted,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Unretained(agent_entry), false));
  agent_entry->agent()->StartTracing(
      config_, TRACE_TIME_TICKS_NOW(),
      base::BindRepeating(&Coordinator::OnTracingStarted,
                          weak_ptr_factory_.GetWeakPtr(),
                          base::Unretained(agent_entry)));
}

void Coordinator::OnTracingStarted(AgentRegistry::AgentEntry* agent_entry,
                                   bool success) {
  agent_entry->set_is_tracing(success);
  bool removed =
      agent_entry->RemoveDisconnectClosure(&kStartTracingClosureName);
  DCHECK(removed);

  if (!agent_registry_->HasDisconnectClosure(&kStartTracingClosureName) &&
      !start_tracing_callback_.is_null()) {
    std::move(start_tracing_callback_).Run(true);
  }
}

void Coordinator::StopAndFlush(mojo::ScopedDataPipeProducerHandle stream,
                               StopAndFlushCallback callback) {
  StopAndFlushAgent(std::move(stream), "", std::move(callback));
}

void Coordinator::StopAndFlushAgent(mojo::ScopedDataPipeProducerHandle stream,
                                    const std::string& agent_label,
                                    StopAndFlushCallback callback) {
  if (!is_tracing_) {
    stream.reset();
    std::move(callback).Run(base::Value(base::Value::Type::DICTIONARY));
    return;
  }
  DCHECK(!trace_streamer_);
  DCHECK(stream.is_valid());
  is_tracing_ = false;

  // Do not send |StartTracing| to agents that connect from now on.
  agent_registry_->RemoveAgentInitializationCallback();
  trace_streamer_.reset(new Coordinator::TraceStreamer(
      std::move(stream), agent_label, task_runner_,
      weak_ptr_factory_.GetWeakPtr()));
  stop_and_flush_callback_ = std::move(callback);
  StopAndFlushInternal();
}

void Coordinator::StopAndFlushInternal() {
  if (agent_registry_->HasDisconnectClosure(&kStartTracingClosureName)) {
    // We received a |StopAndFlush| command before receiving |StartTracing| acks
    // from all agents. Let's retry after a delay.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindRepeating(&Coordinator::StopAndFlushInternal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            mojom::kStopTracingRetryTimeMilliseconds));
    return;
  }

  agent_registry_->ForAllAgents([this](AgentRegistry::AgentEntry* agent_entry) {
    if (!agent_entry->is_tracing() ||
        !agent_entry->supports_explicit_clock_sync()) {
      return;
    }
    const std::string sync_id = base::GenerateGUID();
    agent_entry->AddDisconnectClosure(
        &kRequestClockSyncMarkerClosureName,
        base::BindOnce(&Coordinator::OnRequestClockSyncMarkerResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(agent_entry), sync_id,
                       base::TimeTicks(), base::TimeTicks()));
    agent_entry->agent()->RequestClockSyncMarker(
        sync_id,
        base::BindRepeating(&Coordinator::OnRequestClockSyncMarkerResponse,
                            weak_ptr_factory_.GetWeakPtr(),
                            base::Unretained(agent_entry), sync_id));
  });
  if (!agent_registry_->HasDisconnectClosure(
          &kRequestClockSyncMarkerClosureName)) {
    StopAndFlushAfterClockSync();
  }
}

void Coordinator::OnRequestClockSyncMarkerResponse(
    AgentRegistry::AgentEntry* agent_entry,
    const std::string& sync_id,
    base::TimeTicks issue_ts,
    base::TimeTicks issue_end_ts) {
  bool removed =
      agent_entry->RemoveDisconnectClosure(&kRequestClockSyncMarkerClosureName);
  DCHECK(removed);

  // TODO(charliea): Change this function so that it can accept a boolean
  // success indicator instead of having to rely on sentinel issue_ts and
  // issue_end_ts values to signal failure.
  if (!(issue_ts == base::TimeTicks() || issue_end_ts == base::TimeTicks()))
    TRACE_EVENT_CLOCK_SYNC_ISSUER(sync_id, issue_ts, issue_end_ts);

  if (!agent_registry_->HasDisconnectClosure(
          &kRequestClockSyncMarkerClosureName)) {
    StopAndFlushAfterClockSync();
  }
}

void Coordinator::StopAndFlushAfterClockSync() {
  bool has_tracing_agents = false;
  agent_registry_->ForAllAgents(
      [this, &has_tracing_agents](AgentRegistry::AgentEntry* agent_entry) {
        if (!agent_entry->is_tracing())
          return;
        has_tracing_agents = true;
        backend_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&Coordinator::TraceStreamer::CreateAndSendRecorder,
                           trace_streamer_->AsWeakPtr(), agent_entry->label(),
                           agent_entry->type(), agent_entry->AsWeakPtr()));
      });
  if (!has_tracing_agents)
    OnFlushDone();
}

void Coordinator::SendRecorder(
    base::WeakPtr<AgentRegistry::AgentEntry> agent_entry,
    mojom::RecorderPtr recorder) {
  if (agent_entry) {
    agent_entry->agent()->StopAndFlush(std::move(recorder));
  } else {
    // Recorders are created and closed on |backend_task_runner_|.
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Coordinator::TraceStreamer::CloseRecorder,
                       trace_streamer_->AsWeakPtr(), std::move(recorder)));
  }
}

void Coordinator::OnFlushDone() {
  std::move(stop_and_flush_callback_)
      .Run(std::move(*trace_streamer_->GetMetadata()));
  backend_task_runner_->DeleteSoon(FROM_HERE, trace_streamer_.release());
  agent_registry_->ForAllAgents([](AgentRegistry::AgentEntry* agent_entry) {
    agent_entry->set_is_tracing(false);
  });
  is_tracing_ = false;
}

void Coordinator::IsTracing(IsTracingCallback callback) {
  std::move(callback).Run(is_tracing_);
}

void Coordinator::RequestBufferUsage(RequestBufferUsageCallback callback) {
  if (!request_buffer_usage_callback_.is_null()) {
    std::move(callback).Run(false, 0, 0);
    return;
  }

  maximum_trace_buffer_usage_ = 0;
  approximate_event_count_ = 0;
  request_buffer_usage_callback_ = std::move(callback);
  agent_registry_->ForAllAgents([this](AgentRegistry::AgentEntry* agent_entry) {
    agent_entry->AddDisconnectClosure(
        &kRequestBufferUsageClosureName,
        base::BindOnce(&Coordinator::OnRequestBufferStatusResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(agent_entry), 0 /* capacity */,
                       0 /* count */));
    agent_entry->agent()->RequestBufferStatus(base::BindRepeating(
        &Coordinator::OnRequestBufferStatusResponse,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(agent_entry)));
  });
}

void Coordinator::OnRequestBufferStatusResponse(
    AgentRegistry::AgentEntry* agent_entry,
    uint32_t capacity,
    uint32_t count) {
  bool removed =
      agent_entry->RemoveDisconnectClosure(&kRequestBufferUsageClosureName);
  DCHECK(removed);

  if (capacity > 0) {
    float percent_full =
        static_cast<float>(static_cast<double>(count) / capacity);
    maximum_trace_buffer_usage_ =
        std::max(maximum_trace_buffer_usage_, percent_full);
    approximate_event_count_ += count;
  }

  if (!agent_registry_->HasDisconnectClosure(&kRequestBufferUsageClosureName)) {
    std::move(request_buffer_usage_callback_)
        .Run(true, maximum_trace_buffer_usage_, approximate_event_count_);
  }
}

void Coordinator::GetCategories(GetCategoriesCallback callback) {
  if (is_tracing_) {
    std::move(callback).Run(false, "");
    return;
  }

  DCHECK(get_categories_callback_.is_null());
  is_tracing_ = true;
  category_set_.clear();
  get_categories_callback_ = std::move(callback);
  agent_registry_->ForAllAgents([this](AgentRegistry::AgentEntry* agent_entry) {
    agent_entry->AddDisconnectClosure(
        &kGetCategoriesClosureName,
        base::BindOnce(&Coordinator::OnGetCategoriesResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(agent_entry), ""));
    agent_entry->agent()->GetCategories(base::BindRepeating(
        &Coordinator::OnGetCategoriesResponse, weak_ptr_factory_.GetWeakPtr(),
        base::Unretained(agent_entry)));
  });
}

void Coordinator::OnGetCategoriesResponse(
    AgentRegistry::AgentEntry* agent_entry,
    const std::string& categories) {
  bool removed =
      agent_entry->RemoveDisconnectClosure(&kGetCategoriesClosureName);
  DCHECK(removed);

  std::vector<std::string> split = base::SplitString(
      categories, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& category : split) {
    category_set_.insert(category);
  }

  if (!agent_registry_->HasDisconnectClosure(&kGetCategoriesClosureName)) {
    std::vector<std::string> category_vector(category_set_.begin(),
                                             category_set_.end());
    std::move(get_categories_callback_)
        .Run(true, base::JoinString(category_vector, ","));
    is_tracing_ = false;
  }
}

}  // namespace tracing
