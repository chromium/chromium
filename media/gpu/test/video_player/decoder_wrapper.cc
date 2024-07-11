// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/decoder_wrapper.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/waiting.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video_bitstream.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"
#include "media/gpu/test/video_player/test_vda_video_decoder.h"
#include "media/gpu/test/video_test_helpers.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

namespace media {
namespace test {

namespace {
// Callbacks can be called from any thread, but WeakPtrs are not thread-safe.
// This helper thunk wraps a WeakPtr into an 'Optional' value, so the WeakPtr is
// only dereferenced after rescheduling the task on the specified task runner.
template <typename F, typename... Args>
void CallbackThunk(std::optional<base::WeakPtr<DecoderWrapper>> decoder_client,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   F f,
                   Args... args) {
  DCHECK(decoder_client);
  task_runner->PostTask(FROM_HERE, base::BindOnce(f, *decoder_client, args...));
}
}  // namespace

DecoderWrapper::DecoderWrapper(
    const DecoderListener::EventCallback& event_cb,
    std::unique_ptr<FrameRendererDummy> renderer,
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
    const DecoderWrapperConfig& config)
    : event_cb_(event_cb),
      frame_renderer_(std::move(renderer)),
      frame_processors_(std::move(frame_processors)),
      decoder_wrapper_config_(config),
      worker_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
           base::WithBaseSyncPrimitives()},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)),
      state_(DecoderWrapperState::kUninitialized) {
  DCHECK(event_cb_);
  DCHECK(frame_renderer_);
  DETACH_FROM_SEQUENCE(worker_sequence_checker_);

  weak_this_ = weak_this_factory_.GetWeakPtr();
}

DecoderWrapper::~DecoderWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  base::WaitableEvent done;
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DecoderWrapper::DestroyDecoderTask, weak_this_, &done));
  done.Wait();

  // Wait until the renderer and frame processors are done before destroying
  // them. This needs to be done after destroying the decoder so no new frames
  // will be queued while waiting.
  WaitForRenderer();
  WaitForFrameProcessors();
  frame_renderer_ = nullptr;
  frame_processors_.clear();
}

// static
std::unique_ptr<DecoderWrapper> DecoderWrapper::Create(
    const DecoderListener::EventCallback& event_cb,
    std::unique_ptr<FrameRendererDummy> frame_renderer,
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors,
    const DecoderWrapperConfig& config) {
  auto wrapper =
      base::WrapUnique(new DecoderWrapper(event_cb, std::move(frame_renderer),
                                          std::move(frame_processors), config));
  wrapper->CreateDecoder();
  return wrapper;
}

void DecoderWrapper::CreateDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  base::WaitableEvent done;
  worker_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DecoderWrapper::CreateDecoderTask, weak_this_, &done));
  done.Wait();
}

bool DecoderWrapper::WaitForFrameProcessors() {
  bool success = true;
  for (auto& frame_processor : frame_processors_)
    success &= frame_processor->WaitUntilDone();
  return success;
}

void DecoderWrapper::WaitForRenderer() {
  ASSERT_TRUE(frame_renderer_);
  frame_renderer_->WaitUntilRenderingDone();
}

void DecoderWrapper::Initialize(const VideoBitstream* video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);
  DCHECK(video);

  base::WaitableEvent done;
  worker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DecoderWrapper::InitializeTask, weak_this_,
                                video, &done));
  done.Wait();
}

void DecoderWrapper::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  worker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DecoderWrapper::PlayTask, weak_this_));
}

void DecoderWrapper::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  worker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DecoderWrapper::FlushTask, weak_this_));
}

void DecoderWrapper::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(parent_sequence_checker_);

  worker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DecoderWrapper::ResetTask, weak_this_));
}

