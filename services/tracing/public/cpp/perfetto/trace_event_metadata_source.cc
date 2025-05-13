// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_event_metadata_source.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/common/scoped_defer_task_posting.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/track_name_recorder.h"
#include "services/tracing/public/cpp/trace_event_args_allowlist.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/message.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

using TraceConfig = base::trace_event::TraceConfig;
using TraceRecordMode = base::trace_event::TraceRecordMode;
using perfetto::protos::pbzero::ChromeMetadataPacket;

namespace tracing {
namespace {

TraceEventMetadataSource* g_trace_event_metadata_source_for_testing = nullptr;

// Helper class used to ensure no tasks are posted while
// TraceEventMetadataSource::lock_ is held.
class SCOPED_LOCKABLE AutoLockWithDeferredTaskPosting {
  STACK_ALLOCATED();

 public:
  explicit AutoLockWithDeferredTaskPosting(base::Lock& lock)
      EXCLUSIVE_LOCK_FUNCTION(lock)
      : autolock_(lock) {}

  ~AutoLockWithDeferredTaskPosting() UNLOCK_FUNCTION() = default;

 private:
  // The ordering is important: |defer_task_posting_| must be destroyed
  // after |autolock_| to ensure the lock is not held when any deferred
  // tasks are posted..
  base::ScopedDeferTaskPosting defer_task_posting_;
  base::AutoLock autolock_;
};

}  // namespace

using perfetto::protos::pbzero::ChromeEventBundle;
using ChromeEventBundleHandle = protozero::MessageHandle<ChromeEventBundle>;

// static
TraceEventMetadataSource* TraceEventMetadataSource::GetInstance() {
  static base::NoDestructor<TraceEventMetadataSource> instance;
  return instance.get();
}

TraceEventMetadataSource::TraceEventMetadataSource()
    : DataSourceBase(mojom::kMetaDataSourceName),
      origin_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  g_trace_event_metadata_source_for_testing = this;
  AddGeneratorFunction(base::BindRepeating(
      &TraceEventMetadataSource::WriteMetadataPacket, base::Unretained(this)));
  AddGeneratorFunction(base::BindRepeating(
      &TraceEventMetadataSource::GenerateTraceConfigMetadataDict,
      base::Unretained(this)));

  perfetto::DataSourceDescriptor dsd;
  dsd.set_name(mojom::kMetaDataSourceName);
  DataSourceProxy::Register(dsd, this);
}

TraceEventMetadataSource::~TraceEventMetadataSource() = default;

void TraceEventMetadataSource::AddGeneratorFunction(
    JsonMetadataGeneratorFunction generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(lock_);
    json_generator_functions_.push_back(generator);
  }
  // An EventBundle is created when nullptr is passed.
  GenerateJsonMetadataFromGenerator(generator, nullptr);
}

void TraceEventMetadataSource::AddGeneratorFunction(
    MetadataGeneratorFunction generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(lock_);
    generator_functions_.push_back(generator);
  }
  GenerateMetadataFromGenerator(generator);
}

void TraceEventMetadataSource::AddGeneratorFunction(
    PacketGeneratorFunction generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  {
    base::AutoLock lock(lock_);
    packet_generator_functions_.push_back(generator);
  }
  GenerateMetadataPacket(generator);
}

void TraceEventMetadataSource::WriteMetadataPacket(
    perfetto::protos::pbzero::ChromeMetadataPacket* metadata_proto,
    bool privacy_filtering_enabled) {
#if BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)
  // Version code is only set for official builds on Android.
  const std::string& version_code_str =
      base::android::BuildInfo::GetInstance()->package_version_code();
  if (!version_code_str.empty()) {
    int version_code = 0;
    bool res = base::StringToInt(version_code_str, &version_code);
    DCHECK(res);
    metadata_proto->set_chrome_version_code(version_code);
  }
#endif  // BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)

  AutoLockWithDeferredTaskPosting lock(lock_);

  if (parsed_chrome_config_) {
    metadata_proto->set_enabled_categories(
        parsed_chrome_config_->ToCategoryFilterString());
  }
}

std::optional<base::Value::Dict>
TraceEventMetadataSource::GenerateTraceConfigMetadataDict() {
  AutoLockWithDeferredTaskPosting lock(lock_);
  if (chrome_config_.empty()) {
    return std::nullopt;
  }

  base::Value::Dict metadata;
  // If argument filtering is enabled, we need to check if the trace config is
  // allowlisted before emitting it.
  // TODO(eseckler): Figure out a way to solve this without calling directly
  // into IsMetadataAllowlisted().
  if (!parsed_chrome_config_->IsArgumentFilterEnabled() ||
      IsMetadataAllowlisted("trace-config")) {
    metadata.Set("trace-config", chrome_config_);
  } else {
    metadata.Set("trace-config", "__stripped__");
  }

  chrome_config_ = std::string();
  return metadata;
}

