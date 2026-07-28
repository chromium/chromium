// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MojoLPM fuzzer for the media.mojom.VideoEncodeAccelerator interface. A real
// MojoVideoEncodeAcceleratorService is bound into a live mojo::Remote and driven
// via mojolpm::HandleRemoteAction, so every call goes through the Mojo
// serialization path the same way a renderer reaches the GPU process.
//
// MojoVideoEncodeAcceleratorService is the GPU-process endpoint for hardware
// video encode offload, constructed by GpuVideoEncodeAcceleratorFactory and fed
// by renderer processes, so a compromised renderer controls every argument.
// Initialize() takes a VideoEncodeAcceleratorConfig (pixel format, profile,
// coded_size, bitrate, spatial layers). Encode() takes a VideoFrame whose plane
// layout (offsets[]/strides[]) and shared-memory/GpuMemoryBuffer backing are
// rebuilt by VideoFrameStructTraits::Read; this deserialize path is the hot
// untrusted target and has no existing fuzzer. UseOutputBitstreamBuffer() takes
// an int32 id plus an UnsafeSharedMemoryRegion, and
// RequestEncodingParametersChange* takes a VideoBitrateAllocation or Bitrate.
//
// Each NewVideoEncodeAccelerator action binds a fresh service whose
// create_vea_callback runs CreateAndInitializeFakeVEA, the same fake-encoder
// path the service unittest uses, so Initialize() stands up a working
// FakeVideoEncodeAccelerator backend with no GPU/GL/command-buffer bring-up and
// Encode() reaches the real VideoFrame deserialize and bitstream-buffer
// handling. The service is registered in the MojoLPM context under the testcase
// id; the generated RemoteAction then drives Initialize, Encode,
// UseOutputBitstreamBuffer, RequestEncodingParametersChange*, IsFlushSupported,
// and Flush over the live pipe. MojoLPM synthesizes the VEAClient remote, the
// MediaLog remote, and the UnsafeSharedMemoryRegion handle. run_thread actions
// pump the loop so async Encode completions and FakeVideoEncodeAccelerator reply
// tasks drain between actions.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/encoder_status.h"
#include "media/base/media_log.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom-mojolpm.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/mojo/services/mojo_video_encode_accelerator_service.h"
#include "media/video/fake_video_encode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/cpp/test_support/validation_errors_test_util.h"
#include "mojo/public/tools/fuzzers/mojolpm.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

#include "media/mojo/services/mojo_video_encode_accelerator_service_mojolpm_fuzzer.pb.h"

