// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "media/capabilities/webrtc_video_stats.pb.h"
#include "media/capabilities/webrtc_video_stats_db_impl.h"
#include "media/mojo/mojom/webrtc_video_perf.mojom-mojolpm.h"
#include "media/mojo/services/webrtc_video_perf_history.h"
#include "media/mojo/services/webrtc_video_perf_mojolpm_fuzzer.pb.h"
#include "media/mojo/services/webrtc_video_perf_recorder.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

namespace media {

// Helper class to call private constructor of friend class.
class WebrtcVideoPerfLPMFuzzerHelper {
 public:
  static std::unique_ptr<WebrtcVideoStatsDBImpl> CreateWebrtcVideoStatsDbImpl(
      std::unique_ptr<leveldb_proto::ProtoDatabase<WebrtcVideoStatsEntryProto>>
          proto_db) {
    return base::WrapUnique(new WebrtcVideoStatsDBImpl(std::move(proto_db)));
  }
};

namespace {

struct InitGlobals {
  InitGlobals() {
    // The call to CommandLine::Init is needed so that TestTimeouts::Initialize
    // does not fail.
    bool success = base::CommandLine::Init(0, nullptr);
    DCHECK(success);
    // TaskEnvironment requires TestTimeouts initialization to watch for
    // problematic long-running tasks.
    TestTimeouts::Initialize();

    // Mark this thread as an IO_THREAD with MOCK_TIME, and ensure that Now()
    // is driven from the same mock clock.
    task_environment = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::IO,
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  }

  // This allows us to mock time for all threads.
  std::unique_ptr<base::test::TaskEnvironment> task_environment;
};

InitGlobals* init_globals = new InitGlobals();

base::test::TaskEnvironment& GetEnvironment() {
  return *init_globals->task_environment;
}

scoped_refptr<base::SingleThreadTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().GetMainThreadTaskRunner();
}

// This in-memory database uses the FakeDB proto implementation as the
// underlying storage. The underlying FakeDB class requires that all callbacks
// are triggered manually. This class is used as a convenience class triggering
// the callbacks with success=true.
class InMemoryWebrtcVideoPerfDb
    : public leveldb_proto::test::FakeDB<WebrtcVideoStatsEntryProto> {
 public:
  explicit InMemoryWebrtcVideoPerfDb(EntryMap* db) : FakeDB(db) {}

  // Partial ProtoDatabase implementation.
  void Init(leveldb_proto::Callbacks::InitStatusCallback callback) override {
    FakeDB::Init(std::move(callback));
    InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  }

  void GetEntry(
      const std::string& key,
      typename leveldb_proto::Callbacks::Internal<
          WebrtcVideoStatsEntryProto>::GetCallback callback) override {
    FakeDB::GetEntry(key, std::move(callback));
    // Run callback.
    GetCallback(true);
  }

  void LoadKeysAndEntriesWhile(
      const std::string& start,
      const leveldb_proto::KeyIteratorController& controller,
      typename leveldb_proto::Callbacks::Internal<WebrtcVideoStatsEntryProto>::
          LoadKeysAndEntriesCallback callback) override {
    FakeDB::LoadKeysAndEntriesWhile(start, controller, std::move(callback));
    // Run callback.
    LoadCallback(true);
  }

  void UpdateEntries(
      std::unique_ptr<typename ProtoDatabase<
          WebrtcVideoStatsEntryProto>::KeyEntryVector> entries_to_save,
      std::unique_ptr<std::vector<std::string>> keys_to_remove,
      leveldb_proto::Callbacks::UpdateCallback callback) override {
    FakeDB::UpdateEntries(std::move(entries_to_save), std::move(keys_to_remove),
                          std::move(callback));
    // Run callback.
    UpdateCallback(true);
  }

  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<typename leveldb_proto::Util::Internal<
          WebrtcVideoStatsEntryProto>::KeyEntryVector> entries_to_save,
      const leveldb_proto::KeyFilter& filter,
      leveldb_proto::Callbacks::UpdateCallback callback) override {
    FakeDB::UpdateEntriesWithRemoveFilter(std::move(entries_to_save), filter,
                                          std::move(callback));
    // Run callback.
    UpdateCallback(true);
  }
};