void TraceEventMetadataSource::GenerateMetadataFromGenerator(
    const TraceEventMetadataSource::MetadataGeneratorFunction& generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    if (!emit_metadata_at_start_) {
      return;
    }
  }
  DataSourceProxy::Trace([&](DataSourceProxy::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
    packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
    auto* chrome_metadata = packet->set_chrome_metadata();
    generator.Run(chrome_metadata, privacy_filtering_enabled_);
  });
}

void TraceEventMetadataSource::GenerateMetadataPacket(
    const TraceEventMetadataSource::PacketGeneratorFunction& generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    if (!emit_metadata_at_start_) {
      return;
    }
  }
  DataSourceProxy::Trace([&](DataSourceProxy::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
    packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
    generator.Run(packet.get(), privacy_filtering_enabled_);
  });
}

void TraceEventMetadataSource::GenerateJsonMetadataFromGenerator(
    const TraceEventMetadataSource::JsonMetadataGeneratorFunction& generator,
    ChromeEventBundle* event_bundle) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

  auto write_to_bundle = [&generator](ChromeEventBundle* bundle) {
    std::optional<base::Value::Dict> metadata_dict = generator.Run();
    if (!metadata_dict) {
      return;
    }
    for (auto it : *metadata_dict) {
      auto* new_metadata = bundle->add_metadata();
      new_metadata->set_name(it.first.c_str());

      if (it.second.is_int()) {
        new_metadata->set_int_value(it.second.GetInt());
      } else if (it.second.is_bool()) {
        new_metadata->set_bool_value(it.second.GetBool());
      } else if (it.second.is_string()) {
        new_metadata->set_string_value(it.second.GetString().c_str());
      } else {
        std::string json_value;
        base::JSONWriter::Write(it.second, &json_value);
        new_metadata->set_json_value(json_value.c_str());
      }
    }
  };

  if (event_bundle) {
    write_to_bundle(event_bundle);
    return;
  }
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    if (!emit_metadata_at_start_) {
      return;
    }
  }
  DataSourceProxy::Trace([&](DataSourceProxy::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
    packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
    write_to_bundle(packet->set_chrome_events());
  });
}

void TraceEventMetadataSource::GenerateMetadata(
    std::unique_ptr<
        std::vector<TraceEventMetadataSource::JsonMetadataGeneratorFunction>>
        json_generators,
    std::unique_ptr<
        std::vector<TraceEventMetadataSource::MetadataGeneratorFunction>>
        proto_generators,
    std::unique_ptr<
        std::vector<TraceEventMetadataSource::PacketGeneratorFunction>>
        packet_generators) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

  bool privacy_filtering_enabled;
  base::trace_event::TraceRecordMode record_mode;
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    privacy_filtering_enabled = privacy_filtering_enabled_;
    record_mode = parsed_chrome_config_->GetTraceRecordMode();
  }

  DataSourceProxy::Trace([&](DataSourceProxy::TraceContext ctx) {
    for (auto& generator : *packet_generators) {
      auto packet = ctx.NewTracePacket();
      packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
      packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
      generator.Run(packet.get(), privacy_filtering_enabled);
    }

    {
      auto trace_packet = ctx.NewTracePacket();
      trace_packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
      trace_packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
      auto* chrome_metadata = trace_packet->set_chrome_metadata();
      for (auto& generator : *proto_generators) {
        generator.Run(chrome_metadata, privacy_filtering_enabled);
      }
    }

    if (!privacy_filtering_enabled) {
      auto trace_packet = ctx.NewTracePacket();
      trace_packet->set_timestamp(base::TrackEvent::GetTraceTimeNs());
      trace_packet->set_timestamp_clock_id(base::TrackEvent::GetTraceClockId());
      ChromeEventBundle* event_bundle = trace_packet->set_chrome_events();
      for (auto& generator : *json_generators) {
        GenerateJsonMetadataFromGenerator(generator, event_bundle);
      }
    }

    // When not using ring buffer mode (but instead record-until-full / discard
    // mode), force flush the packets since the default flush happens at end of
    // the trace, and the trace writer's chunk could then be discarded.
    if (record_mode != base::trace_event::RECORD_CONTINUOUSLY) {
      ctx.Flush();
    }
  });
}

