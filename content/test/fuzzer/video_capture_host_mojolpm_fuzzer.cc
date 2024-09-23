// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/test/test_future.h"
#include "base/threading/thread.h"
#include "content/browser/media/media_devices_util.h"  // nogncheck
#include "content/browser/media/media_internals.h"     // nogncheck
#include "content/browser/renderer_host/media/fake_video_capture_provider.h"  // nogncheck
#include "content/browser/renderer_host/media/in_process_video_capture_provider.h"  // nogncheck
#include "content/browser/renderer_host/media/media_stream_manager.h"  // nogncheck
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"  // nogncheck
#include "content/browser/renderer_host/media/service_video_capture_provider.h"  // nogncheck
#include "content/browser/renderer_host/media/video_capture_host.h"  // nogncheck
#include "content/browser/renderer_host/media/video_capture_manager.h"  // nogncheck
#include "content/browser/renderer_host/media/video_capture_provider_switcher.h"  // nogncheck
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "content/test/fuzzer/video_capture_host_mojolpm_fuzzer.pb.h"
#include "content/test/test_content_browser_client.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/capture/mojom/video_capture.mojom-mojolpm.h"
#include "media/capture/video/create_video_capture_device_factory.h"
#include "media/capture/video/linux/fake_device_provider.h"
#include "media/capture/video/linux/fake_v4l2_impl.h"
#include "media/capture/video/linux/video_capture_device_factory_v4l2.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

const char* kCmdline[] = {"video_capture_host_mojolpm_fuzzer", nullptr};

// Describe all the devices (as descriptors).
const uint32_t kNumDeviceDescriptors = 4;
const media::VideoCaptureDeviceDescriptors kDeviceDescriptors{
    {"dev_name_1", "dev_id_1"},
    {"dev_name_2", "dev_id_2"},
    {"dev_name_3", "dev_id_3"},
    {"dev_name_4", "dev_id_4"}};
// Specifies number of render process ids (counted from 0).
// All devices are opened for each id.
const uint32_t kNumRenderProcessIds = 2;

using blink::mojom::MediaDeviceType;

content::mojolpm::FuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<content::mojolpm::FuzzerEnvironment> environment(
      1, kCmdline);
  return *environment;
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().fuzzer_task_runner();
}

// Per-testcase state needed to run the interface being tested.
//
// The lifetime of this is scoped to a single testcase, and it is created and
// destroyed from the fuzzer sequence.
//
// The component under fuzz `VideoCaptureHost` is created on call
// `AddVideoCaptureHost` (singly bound using a self owned receiver).
// This directly relies on `MediaStreamManager`, which is owned here (as well as
// indirectly reliant on other components owned here).
//
// `MediaStreamManager` is a `CurrentThread::DestructionObserver`, so it must
// outlive the `BrowserTaskEnvironment`. So the task environment is scoped per
// testcase (undesirable for performance).
class VideoCaptureHostTestcase {
 public:
  VideoCaptureHostTestcase(
      const content::fuzzing::video_capture_host::proto::Testcase& testcase);
  ~VideoCaptureHostTestcase();

  // Returns true once either all of the actions in the testcase have been
  // performed, or the per-testcase action limit has been exceeded.
  //
  // This should only be called from the fuzzer sequence.
  bool IsFinished();

  // If there are still actions remaining in the testcase, this will perform the
  // next sequence of actions before returning.
  //
  // If IsFinished() would return true, then calling this function is a no-op.
  //
  // This should only be called from the fuzzer sequence.
  void NextAction();

 private:
  using Action = content::fuzzing::video_capture_host::proto::Action;

  void SetUp();
  void SetUpOnIOThreadFirst();
  void SetUpOnUIThread();
  void SetUpOnIOThreadSecond();

