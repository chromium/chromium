// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_NAME_RECORDER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_NAME_RECORDER_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/process/current_process.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_enums.pbzero.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/track_event_internal.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.gen.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace tracing {

// A class that emits track descriptors for Chrome processes and threads.
class COMPONENT_EXPORT(TRACING_CPP) TrackNameRecorder
    : public perfetto::TrackEventSessionObserver,
      base::ThreadIdNameManager::Observer,
      base::CurrentProcess::Delegate {
 public:
  static TrackNameRecorder* GetInstance();

  TrackNameRecorder(const TrackNameRecorder&) = delete;
  TrackNameRecorder& operator=(const TrackNameRecorder&) = delete;

  // perfetto::TrackEventSessionObserver implementation
  void OnSetup(const perfetto::DataSourceBase::SetupArgs&) override;

  // base::ThreadIdNameManager::Observer implementation.
  void OnThreadNameChanged(const char* name) override;

  // base::CurrentProcess::Delegate implementation.
  void OnProcessNameChanged(const std::string& process_name,
                            base::CurrentProcessType process_type) override;

  // Processes can have labels in addition to their names. Use labels, for
  // instance, to list out the web page titles that a process is handling.
  int GetNewProcessLabelId();
  void UpdateProcessLabel(int label_id, const std::string& current_label);
  void RemoveProcessLabel(int label_id);

  static void SetRecordHostAppPackageName(bool record_host_app_package_name);

 private:
  friend class base::NoDestructor<TrackNameRecorder>;
  FRIEND_TEST_ALL_PREFIXES(TrackNameRecorderTest,
                           GenerateProcessTrackDescriptor);

  TrackNameRecorder();
  ~TrackNameRecorder() override;

  // Set the track descriptor for the current process.
  void SetProcessTrackDescriptor(
      const std::string& process_name,
      perfetto::protos::chrome_enums::pbzero::ProcessType process_type);
  void SetProcessTrackDescriptor();

  // Helper function for SetProcessTrackDescriptor.
  static perfetto::protos::gen::TrackDescriptor GenerateProcessTrackDescriptor(
      const perfetto::ProcessTrack& process_track,
      const std::string& process_name,
      perfetto::protos::chrome_enums::pbzero::ProcessType process_type,
      base::ProcessId process_id,
      int64_t process_start_timestamp,
      const absl::flat_hash_map<int, std::string>& process_labels,
      const std::optional<uint64_t>& crash_trace_id,
      const std::string& host_app_package_name);

  absl::flat_hash_map<int, std::string> process_labels() const {
    base::AutoLock lock(lock_);
    return process_labels_;
  }

  int64_t process_start_timestamp_;
  static bool record_host_app_package_name_;

  // This lock protects `process_labels_` member accesses from arbitrary
  // threads.
  mutable base::Lock lock_;
  int next_process_label_id_ GUARDED_BY(lock_) = 0;
  absl::flat_hash_map<int, std::string> process_labels_ GUARDED_BY(lock_);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACK_NAME_RECORDER_H_
