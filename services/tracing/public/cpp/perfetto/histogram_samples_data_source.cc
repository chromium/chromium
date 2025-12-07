// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/histogram_samples_data_source.h"

#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/trace_time.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/base/time.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/common/data_source_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/chrome_config.gen.h"
#include "third_party/perfetto/protos/perfetto/config/data_source_config.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet_defaults.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_histogram_sample.pbzero.h"

namespace tracing {
namespace {

// Counts data source that record all histograms;
std::atomic_size_t g_num_global_sample_observers{0};

std::optional<base::HistogramBase::Sample32> MaybeReferenceValue(
    bool has_value,
    uint64_t value) {
  if (has_value) {
    return base::checked_cast<base::HistogramBase::Sample32>(value);
  }
  return std::nullopt;
}

}  // namespace

struct HistogramSamplesIncrementalState {
  using InternedHistogramName =
      perfetto::SmallInternedDataTraits::Index<std::string_view>;

  bool was_cleared = true;
  InternedHistogramName histogram_names_;

  size_t GetInternedHistogramName(
      std::string_view value,
      HistogramSamplesDataSource::TraceContext::TracePacketHandle& packet) {
    size_t iid;
    if (histogram_names_.LookUpOrInsert(&iid, value)) {
      return iid;
    }
    auto* interned_data = packet->set_interned_data();
    auto* msg = interned_data->add_histogram_names();
    msg->set_iid(iid);
    msg->set_name(value.data(), value.size());
    return iid;
  }
};

struct HistogramSamplesTlsState {
  explicit HistogramSamplesTlsState(
      const HistogramSamplesDataSource::TraceContext& trace_context) {
    auto locked_ds = trace_context.GetDataSourceLocked();
    if (locked_ds.valid()) {
      filter_histogram_names = locked_ds->filter_histogram_names();
      if (!locked_ds->records_all_histograms()) {
        instance = reinterpret_cast<uintptr_t>(&(*locked_ds));
      }
    }
  }
  bool filter_histogram_names = false;
  std::optional<uintptr_t> instance;
};

void HistogramSamplesDataSource::Register() {
  perfetto::DataSourceDescriptor desc;
  desc.set_name(tracing::mojom::kHistogramSampleSourceName);
  perfetto::DataSource<HistogramSamplesDataSource,
                       HistogramSamplesTraits>::Register(desc);
}

HistogramSamplesDataSource::HistogramSamplesDataSource() = default;
HistogramSamplesDataSource::~HistogramSamplesDataSource() = default;

void HistogramSamplesDataSource::OnSetup(const SetupArgs& args) {
  if (args.config->chromium_histogram_samples_raw().empty()) {
    return;
  }
  perfetto::protos::gen::ChromiumHistogramSamplesConfig config;
  if (!config.ParseFromString(args.config->chromium_histogram_samples_raw())) {
    DLOG(ERROR) << "Failed to parse chromium_histogram_samples";
    return;
  }
  filter_histogram_names_ = config.filter_histogram_names();
  for (const auto& histogram : config.histograms()) {
    monitored_histograms_.push_back(histogram);
  }
}

void HistogramSamplesDataSource::OnStart(const StartArgs&) {
  if (monitored_histograms_.empty()) {
    size_t num_global_sample_observers =
        g_num_global_sample_observers.fetch_add(1, std::memory_order_relaxed);
    if (num_global_sample_observers == 0) {
      base::StatisticsRecorder::SetGlobalSampleCallback(
          &HistogramSamplesDataSource::OnAnyMetricSample);
    }
  }
  for (const auto& histogram : monitored_histograms_) {
    histogram_observers_.push_back(
        std::make_unique<
            base::StatisticsRecorder::ScopedHistogramSampleObserver>(
            histogram.histogram_name(),
            base::BindRepeating(&HistogramSamplesDataSource::OnMetricSample,
                                base::Unretained(this),
                                MaybeReferenceValue(histogram.has_min_value(),
                                                    histogram.min_value()),
                                MaybeReferenceValue(histogram.has_max_value(),
                                                    histogram.max_value()))));
  }
}

void HistogramSamplesDataSource::OnFlush(const FlushArgs&) {}

void HistogramSamplesDataSource::OnStop(const StopArgs&) {
  if (monitored_histograms_.empty()) {
    size_t num_global_sample_observers =
        g_num_global_sample_observers.fetch_sub(1, std::memory_order_relaxed);
    if (num_global_sample_observers == 1) {
      base::StatisticsRecorder::SetGlobalSampleCallback(nullptr);
    }
  }
  histogram_observers_.clear();
}

void HistogramSamplesDataSource::OnMetricSample(
    std::optional<base::HistogramBase::Sample32> reference_lower_value,
    std::optional<base::HistogramBase::Sample32> reference_upper_value,
    std::optional<uint64_t> event_id,
    std::string_view histogram_name,
    uint64_t name_hash,
    base::HistogramBase::Sample32 sample) {
  if ((reference_lower_value && sample < reference_lower_value) ||
      (reference_upper_value && sample > reference_upper_value)) {
    return;
  }
  OnMetricSampleImpl(event_id, histogram_name, name_hash, sample,
                     reinterpret_cast<uintptr_t>(this));
}

void HistogramSamplesDataSource::OnAnyMetricSample(
    std::string_view histogram_name,
    uint64_t name_hash,
    base::HistogramBase::Sample32 sample,
    std::optional<uint64_t> event_id) {
  OnMetricSampleImpl(event_id, histogram_name, name_hash, sample, std::nullopt);
}

void HistogramSamplesDataSource::OnMetricSampleImpl(
    std::optional<uint64_t> event_id,
    std::string_view histogram_name,
    uint64_t name_hash,
    base::HistogramBase::Sample32 sample,
    std::optional<uintptr_t> instance) {
  HistogramSamplesDataSource::Trace([&](TraceContext ctx) {
    if (instance != ctx.GetCustomTlsState()->instance) {
      return;
    }
    bool filter_histogram_names =
        ctx.GetCustomTlsState()->filter_histogram_names;
    auto* incr_state = ctx.GetIncrementalState();

    if (incr_state->was_cleared) {
      ResetIncrementalState(ctx, !instance.has_value());
      incr_state->was_cleared = false;
    }

    auto packet = ctx.NewTracePacket();
    std::optional<size_t> iid;
    if (!filter_histogram_names) {
      iid = incr_state->GetInternedHistogramName(histogram_name, packet);
    }

    packet->set_timestamp(
        TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds());
    packet->set_sequence_flags(
        ::perfetto::protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
    auto* event = packet->set_track_event();
    event->set_name_iid(1);
    event->add_category_iids(1);
    event->set_type(::perfetto::protos::pbzero::TrackEvent::TYPE_INSTANT);
    if (event_id) {
      event->add_flow_ids(*event_id);
    }

    perfetto::protos::pbzero::ChromeHistogramSample* new_sample =
        event->set_chrome_histogram_sample();
    new_sample->set_name_hash(name_hash);
    new_sample->set_sample(sample);

    if (iid) {
      new_sample->set_name_iid(*iid);
    }
  });
}

void HistogramSamplesDataSource::ResetIncrementalState(
    TraceContext& ctx,
    bool records_all_histograms) {
  uint64_t track_uuid;
  if (records_all_histograms) {
    auto track = perfetto::ThreadTrack::Current();
    perfetto::internal::TrackRegistry::Get()->SerializeTrack(
        track, ctx.NewTracePacket());
    track_uuid = track.uuid;
  } else {
    // Specific histogram samples are scoped to the process because the callback
    // is not called on the thread that emitted the record.
    auto track = perfetto::NamedTrack("HistogramSamples");
    perfetto::internal::TrackRegistry::Get()->SerializeTrack(
        track, ctx.NewTracePacket());
    track_uuid = track.uuid;
  }

  auto packet = ctx.NewTracePacket();
  packet->set_sequence_flags(
      ::perfetto::protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED);

  auto* defaults = packet->set_trace_packet_defaults();
  defaults->set_timestamp_clock_id(base::tracing::kTraceClockId);
  auto* track_defaults = defaults->set_track_event_defaults();
  track_defaults->set_track_uuid(track_uuid);

  auto* interned_data = packet->set_interned_data();
  auto* name = interned_data->add_event_names();
  name->set_iid(1);
  name->set_name("HistogramSample");

  auto* category = interned_data->add_event_categories();
  category->set_iid(1);
  // This is a synthetic category that's created by this data source, although
  // it can't be enabled on TrackEvent. The data is emitted this way mainly for
  // backward compatibility with existing trace processor.
  category->set_name(TRACE_DISABLED_BY_DEFAULT("histogram_samples"));
}

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    COMPONENT_EXPORT(TRACING_CPP),
    tracing::HistogramSamplesDataSource,
    tracing::HistogramSamplesTraits);