void DecoderWrapper::CreateDecoderTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DCHECK_EQ(state_, DecoderWrapperState::kUninitialized);
  ASSERT_TRUE(!decoder_) << "Can't create decoder: already created";

  switch (decoder_wrapper_config_.implementation) {
    case DecoderImplementation::kVD:
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
      decoder_ = VideoDecoderPipeline::CreateForTesting(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          std::make_unique<NullMediaLog>(),
          decoder_wrapper_config_.ignore_resolution_changes_to_smaller_vp9);
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
      break;
    case DecoderImplementation::kVDA:
    case DecoderImplementation::kVDVDA:
      // The video decoder client expects decoders to use the VD interface. We
      // can use the TestVDAVideoDecoder wrapper here to test VDA-based video
      // decoders.
      decoder_ = std::make_unique<TestVDAVideoDecoder>(
          decoder_wrapper_config_.implementation ==
              DecoderImplementation::kVDVDA,
          // base::Unretained(this) is safe because |decoder_| is owned by
          // |*this|. The lifetime of |decoder_| must be shorter than |*this|.
          base::BindRepeating(&DecoderWrapper::OnResolutionChangedTask,
                              base::Unretained(this)),
          gfx::ColorSpace(), frame_renderer_.get(),
          decoder_wrapper_config_.linear_output);
      break;
  }

  done->Signal();
}

void DecoderWrapper::InitializeTask(const VideoBitstream* video,
                                    base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DCHECK(state_ == DecoderWrapperState::kUninitialized ||
         state_ == DecoderWrapperState::kIdle);
  ASSERT_TRUE(decoder_) << "Can't initialize decoder: not created yet";
  ASSERT_TRUE(video);

  encoded_data_helper_ =
      EncodedDataHelper::Create(video->Data(), video->Codec());

  // (Re-)initialize the decoder.
  VideoDecoderConfig config(
      video->Codec(), video->Profile(),
      VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
      kNoTransformation, video->Resolution(), gfx::Rect(video->Resolution()),
      video->Resolution(), std::vector<uint8_t>(0), EncryptionScheme());
  input_video_codec_ = video->Codec();
  input_video_profile_ = video->Profile();

  VideoDecoder::InitCB init_cb = base::BindOnce(
      CallbackThunk<decltype(&DecoderWrapper::OnDecoderInitializedTask),
                    DecoderStatus>,
      weak_this_, worker_task_runner_,
      &DecoderWrapper::OnDecoderInitializedTask);
  VideoDecoder::OutputCB output_cb = base::BindRepeating(
      CallbackThunk<decltype(&DecoderWrapper::OnFrameReadyTask),
                    scoped_refptr<VideoFrame>>,
      weak_this_, worker_task_runner_, &DecoderWrapper::OnFrameReadyTask);

  decoder_->Initialize(config, false, nullptr, std::move(init_cb), output_cb,
                       WaitingCB());

  DCHECK_LE(decoder_wrapper_config_.max_outstanding_decode_requests,
            static_cast<size_t>(decoder_->GetMaxDecodeRequests()));

  done->Signal();
}

void DecoderWrapper::DestroyDecoderTask(base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  LOG_IF(WARNING, 0u != num_outstanding_decode_requests_)
      << "There is/are " << num_outstanding_decode_requests_
      << " Decode() requests that have not been acknowledged by |decoder_|. "
         "This might be fine or a problem depending on whether the calling "
         "test needed to have processed the full input bitstream or not.";
  DVLOGF(4);

  // Invalidate all scheduled tasks.
  weak_this_factory_.InvalidateWeakPtrs();

  decoder_.reset();

  state_ = DecoderWrapperState::kUninitialized;
  done->Signal();
}

void DecoderWrapper::PlayTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DVLOGF(4);

  // This method should only be called when the decoder client is idle. If
  // called e.g. while flushing, the behavior is undefined.
  ASSERT_EQ(state_, DecoderWrapperState::kIdle);

  // Start decoding the first fragments. While in the decoding state new
  // fragments will automatically be fed to the decoder, when the decoder
  // notifies us it reached the end of a bitstream buffer.
  state_ = DecoderWrapperState::kDecoding;
  for (size_t i = 0;
       i < decoder_wrapper_config_.max_outstanding_decode_requests; ++i) {
    DecodeNextFragmentTask();
  }
}