  // We want to open video sessions, to enable behaviour for the fuzzer.
  // To do this, we enumerate with the `MediaDeviceManager` (on UI thread),
  // then `OpenDevice` with the `MediaStreamManager` (on IO thread).
  // `OpenSession` is repeated for each `render_process_id`.
  // So the same devices are opened for multiple ids.
  void OpenSession(int render_process_id,
                   int render_frame_id,
                   int requester_id,
                   int page_request_id);
  void OpenSessionOnUIThread(
      int render_process_id,
      int render_frame_id,
      content::MediaDeviceSaltAndOrigin* salt_and_origin);
  void OpenSessionOnIOThread(
      int render_process_id,
      int render_frame_id,
      int requester_id,
      int page_request_id,
      const content::MediaDeviceSaltAndOrigin& salt_and_origin,
      base::OnceClosure quit_closure);

  void TearDown();
  void TearDownOnIOThread();
  void TearDownOnUIThread();

  std::unique_ptr<content::FakeMediaStreamUIProxy> CreateFakeUI();

  // A callback to receive the enumerated devices in the
  // `WebMediaDeviceInfoArray`.
  void VideoInputDevicesEnumerated(
      base::OnceClosure quit_closure,
      const std::string& salt,
      const url::Origin& security_origin,
      blink::WebMediaDeviceInfoArray* out,
      const content::MediaDeviceEnumeration& enumeration);

  // A callback which confirms opening device success. This provides the
  // session ids for the devices, stored in `opened_session_ids_`.
  void OnDeviceOpened(base::OnceClosure quit_closure,
                      int render_process_id,
                      uint32_t device_index,
                      bool success,
                      const std::string& label,
                      const blink::MediaStreamDevice& opened_device);

  // Create and bind a new instance for fuzzing. This needs to make sure that
  // the new instance has been created and bound on the correct sequence
  // before returning.
  void AddVideoCaptureHost(uint32_t id,
                           uint32_t render_process_id,
                           uint32_t routing_id);

  // This wraps `HandleRemoteAction`, making the call for the correct device.
  // As it requires specifying the `render_process_id` and `device_index`.
  // to find the correct session id for the device.
  void HandleDeviceRemoteAction(
      const content::fuzzing::video_capture_host::proto::
          VideoCaptureHostDeviceRemoteAction& device_remote_action);

  // Used to directly inject the session id into the `RemoteAction`,
  // overwriting the protobuf field.
  using RemoteAction = mojolpm::media::mojom::VideoCaptureHost_RemoteAction;
  const RemoteAction& RemoteActionInjectSessionId(
      const RemoteAction& remote_method_action,
      const ::base::UnguessableToken& input);

  // We register, and therefore use, the devices per `render_process_id`.
  // So this gets the appropriate session id.
  const base::UnguessableToken& OpenedSessionId(int render_process_id,
                                                uint32_t device_index);

  // The proto message describing the test actions to perform.
  const raw_ref<const content::fuzzing::video_capture_host::proto::Testcase>
      testcase_;

  // Apply a reasonable upper-bound on testcase complexity to avoid timeouts.
  const int max_action_count_ = 512;

  // Apply a reasonable upper-bound on maximum size of action that we will
  // deserialize. (This is deliberately slightly larger than max mojo message
  // size)
  const size_t max_action_size_ = 300 * 1024 * 1024;

  // Count of total actions performed in this testcase.
  int action_count_ = 0;

  // The index of the next sequence of actions to execute.
  int next_sequence_idx_ = 0;

  // Prerequisite components for the `VideoCaptureHost`.
  std::unique_ptr<content::MediaStreamManager> media_stream_manager_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;

  // Indexed by `render_process_id` then `kDeviceDescriptors` index
  // See `OpenedSessionId` getter.
  std::array<std::array<base::UnguessableToken, kNumDeviceDescriptors>,
             kNumRenderProcessIds>
      opened_session_ids_;

  // Prerequisite state.
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  content::TestContentBrowserClient browser_client_;

  SEQUENCE_CHECKER(sequence_checker_);
};

