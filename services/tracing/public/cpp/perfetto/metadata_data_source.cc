// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/metadata_data_source.h"

#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time_override.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/tracing/trace_time.h"
#include "components/variations/active_field_trials.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/perfetto/protos/perfetto/common/data_source_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/chrome_config.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/extension_descriptor.pbzero.h"

#if BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)
#include "base/android/apk_info.h"
#endif

namespace tracing {
namespace {

inline constexpr char kCommandLineKey[] = "command_line";
inline constexpr char kClockDomainMetadataKey[] = "clock-domain";
inline constexpr char kTraceCaptureDatetimeKey[] = "trace-capture-datetime";

inline constexpr char kCpuCoresMetadataKey[] = "cpu-num-cores";
inline constexpr char kOSNameMetadataKey[] = "os-name";
inline constexpr char kOSVersionMetadataKey[] = "os-version";

}  // namespace

std::string_view GetClockString(base::TimeTicks::Clock clock) {
  switch (clock) {
    case base::TimeTicks::Clock::FUCHSIA_ZX_CLOCK_MONOTONIC:
      return "FUCHSIA_ZX_CLOCK_MONOTONIC";
    case base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC:
      return "LINUX_CLOCK_MONOTONIC";
    case base::TimeTicks::Clock::IOS_CF_ABSOLUTE_TIME_MINUS_KERN_BOOTTIME:
      return "IOS_CF_ABSOLUTE_TIME_MINUS_KERN_BOOTTIME";
    case base::TimeTicks::Clock::MAC_MACH_ABSOLUTE_TIME:
      return "MAC_MACH_ABSOLUTE_TIME";
    case base::TimeTicks::Clock::WIN_QPC:
      return "WIN_QPC";
    case base::TimeTicks::Clock::WIN_ROLLOVER_PROTECTED_TIME_GET_TIME:
      return "WIN_ROLLOVER_PROTECTED_TIME_GET_TIME";
  }

  NOTREACHED();
}

struct MetadataDataSourceTlsState {
  explicit MetadataDataSourceTlsState(
      const MetadataDataSource::TraceContext& trace_context) {
    auto locked_ds = trace_context.GetDataSourceLocked();
    if (locked_ds.valid()) {
      privacy_filtering_enabled = locked_ds->privacy_filtering_enabled();
      instance = reinterpret_cast<uintptr_t>(&(*locked_ds));
    }
  }
  bool privacy_filtering_enabled = false;
  uintptr_t instance = 0;
};

void MetadataDataSource::Register(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::vector<BundleRecorder> bundle_recorders,
    std::vector<PacketRecorder> packet_recorders) {
  perfetto::DataSourceDescriptor desc;
  desc.set_name(tracing::mojom::kMetaData2SourceName);
  perfetto::DataSource<MetadataDataSource, MetadataDataSourceTraits>::Register(
      desc, std::move(task_runner), std::move(bundle_recorders),
      std::move(packet_recorders));
}

MetadataDataSource::MetadataDataSource(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::vector<BundleRecorder> bundle_recorders,
    std::vector<PacketRecorder> packet_recorders)
    : task_runner_(std::move(task_runner)),
      bundle_recorders_(std::move(bundle_recorders)),
      packet_recorders_(std::move(packet_recorders)) {}

MetadataDataSource::~MetadataDataSource() = default;

void MetadataDataSource::OnSetup(const SetupArgs& args) {
  const perfetto::protos::gen::ChromeConfig& chrome_config =
      args.config->chrome_config();
  privacy_filtering_enabled_ = chrome_config.privacy_filtering_enabled();
}

void MetadataDataSource::OnStart(const StartArgs&) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&MetadataDataSource::WriteMetadata,
                                        reinterpret_cast<uintptr_t>(this),
                                        std::move(bundle_recorders_),
                                        std::move(packet_recorders_)));
}

void MetadataDataSource::OnFlush(const FlushArgs&) {}

void MetadataDataSource::OnStop(const StopArgs&) {}