void TraceEventMetadataSource::StartTracingImpl(
    const perfetto::DataSourceConfig& data_source_config) {
  auto json_generators =
      std::make_unique<std::vector<JsonMetadataGeneratorFunction>>();
  auto proto_generators =
      std::make_unique<std::vector<MetadataGeneratorFunction>>();
  auto packet_generators =
      std::make_unique<std::vector<PacketGeneratorFunction>>();
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    privacy_filtering_enabled_ =
        data_source_config.chrome_config().privacy_filtering_enabled();
    chrome_config_ = data_source_config.chrome_config().trace_config();
    parsed_chrome_config_ = std::make_unique<TraceConfig>(chrome_config_);
    switch (parsed_chrome_config_->GetTraceRecordMode()) {
      case TraceRecordMode::RECORD_UNTIL_FULL:
      case TraceRecordMode::RECORD_AS_MUCH_AS_POSSIBLE: {
        emit_metadata_at_start_ = true;
        *json_generators = json_generator_functions_;
        *proto_generators = generator_functions_;
        *packet_generators = packet_generator_functions_;
        break;
      }
      case TraceRecordMode::RECORD_CONTINUOUSLY:
      case TraceRecordMode::ECHO_TO_CONSOLE:
        emit_metadata_at_start_ = false;
        return;
    }
  }
  // |emit_metadata_at_start_| is true if we are in discard packets mode, write
  // metadata at the beginning of the trace to make it less likely to be
  // dropped.
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&TraceEventMetadataSource::GenerateMetadata,
                     base::Unretained(this), std::move(json_generators),
                     std::move(proto_generators),
                     std::move(packet_generators)));
}

void TraceEventMetadataSource::StopTracingImpl(
    base::OnceClosure stop_complete_callback) {
  base::OnceClosure maybe_generate_task = base::DoNothing();
  {
    AutoLockWithDeferredTaskPosting lock(lock_);
    if (!emit_metadata_at_start_) {
      // Write metadata at the end of tracing if not emitted at start (in ring
      // buffer mode), to make it less likely that it is overwritten by other
      // trace data in perfetto's ring buffer.
      auto json_generators =
          std::make_unique<std::vector<JsonMetadataGeneratorFunction>>();
      *json_generators = json_generator_functions_;
      auto proto_generators =
          std::make_unique<std::vector<MetadataGeneratorFunction>>();
      *proto_generators = generator_functions_;
      auto packet_generators =
          std::make_unique<std::vector<PacketGeneratorFunction>>();
      *packet_generators = packet_generator_functions_;
      maybe_generate_task = base::BindOnce(
          &TraceEventMetadataSource::GenerateMetadata, base::Unretained(this),
          std::move(json_generators), std::move(proto_generators),
          std::move(packet_generators));
    }
  }
  // Even when not generating metadata, make sure the metadata generate task
  // posted at the start is finished, by posting task on origin task runner.
  origin_task_runner_->PostTaskAndReply(
      FROM_HERE, std::move(maybe_generate_task),
      base::BindOnce(
          [](TraceEventMetadataSource* ds,
             base::OnceClosure stop_complete_callback) {
            {
              AutoLockWithDeferredTaskPosting lock(ds->lock_);
              DataSourceProxy::Trace(
                  [&](DataSourceProxy::TraceContext ctx) { ctx.Flush(); });
              ds->chrome_config_ = std::string();
              ds->parsed_chrome_config_.reset();
              ds->emit_metadata_at_start_ = false;
            }
            std::move(stop_complete_callback).Run();
          },
          base::Unretained(this), std::move(stop_complete_callback)));
}

void TraceEventMetadataSource::Flush(
    base::RepeatingClosure flush_complete_callback) {
  origin_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                        std::move(flush_complete_callback));
}

base::SequencedTaskRunner* TraceEventMetadataSource::GetTaskRunner() {
  // Much like the data source, the task runner is long-lived, so returning the
  // raw pointer here is safe.
  base::AutoLock lock(lock_);
  return origin_task_runner_.get();
}

void TraceEventMetadataSource::ResetForTesting() {
  if (!g_trace_event_metadata_source_for_testing) {
    return;
  }
  g_trace_event_metadata_source_for_testing->~TraceEventMetadataSource();
  new (g_trace_event_metadata_source_for_testing) TraceEventMetadataSource;
}

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    COMPONENT_EXPORT(TRACING_CPP),
    tracing::TraceEventMetadataSource::DataSourceProxy);
