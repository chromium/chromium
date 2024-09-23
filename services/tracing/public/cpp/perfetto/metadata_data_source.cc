// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/metadata_data_source.h"

#include "base/trace_event/trace_event.h"
#include "base/tracing/trace_time.h"
#include "components/variations/active_field_trials.h"
#include "third_party/perfetto/protos/perfetto/common/data_source_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"

#if BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)
#include "base/android/build_info.h"
#include "base/strings/string_number_conversions.h"
#endif

namespace tracing {

void MetadataDataSource::Register() {
  perfetto::DataSourceDescriptor desc;
  desc.set_name("org.chromium.trace_metadata2");
  perfetto::DataSource<MetadataDataSource>::Register(desc);
}

void MetadataDataSource::OnStart(const StartArgs&) {}

void MetadataDataSource::OnFlush(const FlushArgs&) {
  WriteMetadata();
}

void MetadataDataSource::OnStop(const StopArgs&) {
  WriteMetadata();
}

void MetadataDataSource::WriteMetadata() {
  Trace([&](TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp(
        TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds());
    packet->set_timestamp_clock_id(base::tracing::kTraceClockId);

    auto* chrome_metadata = packet->set_chrome_metadata();

#if BUILDFLAG(IS_ANDROID) && defined(OFFICIAL_BUILD)
    // Version code is only set for official builds on Android.
    const char* version_code_str =
        base::android::BuildInfo::GetInstance()->package_version_code();
    if (version_code_str) {
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
  });
}

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    COMPONENT_EXPORT(TRACING_CPP),
    tracing::MetadataDataSource);