VideoCaptureHostTestcase::VideoCaptureHostTestcase(
    const content::fuzzing::video_capture_host::proto::Testcase& testcase)
    : testcase_(testcase),
      task_environment_(
          base::test::TaskEnvironment::MainThreadType::DEFAULT,
          base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
          base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
          content::BrowserTaskEnvironment::REAL_IO_THREAD),
      browser_context_(),
      browser_client_() {
  // VideoCaptureHostTestcase is created on the main thread, but the actions
  // that we want to validate the sequencing of take place on the fuzzer
  // sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
  SetUp();
  for (uint32_t render_process_id = 0; render_process_id < kNumRenderProcessIds;
       render_process_id++)
    OpenSession(render_process_id,
                /*render_frame_id=*/1,
                /*requester_id=*/1,
                /*page_request_id=*/1);
}

VideoCaptureHostTestcase::~VideoCaptureHostTestcase() {
  TearDown();
}

bool VideoCaptureHostTestcase::IsFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return next_sequence_idx_ >= testcase_->sequence_indexes_size();
}

void VideoCaptureHostTestcase::NextAction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (next_sequence_idx_ < testcase_->sequence_indexes_size()) {
    auto sequence_idx = testcase_->sequence_indexes(next_sequence_idx_++);
    const auto& sequence =
        testcase_->sequences(sequence_idx % testcase_->sequences_size());
    for (auto action_idx : sequence.action_indexes()) {
      if (!testcase_->actions_size() || ++action_count_ > max_action_count_) {
        return;
      }
      const auto& action =
          testcase_->actions(action_idx % testcase_->actions_size());
      if (action.ByteSizeLong() > max_action_size_) {
        return;
      }
      switch (action.action_case()) {
        case Action::kRunThread: {
          if (action.run_thread().id()) {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            content::GetUIThreadTaskRunner({})->PostTask(
                FROM_HERE, run_loop.QuitClosure());
            run_loop.Run();
          } else {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            content::GetIOThreadTaskRunner({})->PostTask(
                FROM_HERE, run_loop.QuitClosure());
            run_loop.Run();
          }
        } break;

        case Action::kNewVideoCaptureHost: {
          AddVideoCaptureHost(
              action.new_video_capture_host().id(),
              action.new_video_capture_host().render_process_id(),
              action.new_video_capture_host().routing_id());
        } break;

        case Action::kVideoCaptureHostDeviceRemoteAction: {
          HandleDeviceRemoteAction(
              action.video_capture_host_device_remote_action());
        } break;

        case Action::kVideoCaptureObserverReceiverAction: {
          mojolpm::HandleReceiverAction(
              action.video_capture_observer_receiver_action());
        } break;

        case Action::ACTION_NOT_SET:
          break;
      }
    }
  }
}

void VideoCaptureHostTestcase::SetUp() {
  {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&VideoCaptureHostTestcase::SetUpOnIOThreadFirst,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }
  {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    content::GetUIThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&VideoCaptureHostTestcase::SetUpOnUIThread,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }
  {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&VideoCaptureHostTestcase::SetUpOnIOThreadSecond,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }
}

void VideoCaptureHostTestcase::SetUpOnIOThreadFirst() {
  audio_manager_ = std::make_unique<media::MockAudioManager>(
      std::make_unique<media::TestAudioThread>());
  audio_system_ =
      std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
}

