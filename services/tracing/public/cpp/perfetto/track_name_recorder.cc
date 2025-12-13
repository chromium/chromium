// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/track_name_recorder.h"

#include "base/debug/crash_logging.h"
#include "base/no_destructor.h"
#include "base/process/current_process.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "base/tracing/protos/chrome_enums.pbzero.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/trace_string_lookup.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/track_event_internal.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_process_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_thread_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.gen.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_info.h"
#endif

namespace tracing {

namespace {

namespace pbzero_enums = perfetto::protos::chrome_enums::pbzero;

std::optional<uint64_t> GetTraceCrashId() {
  static base::debug::CrashKeyString* key = base::debug::AllocateCrashKeyString(
      "chrome-trace-id", base::debug::CrashKeySize::Size32);
  if (!key) {
    return std::nullopt;
  }
  uint64_t id = base::RandUint64();
  base::debug::SetCrashKeyString(key, base::NumberToString(id));
  return id;
}

void FillThreadTrack(const perfetto::ThreadTrack& track, const char* name) {
  using perfetto::protos::gen::ChromeThreadDescriptor;

  auto desc = track.Serialize();
  desc.mutable_thread()->set_pid(static_cast<int>(
      base::trace_event::TraceLog::GetInstance()->process_id()));
  desc.mutable_thread()->set_thread_name(name);
  pbzero_enums::ThreadType thread_type = GetThreadType(name);
  if (thread_type != pbzero_enums::THREAD_UNSPECIFIED) {
    desc.mutable_chrome_thread()->set_thread_type(
        static_cast<int32_t>(thread_type));
  }
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_AIX)
  if (base::GetCurrentProcId() !=
      base::trace_event::TraceLog::GetInstance()->process_id()) {
    desc.mutable_chrome_thread()->set_is_sandboxed_tid(true);
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_AIX)

  base::TrackEvent::SetTrackDescriptor(track, std::move(desc));
}

// Set track descriptors for all threads that exist in the current process
// at this moment. OnThreadNameChanged can be called concurrently with this
// method, but this is not a problem because the Perfetto code is guarded
// by a lock and the value of the thread descriptor is the same.
// Should be called on the main thread.
void SetThreadTrackDescriptors() {
  auto* thread_id_name_manager = base::ThreadIdNameManager::GetInstance();
  const auto thread_ids = thread_id_name_manager->GetIds();
  for (base::PlatformThreadId thread_id : thread_ids) {
    const char* thread_name = thread_id_name_manager->GetName(thread_id);
    auto thread_track = perfetto::ThreadTrack::ForThread(thread_id.raw());
    FillThreadTrack(thread_track, thread_name);
  }

  // Main thread is special, it's not registered with ThreadIdNameManager.
  const char* thread_name = thread_id_name_manager->GetNameForCurrentThread();
  auto thread_track = perfetto::ThreadTrack::Current();
  FillThreadTrack(thread_track, thread_name);
}

}  // namespace

bool TrackNameRecorder::record_host_app_package_name_ = false;