void DecoderWrapper::DecodeNextFragmentTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DVLOGF(4);

  // Stop decoding fragments if we're no longer in the decoding state.
  if (state_ != DecoderWrapperState::kDecoding)
    return;

  // Flush immediately when we reached the end of the stream. This changes the
  // state to kFlushing so further decode tasks will be aborted.
  if (encoded_data_helper_->ReachEndOfStream()) {
    FlushTask();
    return;
  }

  scoped_refptr<DecoderBuffer> bitstream_buffer =
      encoded_data_helper_->GetNextBuffer();
  if (!bitstream_buffer) {
    LOG(ERROR) << "Failed to get next video stream data";
    return;
  }
  bitstream_buffer->set_timestamp(base::TimeTicks::Now().since_origin());

  bool has_config_info = false;
  if (input_video_codec_ == media::VideoCodec::kH264 ||
      input_video_codec_ == media::VideoCodec::kHEVC) {
    has_config_info = media::test::EncodedDataHelper::HasConfigInfo(
        bitstream_buffer->data(), bitstream_buffer->size(), input_video_codec_);
  }

  VideoDecoder::DecodeCB decode_cb = base::BindOnce(
      CallbackThunk<decltype(&DecoderWrapper::OnDecodeDoneTask), DecoderStatus>,
      weak_this_, worker_task_runner_, &DecoderWrapper::OnDecodeDoneTask);
  decoder_->Decode(std::move(bitstream_buffer), std::move(decode_cb));

  num_outstanding_decode_requests_++;

  // Throw event when we encounter a config info in a H.264/HEVC stream.
  if (has_config_info)
    FireEvent(DecoderListener::Event::kConfigInfo);
}

void DecoderWrapper::FlushTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DVLOGF(4);

  // Changing the state to flushing will abort any pending decodes.
  state_ = DecoderWrapperState::kFlushing;

  VideoDecoder::DecodeCB flush_done_cb = base::BindOnce(
      CallbackThunk<decltype(&DecoderWrapper::OnFlushDoneTask), DecoderStatus>,
      weak_this_, worker_task_runner_, &DecoderWrapper::OnFlushDoneTask);
  decoder_->Decode(DecoderBuffer::CreateEOSBuffer(), std::move(flush_done_cb));

  FireEvent(DecoderListener::Event::kFlushing);
}

void DecoderWrapper::ResetTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DVLOGF(4);

  // Changing the state to resetting will abort any pending decodes.
  state_ = DecoderWrapperState::kResetting;
  // TODO(dstaessens@) Allow resetting to any point in the stream.
  encoded_data_helper_->Rewind();

  decoder_->Reset(base::BindOnce(
      CallbackThunk<decltype(&DecoderWrapper::OnResetDoneTask)>, weak_this_,
      worker_task_runner_, &DecoderWrapper::OnResetDoneTask));
  FireEvent(DecoderListener::Event::kResetting);
}

void DecoderWrapper::OnDecoderInitializedTask(DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DCHECK(state_ == DecoderWrapperState::kUninitialized ||
         state_ == DecoderWrapperState::kIdle);

  if (!status.is_ok()) {
    state_ = DecoderWrapperState::kUninitialized;
    FireEvent(DecoderListener::Event::kFailure);
  } else {
    state_ = DecoderWrapperState::kIdle;
    FireEvent(DecoderListener::Event::kInitialized);
  }
}