class WebrtcVideoPerfLPMFuzzer {
 public:
  WebrtcVideoPerfLPMFuzzer(
      const fuzzing::webrtc_video_perf::proto::Testcase& testcase)
      : testcase_(testcase) {
    // Create all objects that are needed and connect everything.
    in_memory_db_ = new InMemoryWebrtcVideoPerfDb(&in_memory_db_map_);
    std::unique_ptr<WebrtcVideoStatsDBImpl> stats_db =
        WebrtcVideoPerfLPMFuzzerHelper::CreateWebrtcVideoStatsDbImpl(
            std::unique_ptr<InMemoryWebrtcVideoPerfDb>(in_memory_db_));
    perf_history_ =
        std::make_unique<WebrtcVideoPerfHistory>(std::move(stats_db));
    perf_recorder_ = std::make_unique<WebrtcVideoPerfRecorder>(
        perf_history_->GetSaveCallback());
  }

  void NextAction() {
    const auto& action = testcase_->actions(action_index_);
    switch (action.action_case()) {
      case fuzzing::webrtc_video_perf::proto::Action::kUpdateRecord: {
        const auto& update_record = action.update_record();
        auto features_ptr = media::mojom::WebrtcPredictionFeatures::New();
        auto video_stats_ptr = media::mojom::WebrtcVideoStats::New();
        mojolpm::FromProto(update_record.features(), features_ptr);
        mojolpm::FromProto(update_record.video_stats(), video_stats_ptr);
        perf_recorder_->UpdateRecord(std::move(features_ptr),
                                     std::move(video_stats_ptr));
        break;
      }
      case fuzzing::webrtc_video_perf::proto::Action::kGetPerfInfo: {
        const auto& get_perf_info = action.get_perf_info();
        auto features_ptr = media::mojom::WebrtcPredictionFeatures::New();
        mojolpm::FromProto(get_perf_info.features(), features_ptr);
        perf_history_->GetPerfInfo(std::move(features_ptr),
                                   get_perf_info.frames_per_second(),
                                   base::DoNothing());
        break;
      }
      default: {
        // Do nothing.
      }
    }
    ++action_index_;
  }

  bool IsFinished() { return action_index_ >= testcase_->actions_size(); }

 private:
  const raw_ref<const fuzzing::webrtc_video_perf::proto::Testcase> testcase_;
  int action_index_ = 0;

  // Database storage.
  InMemoryWebrtcVideoPerfDb::EntryMap in_memory_db_map_;
  // Proto buffer database implementation that uses `in_memory_db_map_` as
  // storage.
  raw_ptr<InMemoryWebrtcVideoPerfDb> in_memory_db_;
  std::unique_ptr<WebrtcVideoPerfHistory> perf_history_;
  std::unique_ptr<WebrtcVideoPerfRecorder> perf_recorder_;
};

void NextAction(WebrtcVideoPerfLPMFuzzer* testcase,
                base::OnceClosure fuzzer_run_loop) {
  if (!testcase->IsFinished()) {
    testcase->NextAction();
    GetFuzzerTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                  std::move(fuzzer_run_loop)));
  } else {
    std::move(fuzzer_run_loop).Run();
  }
}

void RunTestcase(WebrtcVideoPerfLPMFuzzer* testcase) {
  base::RunLoop fuzzer_run_loop;
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                fuzzer_run_loop.QuitClosure()));
  // Make sure that all callbacks have completed.
  constexpr base::TimeDelta kTimeout = base::Seconds(5);
  GetEnvironment().FastForwardBy(kTimeout);
  fuzzer_run_loop.Run();
}

}  // namespace

DEFINE_BINARY_PROTO_FUZZER(
    const fuzzing::webrtc_video_perf::proto::Testcase& testcase) {
  if (!testcase.actions_size()) {
    return;
  }

  WebrtcVideoPerfLPMFuzzer webtc_video_perf_fuzzer_instance(testcase);
  base::RunLoop main_run_loop;

  GetFuzzerTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(RunTestcase,
                     base::Unretained(&webtc_video_perf_fuzzer_instance)),
      main_run_loop.QuitClosure());
  main_run_loop.Run();
}

}  // namespace media