void MetadataDataSource::WriteMetadata(
    uintptr_t instance,
    std::vector<BundleRecorder> bundle_recorders,
    std::vector<PacketRecorder> packet_recorders) {
  MetadataDataSource::Trace([&](TraceContext ctx) {
    if (instance != ctx.GetCustomTlsState()->instance) {
      return;
    }
    bool privacy_filtering_enabled =
        ctx.GetCustomTlsState()->privacy_filtering_enabled;

    auto now = TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds();

    if (!privacy_filtering_enabled) {
      auto packet = ctx.NewTracePacket();
      packet->set_timestamp(now);
      packet->set_timestamp_clock_id(base::tracing::kTraceClockId);
      auto* bundle = packet->set_chrome_events();
      base::CommandLine::StringType command_line =
          base::CommandLine::ForCurrentProcess()->GetCommandLineString();
#if BUILDFLAG(IS_WIN)
      AddMetadataToBundle(kCommandLineKey, base::WideToUTF8(command_line),
                          bundle);
#else
      AddMetadataToBundle(kCommandLineKey, command_line, bundle);
#endif
      AddMetadataToBundle(kClockDomainMetadataKey,
                          GetClockString(base::TimeTicks::GetClock()), bundle);
      AddMetadataToBundle(
          kTraceCaptureDatetimeKey,
          base::UnlocalizedTimeFormatWithPattern(
              TRACE_TIME_NOW(), "y-M-d H:m:s", icu::TimeZone::getGMT()),
          bundle);
      for (auto& recorder : bundle_recorders) {
        if (recorder.is_null()) {
          continue;
        }
        recorder.Run(bundle);
      }
    }
    for (auto& recorder : packet_recorders) {
      if (recorder.is_null()) {
        continue;
      }
      auto packet = ctx.NewTracePacket();
      packet->set_timestamp(now);
      packet->set_timestamp_clock_id(base::tracing::kTraceClockId);

      recorder.Run(packet.get(), privacy_filtering_enabled);
    }

    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(now);
    packet->set_timestamp_clock_id(base::tracing::kTraceClockId);
    auto* chrome_metadata = packet->set_chrome_metadata();

#if BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)
    // Version code is only set for official builds on Android.
    const std::string& version_code_str =
        base::android::apk_info::package_version_code();
    if (!version_code_str.empty()) {
      int version_code = 0;
      bool res = base::StringToInt(version_code_str, &version_code);
      DCHECK(res);
      chrome_metadata->set_chrome_version_code(version_code);
    }
#endif  // BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)

    // Do not include low anonymity field trials, to prevent them from being
    // included in chrometto reports.
    std::vector<variations::ActiveGroupId> active_group_ids;
    variations::GetFieldTrialActiveGroupIds(std::string_view(),
                                            &active_group_ids);

    for (const auto& active_group_id : active_group_ids) {
      perfetto::protos::pbzero::ChromeMetadataPacket::FinchHash* finch_hash =
          chrome_metadata->add_field_trial_hashes();
      finch_hash->set_name(active_group_id.name);
      finch_hash->set_group(active_group_id.group);
    }
    packet->Finalize();
    ctx.Flush();
  });
}

void MetadataDataSource::AddMetadataToBundle(
    std::string_view name,
    std::string_view value,
    perfetto::protos::pbzero::ChromeEventBundle* bundle) {
  auto* metadata = bundle->add_metadata();
  metadata->set_name(name.data(), name.size());
  metadata->set_string_value(value.data(), value.size());
}

void MetadataDataSource::AddMetadataToBundle(
    std::string_view name,
    const base::Value& value,
    perfetto::protos::pbzero::ChromeEventBundle* bundle) {
  auto* metadata = bundle->add_metadata();
  metadata->set_name(name.data(), name.size());

  if (value.is_int()) {
    metadata->set_int_value(value.GetInt());
  } else if (value.is_bool()) {
    metadata->set_bool_value(value.GetBool());
  } else if (value.is_string()) {
    metadata->set_string_value(value.GetString());
  } else {
    metadata->set_json_value(base::WriteJson(value).value_or(""));
  }
}

void MetadataDataSource::RecordDefaultBundleMetadata(
    perfetto::protos::pbzero::ChromeEventBundle* bundle) {
#if BUILDFLAG(IS_CHROMEOS)
  MetadataDataSource::AddMetadataToBundle(kOSNameMetadataKey, "CrOS", bundle);
#else
  MetadataDataSource::AddMetadataToBundle(
      kOSNameMetadataKey, base::SysInfo::OperatingSystemName(), bundle);
#endif
  MetadataDataSource::AddMetadataToBundle(
      kOSVersionMetadataKey, base::SysInfo::OperatingSystemVersion(), bundle);

  MetadataDataSource::AddMetadataToBundle(
      kCpuCoresMetadataKey,
      base::Value(static_cast<int>(base::SysInfo::NumberOfProcessors())),
      bundle);
}

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    COMPONENT_EXPORT(TRACING_CPP),
    tracing::MetadataDataSource,
    tracing::MetadataDataSourceTraits);