namespace {

namespace proto = media::fuzzing::video_encode_accelerator::proto;

// Factory the service calls from Initialize(): stands up a real
// FakeVideoEncodeAccelerator (no GPU bring-up) and runs its synchronous
// Initialize so the service receives a working backend, the same fake-encoder
// path the service unittest uses. `will_succeed` is bound to true so the
// service's Initialize() reaches the kOk path and Encode() can flow into the
// real VideoFrame deserialize.
media::EncoderStatus::Or<std::unique_ptr<media::VideoEncodeAccelerator>>
CreateAndInitializeFakeVEA(
    bool will_succeed,
    const media::VideoEncodeAccelerator::Config& config,
    media::VideoEncodeAccelerator::Client* client,
    const gpu::GpuPreferences& /*gpu_preferences*/,
    const gpu::GpuDriverBugWorkarounds& /*gpu_workarounds*/,
    const gpu::GPUInfo::GPUDevice& /*gpu_device*/,
    std::unique_ptr<media::MediaLog> media_log,
    media::MojoVideoEncodeAcceleratorService::GetCommandBufferHelperCB
    /*get_command_buffer_helper_cb*/,
    scoped_refptr<base::SingleThreadTaskRunner> /*gpu_task_runner*/) {
  auto vea = std::make_unique<media::FakeVideoEncodeAccelerator>(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  vea->SetWillInitializationSucceed(will_succeed);
  const media::EncoderStatus result =
      vea->Initialize(config, client, media_log->Clone());
  if (result.is_ok()) {
    return base::WrapUnique<media::VideoEncodeAccelerator>(vea.release());
  }
  return std::move(result);
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

// Process-global fuzzer state, established once.
struct FuzzerEnvironment {
  FuzzerEnvironment() {
    mojo::core::Init();
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    // GURL/IDN-host bytes can be reached via the deserialize graph; UTS46 data
    // must be present or an IDN host NOTREACHEDs.
    CHECK(base::i18n::InitializeICU());
  }

  // base::AtExitManager MUST be the first member and live for the whole process;
  // anything reaching a base/ at-exit registration CHECKs without it.
  base::AtExitManager at_exit_manager;

  // TestTimeouts::Initialize() MUST run before the TaskEnvironment member is
  // constructed (its ctor DCHECKs TestTimeouts is initialized). A member ctor
  // runs before the enclosing ctor body, so do it in a lambda-init member that
  // is declared *before* task_environment.
  const bool initialized_ = [] {
    base::CommandLine::Init(0, nullptr);
    TestTimeouts::Initialize();
    return true;
  }();

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // VideoFrame (over GpuMemoryBuffer/Mailbox data) and UnsafeSharedMemoryRegion
  // serialize fixed-size arrays/tokens; without this, mojolpm over those fields
  // CHECK-storms on serialization warnings. ASAN still catches real bugs.
  mojo::internal::SerializationWarningObserverForTesting
      serialization_warning_observer;
};

class VideoEncodeAcceleratorTestcase
    : public mojolpm::Testcase<proto::Testcase, proto::Action> {
 public:
  using ProtoTestcase = proto::Testcase;
  using ProtoAction = proto::Action;

  explicit VideoEncodeAcceleratorTestcase(const ProtoTestcase& testcase)
      : mojolpm::Testcase<ProtoTestcase, ProtoAction>(testcase) {
    // Created on the main thread; actions run on the fuzzer sequence.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  void SetUp(base::OnceClosure done_closure) override {
    mojolpm::GetContext()->StartTestcase();
    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
  }

  void TearDown(base::OnceClosure done_closure) override {
    // Drop all live receivers (and the services they own) before ending the
    // testcase so any queued reply/encode-completion tasks are cancelled
    // cleanly.
    receivers_.Clear();
    mojolpm::GetContext()->EndTestcase();
    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
  }

  void RunAction(const ProtoAction& action,
                 base::OnceClosure run_closure) override {
    switch (action.action_case()) {
      case ProtoAction::kNewVideoEncodeAccelerator:
        AddVideoEncodeAccelerator(action.new_video_encode_accelerator().id());
        break;

      case ProtoAction::kRunThread:
        // Let posted tasks (BindPostTaskToCurrentDefault Encode completion,
        // FakeVEA replies) drain before continuing. PostTaskAndReply preserves
        // ordering.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(run_closure));
        return;

      case ProtoAction::kVideoEncodeAcceleratorRemoteAction:
        mojolpm::HandleRemoteAction(
            action.video_encode_accelerator_remote_action());
        break;

      case ProtoAction::ACTION_NOT_SET:
        break;
    }

    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
  }

 private:
  // Binds a fresh MojoVideoEncodeAcceleratorService (over a real
  // FakeVideoEncodeAccelerator backend) into the receiver set and registers the
  // corresponding remote in the MojoLPM context under `id`, so subsequent
  // RemoteActions that reference `id` drive this live service over a real pipe.
  void AddVideoEncodeAccelerator(uint32_t id) {
    auto service = std::make_unique<media::MojoVideoEncodeAcceleratorService>(
        base::BindOnce(&CreateAndInitializeFakeVEA,
                       /*will_succeed=*/true),
        gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds(),
        gpu::GPUInfo::GPUDevice(),
        media::MojoVideoEncodeAcceleratorService::GetCommandBufferHelperCB(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    mojo::Remote<media::mojom::VideoEncodeAccelerator> remote;
    auto receiver = remote.BindNewPipeAndPassReceiver();
    receivers_.Add(std::move(service), std::move(receiver));

    mojolpm::GetContext()->AddInstance(id, std::move(remote));
  }

  mojo::UniqueReceiverSet<media::mojom::VideoEncodeAccelerator> receivers_;
};

}  // namespace

DEFINE_BINARY_PROTO_FUZZER(const proto::Testcase& testcase) {
  // The MojoLPM Testcase runner needs at least one action and one
  // sequence/sequence-index to schedule anything.
  if (!testcase.actions_size() || !testcase.sequences_size() ||
      !testcase.sequence_indexes_size()) {
    return;
  }

  static base::NoDestructor<FuzzerEnvironment> env;

  VideoEncodeAcceleratorTestcase testcase_runner(testcase);

  base::RunLoop main_run_loop;
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<VideoEncodeAcceleratorTestcase>,
                     base::Unretained(&testcase_runner), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));
  main_run_loop.Run();
}