void VideoCaptureHostTestcase::SetUpOnUIThread() {
  // Here we specify the devices described by `kDeviceDescriptors`.
  // Which tells the `VideoCaptureDeviceFactoryV4L2` what devices we have.
  // This factory is then used to setup the `MediaStreamManager`.
  std::unique_ptr<media::FakeDeviceProvider> fake_device_provider =
      std::make_unique<media::FakeDeviceProvider>();
  scoped_refptr<media::FakeV4L2Impl> fake_v4l2_impl =
      base::MakeRefCounted<media::FakeV4L2Impl>();

  for (const auto& descriptor : kDeviceDescriptors) {
    // Note, despite the param name, `device_name` should match `device_id`
    fake_v4l2_impl->AddDevice(/*device_name=*/descriptor.device_id,
                              media::FakeV4L2DeviceConfig(descriptor));
    fake_device_provider->AddDevice(descriptor);
  }

  std::unique_ptr<media::VideoCaptureDeviceFactoryV4L2>
      video_capture_device_factory =
          std::make_unique<media::VideoCaptureDeviceFactoryV4L2>(
              task_environment_.GetMainThreadTaskRunner());

  video_capture_device_factory->SetV4L2EnvironmentForTesting(
      std::move(fake_v4l2_impl), std::move(fake_device_provider));

  // Ensure MediaInternals is created on the UI thread before starting the
  // MediaStreamManager instance.
  content::MediaInternals::GetInstance();

  auto fake_video_capture_provider =
      std::make_unique<content::FakeVideoCaptureProvider>(
          std::move(video_capture_device_factory));
  auto screencapture_video_capture_provider =
      content::InProcessVideoCaptureProvider::CreateInstanceForScreenCapture(
          base::SingleThreadTaskRunner::GetCurrentDefault());

  media_stream_manager_ = std::make_unique<content::MediaStreamManager>(
      audio_system_.get(),
      std::make_unique<content::VideoCaptureProviderSwitcher>(
          std::move(fake_video_capture_provider),
          std::move(screencapture_video_capture_provider)));
}

void VideoCaptureHostTestcase::SetUpOnIOThreadSecond() {
  media_stream_manager_->UseFakeUIFactoryForTests(base::BindRepeating(
      &VideoCaptureHostTestcase::CreateFakeUI, base::Unretained(this)));
}

void VideoCaptureHostTestcase::OpenSession(int render_process_id,
                                           int render_frame_id,
                                           int requester_id,
                                           int page_request_id) {
  // We get `salt_and_origin` on the UI Thread, and use it on the IO thread.
  content::MediaDeviceSaltAndOrigin salt_and_origin =
      content::MediaDeviceSaltAndOrigin::Empty();
  {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    content::GetUIThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&VideoCaptureHostTestcase::OpenSessionOnUIThread,
                       base::Unretained(this), render_process_id,
                       render_frame_id, base::Unretained(&salt_and_origin)),
        run_loop.QuitClosure());
    run_loop.Run();
  }
  {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoCaptureHostTestcase::OpenSessionOnIOThread,
                       base::Unretained(this), render_process_id,
                       render_frame_id, requester_id, page_request_id,
                       salt_and_origin, run_loop.QuitClosure()));
    run_loop.Run();
  }
}

void VideoCaptureHostTestcase::OpenSessionOnUIThread(
    int render_process_id,
    int render_frame_id,
    content::MediaDeviceSaltAndOrigin* out_salt_and_origin) {
  base::test::TestFuture<const content::MediaDeviceSaltAndOrigin&> future;
  content::GetMediaDeviceSaltAndOrigin(
      content::GlobalRenderFrameHostId(render_process_id, render_frame_id),
      future.GetCallback());
  *out_salt_and_origin = future.Get();
}

void VideoCaptureHostTestcase::OpenSessionOnIOThread(
    int render_process_id,
    int render_frame_id,
    int requester_id,
    int page_request_id,
    const content::MediaDeviceSaltAndOrigin& salt_and_origin,
    base::OnceClosure quit_closure) {
  // Enumerate video devices.
  blink::WebMediaDeviceInfoArray video_devices;
  {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    content::MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
    devices_to_enumerate[static_cast<size_t>(
        MediaDeviceType::kMediaVideoInput)] = true;
    media_stream_manager_->media_devices_manager()->EnumerateDevices(
        devices_to_enumerate,
        base::BindOnce(&VideoCaptureHostTestcase::VideoInputDevicesEnumerated,
                       base::Unretained(this), run_loop.QuitClosure(),
                       salt_and_origin.device_id_salt(),
                       salt_and_origin.origin(), &video_devices));

    run_loop.Run();
  }

  // Open video devices.
  for (uint32_t device_index = 0; device_index < video_devices.size();
       device_index++) {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    media_stream_manager_->OpenDevice(
        {render_process_id, render_frame_id}, requester_id, page_request_id,
        video_devices[device_index].device_id,
        blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, salt_and_origin,
        base::BindOnce(&VideoCaptureHostTestcase::OnDeviceOpened,
                       base::Unretained(this), run_loop.QuitClosure(),
                       render_process_id, device_index),
        content::MediaStreamManager::DeviceStoppedCallback());
    run_loop.Run();
  }

  std::move(quit_closure).Run();
}