void TrackNameRecorder::SetProcessTrackDescriptor(
    const std::string& process_name,
    pbzero_enums::ProcessType process_type) {
  // Add the crash trace ID to all the traces uploaded. If there are crashes
  // during this tracing session, then the crash will contain the process's
  // trace ID as "chrome-trace-id" crash key. This should be emitted
  // periodically to ensure it is present in the traces when the process
  // crashes. Metadata can go missing if process crashes. So, record this in
  // process descriptor.
  static const std::optional<uint64_t> crash_trace_id = GetTraceCrashId();

  std::string host_package_name;
#if BUILDFLAG(IS_ANDROID)
  // Host app package name is only recorded if the corresponding TraceLog
  // setting is set to true.
  if (record_host_app_package_name_) {
    // Host app package name is used to group information from different
    // processes that "belong" to the same WebView app.
    if (process_type == pbzero_enums::PROCESS_RENDERER ||
        process_type == pbzero_enums::PROCESS_BROWSER) {
      host_package_name = base::android::apk_info::host_package_name();
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  auto process_track = perfetto::ProcessTrack::Current();
  base::TrackEvent::SetTrackDescriptor(
      process_track,
      GenerateProcessTrackDescriptor(
          process_track, process_name, process_type,
          base::trace_event::TraceLog::GetInstance()->process_id(),
          process_start_timestamp_, process_labels(), crash_trace_id,
          host_package_name));
}

// static
perfetto::protos::gen::TrackDescriptor
TrackNameRecorder::GenerateProcessTrackDescriptor(
    const perfetto::ProcessTrack& process_track,
    const std::string& process_name,
    pbzero_enums::ProcessType process_type,
    base::ProcessId process_id,
    int64_t process_start_timestamp,
    const absl::flat_hash_map<int, std::string>& process_labels,
    const std::optional<uint64_t>& crash_trace_id,
    const std::string& host_app_package_name) {
  auto process_track_desc = process_track.Serialize();

  // We record a few (string) fields here that are stripped for background
  // tracing. We rely on the post-process privacy filtering to remove them.
  auto* process = process_track_desc.mutable_process();
  process->set_pid(process_id);
  process->set_process_name(process_name);
  process->set_start_timestamp_ns(process_start_timestamp);
  for (const auto& label : process_labels) {
    process->add_process_labels(label.second);
  }

  auto* chrome_process = process_track_desc.mutable_chrome_process();
  if (process_type != pbzero_enums::PROCESS_UNSPECIFIED) {
    chrome_process->set_process_type(static_cast<int32_t>(process_type));
  }

  if (crash_trace_id) {
    chrome_process->set_crash_trace_id(*crash_trace_id);
  }

  if (!host_app_package_name.empty()) {
    chrome_process->set_host_app_package_name(host_app_package_name);
  }

  return process_track_desc;
}

TrackNameRecorder::TrackNameRecorder()
    : process_start_timestamp_(
          TRACE_TIME_TICKS_NOW().since_origin().InNanoseconds()) {
  CHECK(perfetto::Tracing::IsInitialized());
  base::ThreadIdNameManager::GetInstance()->AddObserver(this);
  base::CurrentProcess::GetInstance().SetDelegate(this, {});
  base::TrackEvent::AddSessionObserver(this);
  SetThreadTrackDescriptors();
}

TrackNameRecorder::~TrackNameRecorder() = default;

// static
TrackNameRecorder* TrackNameRecorder::GetInstance() {
  static base::NoDestructor<TrackNameRecorder> instance;
  return instance.get();
}

void TrackNameRecorder::OnSetup(const perfetto::DataSourceBase::SetupArgs&) {
  SetProcessTrackDescriptor();
}

void TrackNameRecorder::OnThreadNameChanged(const char* name) {
  FillThreadTrack(perfetto::ThreadTrack::Current(), name);
}

void TrackNameRecorder::OnProcessNameChanged(
    const std::string& process_name,
    base::CurrentProcessType process_type) {
  SetProcessTrackDescriptor(process_name, process_type);
}

int TrackNameRecorder::GetNewProcessLabelId() {
  base::AutoLock lock(lock_);
  return next_process_label_id_++;
}

void TrackNameRecorder::UpdateProcessLabel(int label_id,
                                           const std::string& current_label) {
  if (!current_label.length()) {
    return RemoveProcessLabel(label_id);
  }

  auto track = perfetto::ProcessTrack::Current();
  auto desc = track.Serialize();
  desc.mutable_process()->add_process_labels(current_label);
  base::TrackEvent::SetTrackDescriptor(track, std::move(desc));

  base::AutoLock lock(lock_);
  process_labels_[label_id] = current_label;
}

void TrackNameRecorder::RemoveProcessLabel(int label_id) {
  base::AutoLock lock(lock_);
  process_labels_.erase(label_id);
}

void TrackNameRecorder::SetProcessTrackDescriptor() {
  std::string process_name = base::CurrentProcess::GetInstance().GetName({});
  auto process_type = base::CurrentProcess::GetInstance().GetType({});
  SetProcessTrackDescriptor(process_name, process_type);
}

// static
void TrackNameRecorder::SetRecordHostAppPackageName(
    bool record_host_app_package_name) {
  record_host_app_package_name_ = record_host_app_package_name;
}

}  // namespace tracing