void DecoderWrapper::OnDecodeDoneTask(DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DCHECK_NE(DecoderWrapperState::kIdle, state_);
  ASSERT_TRUE(status != DecoderStatus::Codes::kAborted ||
              state_ == DecoderWrapperState::kResetting);
  DVLOGF(4);

  num_outstanding_decode_requests_--;

  base::TimeDelta delay = base::Milliseconds(0);
  // Queue the next fragment to be decoded.
  // TODO(mcasas): Introduce a minor delay here to avoid overrunning the driver;
  // this is a provision for Mediatek devices and for the erroneous behaviour
  // of feeding more encoded chunk here (the driver has likely not seen any
  // encoded chunk enqueued at this point) and not in OnFrameReadyTask as it
  // should (naively moving this task there doesn't work because it prevents the
  // V4L2VideoDecoder backend from polling the device driver).
#if BUILDFLAG(USE_V4L2_CODEC)
  delay = base::Milliseconds(1);
  static bool log_delay_message = true;
  if (log_delay_message) {
    LOG(INFO) << "Using a delay of " << delay
              << " between sending encoded chunks to accommodate "
                 "MTK stateful V4L2 drivers";
    log_delay_message = false;
  }
#endif

  worker_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DecoderWrapper::DecodeNextFragmentTask, weak_this_),
      delay);
  FireEvent(DecoderListener::Event::kDecoderBufferAccepted);
}

void DecoderWrapper::OnFrameReadyTask(scoped_refptr<VideoFrame> video_frame) {
  DVLOGF(4) << current_frame_index_;
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DCHECK(video_frame->metadata().power_efficient);

  // Technically VideoDecoder clients shouldn't care about |video_frame|'s'
  // timestamps but we do because we feed non-zeros in DecodeNextFragmentTask().
  // Note that we cannot enforce non-strictly monotonically increasing time
  // deltas because the feeding order might not be the same as the output order
  // (e.g. in H.264 with B-frames the output order would be the "presentation"
  // order and not the "decode" or "transmission" order).
  DCHECK_NE(video_frame->timestamp(), base::TimeDelta());

  frame_renderer_->RenderFrame(video_frame);

  for (auto& frame_processor : frame_processors_)
    frame_processor->ProcessVideoFrame(video_frame, current_frame_index_);

  // Notify the test a frame has been decoded. We should only do this after
  // scheduling the frame to be processed, so calling WaitForFrameProcessors()
  // after receiving this event will always guarantee the frame to be processed.
  FireEvent(DecoderListener::Event::kFrameDecoded);

  current_frame_index_++;
}

void DecoderWrapper::OnFlushDoneTask(DecoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DCHECK_EQ(0u, num_outstanding_decode_requests_);

  // Send an EOS frame to the renderer, so it can reset any internal state it
  // might keep in preparation of the next stream of video frames.
  frame_renderer_->RenderFrame(VideoFrame::CreateEOSFrame());
  state_ = DecoderWrapperState::kIdle;
  FireEvent(DecoderListener::Event::kFlushDone);
}

void DecoderWrapper::OnResetDoneTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);
  DCHECK_EQ(0u, num_outstanding_decode_requests_);

  // We finished resetting to a different point in the stream, so we should
  // update the frame index. Currently only resetting to the start of the stream
  // is supported, so we can set the frame index to zero here.
  current_frame_index_ = 0;

  frame_renderer_->RenderFrame(VideoFrame::CreateEOSFrame());
  state_ = DecoderWrapperState::kIdle;
  FireEvent(DecoderListener::Event::kResetDone);
}

bool DecoderWrapper::OnResolutionChangedTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);

  return FireEvent(DecoderListener::Event::kNewBuffersRequested);
}

bool DecoderWrapper::FireEvent(DecoderListener::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_sequence_checker_);

  bool continue_decoding = event_cb_.Run(event);
  if (!continue_decoding) {
    // Changing the state to idle will abort any pending decodes.
    state_ = DecoderWrapperState::kIdle;
  }
  return continue_decoding;
}

}  // namespace test
}  // namespace media