void VideoCaptureHostTestcase::TearDown() {
  {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&VideoCaptureHostTestcase::TearDownOnIOThread,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }
  {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    content::GetUIThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&VideoCaptureHostTestcase::TearDownOnUIThread,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }
}

void VideoCaptureHostTestcase::TearDownOnIOThread() {
  audio_manager_->Shutdown();
  audio_manager_.reset();
}

void VideoCaptureHostTestcase::TearDownOnUIThread() {
  audio_system_.reset();
}

std::unique_ptr<content::FakeMediaStreamUIProxy>
VideoCaptureHostTestcase::CreateFakeUI() {
  return std::make_unique<content::FakeMediaStreamUIProxy>(
      /*tests_use_fake_render_frame_hosts=*/true);
}

void VideoCaptureHostTestcase::VideoInputDevicesEnumerated(
    base::OnceClosure quit_closure,
    const std::string& salt,
    const url::Origin& security_origin,
    blink::WebMediaDeviceInfoArray* out,
    const content::MediaDeviceEnumeration& enumeration) {
  for (const auto& info :
       enumeration[static_cast<size_t>(MediaDeviceType::kMediaVideoInput)]) {
    std::string device_id =
        content::GetHMACForMediaDeviceID(salt, security_origin, info.device_id);
    out->push_back(
        blink::WebMediaDeviceInfo(device_id, info.label, std::string()));
  }
  std::move(quit_closure).Run();
}

void VideoCaptureHostTestcase::OnDeviceOpened(
    base::OnceClosure quit_closure,
    int render_process_id,
    uint32_t device_index,
    bool success,
    const std::string& label,
    const blink::MediaStreamDevice& opened_device) {
  if (success) {
    opened_session_ids_[render_process_id][device_index] =
        opened_device.session_id();
  }
  std::move(quit_closure).Run();
}

void VideoCaptureHostTestcase::AddVideoCaptureHost(uint32_t id,
                                                   uint32_t render_process_id,
                                                   uint32_t routing_id) {
  mojo::Remote<::media::mojom::VideoCaptureHost> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &content::VideoCaptureHost::Create,
          content::GlobalRenderFrameHostId(render_process_id, routing_id),
          media_stream_manager_.get(), std::move(receiver)),
      run_loop.QuitClosure());
  run_loop.Run();

  mojolpm::GetContext()->AddInstance(id, std::move(remote));
}

/* Matches signature of a mojolpm::ToProto implementation, which has not been
 * implemented yet, to be replaced once that is done.
 * see "mojo/public/mojom/base/unguessable_token.mojom-mojolpm.h"
 */
bool ToProto(const ::base::UnguessableToken& input,
             mojolpm::mojo_base::mojom::UnguessableToken& output) {
  if (output.has_new_()) {
    output.mutable_new_()->set_id(1UL);
    output.mutable_new_()->set_m_low(input.GetLowForSerialization());
    output.mutable_new_()->set_m_high(input.GetHighForSerialization());
    return true;
  }
  auto allocated_new = std::make_unique<
      mojolpm::mojo_base::mojom::UnguessableToken_ProtoStruct>();

  allocated_new->set_id(1UL);
  allocated_new->set_m_low(input.GetLowForSerialization());
  allocated_new->set_m_high(input.GetHighForSerialization());

  // passes ownership of `allocated_new`
  output.set_allocated_new_(allocated_new.release());
  return true;
}

void VideoCaptureHostTestcase::HandleDeviceRemoteAction(
    const content::fuzzing::video_capture_host::proto::
        VideoCaptureHostDeviceRemoteAction& device_remote_action) {
  const base::UnguessableToken& token =
      OpenedSessionId(device_remote_action.render_process_id(),
                      device_remote_action.device_index());

  mojolpm::HandleRemoteAction(
      RemoteActionInjectSessionId(device_remote_action.remote_action(), token));
}

const VideoCaptureHostTestcase::RemoteAction&
VideoCaptureHostTestcase::RemoteActionInjectSessionId(
    const RemoteAction& remote_method_action,
    const ::base::UnguessableToken& token) {
  // `const_cast` used for performance (could also copy)
  RemoteAction& remote_method_action_mutable =
      const_cast<RemoteAction&>(remote_method_action);
  mojolpm::mojo_base::mojom::UnguessableToken* m_session_id;

  switch (remote_method_action_mutable.method_case()) {
    case RemoteAction::kMStart:
      m_session_id = remote_method_action_mutable.mutable_m_start()
                         ->mutable_m_session_id();
      break;

    case RemoteAction::kMResume:
      m_session_id = remote_method_action_mutable.mutable_m_resume()
                         ->mutable_m_session_id();
      break;

    case RemoteAction::kMGetDeviceSupportedFormats:
      m_session_id =
          remote_method_action_mutable.mutable_m_get_device_supported_formats()
              ->mutable_m_session_id();
      break;

    case RemoteAction::kMGetDeviceFormatsInUse:
      m_session_id =
          remote_method_action_mutable.mutable_m_get_device_formats_in_use()
              ->mutable_m_session_id();
      break;

    default:
      return remote_method_action;
  }

  if (!ToProto(token, *m_session_id))
    return remote_method_action;
  return remote_method_action_mutable;
}

const base::UnguessableToken& VideoCaptureHostTestcase::OpenedSessionId(
    int render_process_id,
    uint32_t device_index) {
  return opened_session_ids_[render_process_id % kNumRenderProcessIds]
                            [device_index % kNumDeviceDescriptors];
}

// Helper function to keep scheduling fuzzer actions on the current runloop
// until the testcase has completed, and then quit the runloop.
void NextAction(VideoCaptureHostTestcase* testcase,
                base::RepeatingClosure quit_closure) {
  if (!testcase->IsFinished()) {
    testcase->NextAction();
    GetFuzzerTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                  std::move(quit_closure)));
  } else {
    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(quit_closure));
  }
}

// Helper function to setup and run the testcase, since we need to do that from
// the fuzzer sequence rather than the main thread.
void RunTestcase(VideoCaptureHostTestcase* testcase) {
  mojo::Message message;
  auto dispatch_context =
      std::make_unique<mojo::internal::MessageDispatchContext>(&message);

  mojolpm::GetContext()->StartTestcase();

  base::RunLoop fuzzer_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                fuzzer_run_loop.QuitClosure()));
  fuzzer_run_loop.Run();

  mojolpm::GetContext()->EndTestcase();
}

DEFINE_BINARY_PROTO_FUZZER(
    const content::fuzzing::video_capture_host::proto::Testcase&
        proto_testcase) {
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  // Make sure that the environment is initialized before we do anything else.
  GetEnvironment();

  VideoCaptureHostTestcase testcase(proto_testcase);

  base::RunLoop ui_run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  // Unretained is safe here, because ui_run_loop has to finish before testcase
  // goes out of scope.
  GetFuzzerTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(RunTestcase, base::Unretained(&testcase)),
      ui_run_loop.QuitClosure());

  ui_run_loop.Run();
}
