// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decoder.h"

#include <drm_fourcc.h>

#include <algorithm>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/chromeos_status.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/legacy/v4l2_video_decoder_backend_stateful.h"
#include "media/gpu/v4l2/v4l2_status.h"
#include "media/gpu/v4l2/v4l2_utils.h"
#include "media/gpu/v4l2/v4l2_video_decoder_backend_stateless.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
// gn check does not account for BUILDFLAG(), so including this header will
// make gn check fail for builds other than ash-chrome. See gn help nogncheck
// for more information.
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(jkardatzke): Remove these once they are in linux/videodev2.h.
#define V4L2_CID_MPEG_MTK_BASE (0x00990000 | 0x2000)
#define V4L2_CID_MPEG_MTK_GET_SECURE_HANDLE (V4L2_CID_MPEG_MTK_BASE + 8)
#define V4L2_CID_MPEG_MTK_SET_SECURE_MODE (V4L2_CID_MPEG_MTK_BASE + 9)

namespace media {

namespace {

using PixelLayoutCandidate = ImageProcessor::PixelLayoutCandidate;

// See http://crbug.com/255116.
constexpr int k480pArea = 720 * 480;
constexpr int k1080pArea = 1920 * 1088;
// We are aligning these with the Widevine spec for sample sizes for various
// resolutions. 1MB for SD, 2MB for HD and 4MB for UHD.
// Input bitstream buffer size for up to 480p streams.
constexpr size_t kInputBuferMaxSizeFor480p = 1024 * 1024;
// Input bitstream buffer size for up to 1080p streams.
constexpr size_t kInputBufferMaxSizeFor1080p = 2 * kInputBuferMaxSizeFor480p;
// Input bitstream buffer size for up to 4k streams.
constexpr size_t kInputBufferMaxSizeFor4k = 2 * kInputBufferMaxSizeFor1080p;

// Input format V4L2 fourccs this class supports.
const std::vector<uint32_t> kSupportedInputFourccs = {
    // V4L2 stateless formats
    V4L2_PIX_FMT_H264_SLICE,
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    V4L2_PIX_FMT_HEVC_SLICE,
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    V4L2_PIX_FMT_VP8_FRAME,
    V4L2_PIX_FMT_VP9_FRAME,
    V4L2_PIX_FMT_AV1_FRAME,
    // V4L2 stateful formats
    V4L2_PIX_FMT_H264,
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    V4L2_PIX_FMT_HEVC,
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    V4L2_PIX_FMT_VP8,
    V4L2_PIX_FMT_VP9,
    V4L2_PIX_FMT_AV1,
};

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "V4l2VideoDecoderFunctions" in src/tools/metrics/histograms/enums.xml.
enum class V4l2VideoDecoderFunctions {
  kInitializeBackend = 0,
  kSetupInputFormat = 1,
  kSetupOutputFormat = 2,
  kStartStreamV4L2Queue = 3,
  kStopStreamV4L2Queue = 4,
  // Anything else is captured in this last entry.
  kOtherV4l2VideoDecoderFunction = 5,
  kMaxValue = kOtherV4l2VideoDecoderFunction,
};

constexpr std::array<const char*,
                     static_cast<size_t>(V4l2VideoDecoderFunctions::kMaxValue) +
                         1>
    kV4l2VideoDecoderFunctionNames = {
        "InitializeBackend",   "SetupInputFormat",
        "SetupOutputFormat",   "StartStreamV4L2Queue",
        "StopStreamV4L2Queue", "Other V4l2VideoDecoder functions"};

// Translates |function| into a human readable string for logging.
const char* V4l2VideoDecoderFunctionName(V4l2VideoDecoderFunctions function) {
  CHECK(function <= V4l2VideoDecoderFunctions::kMaxValue);
  return kV4l2VideoDecoderFunctionNames[static_cast<size_t>(function)];
}

// Logs and records UMA when member functions of V4l2VideoDecoder class fail.
void LogAndRecordUMA(const base::Location& location,
                     V4l2VideoDecoderFunctions function,
                     const std::string& message = "") {
  LOG(ERROR) << V4l2VideoDecoderFunctionName(function) << " failed at "
             << location.ToString() << (message.empty() ? " " : " : ")
             << message;
  base::UmaHistogramEnumeration("Media.V4l2VideoDecoder.Error", function);
}

size_t GetInputBufferSizeForResolution(const gfx::Size& resolution) {
  auto area = resolution.GetArea();
  if (area < k480pArea) {
    return kInputBuferMaxSizeFor480p;
  } else if (area < k1080pArea) {
    return kInputBufferMaxSizeFor1080p;
  } else {
    return kInputBufferMaxSizeFor4k;
  }
}

}  // namespace

// static
base::AtomicRefCount V4L2VideoDecoder::num_instances_(0);

// static
std::unique_ptr<VideoDecoderMixin> V4L2VideoDecoder::Create(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client) {
  DCHECK(decoder_task_runner->RunsTasksInCurrentSequence());
  DCHECK(client);

  return base::WrapUnique<VideoDecoderMixin>(
      new V4L2VideoDecoder(std::move(media_log), std::move(decoder_task_runner),
                           std::move(client), new V4L2Device()));
}

// static
std::optional<SupportedVideoDecoderConfigs>
V4L2VideoDecoder::GetSupportedConfigs() {
  auto device = base::MakeRefCounted<V4L2Device>();
  auto configs = device->GetSupportedDecodeProfiles(kSupportedInputFourccs);
  if (configs.empty())
    return std::nullopt;

  return ConvertFromSupportedProfiles(configs,
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
                                      true /* allow_encrypted */);
#else
                                      false /* allow_encrypted */);
#endif
}

V4L2VideoDecoder::V4L2VideoDecoder(
    std::unique_ptr<MediaLog> media_log,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    base::WeakPtr<VideoDecoderMixin::Client> client,
    scoped_refptr<V4L2Device> device)
    : VideoDecoderMixin(std::move(media_log),
                        std::move(decoder_task_runner),
                        std::move(client)),
      device_(std::move(device)),
      weak_this_for_polling_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  VLOGF(2);

  weak_this_for_polling_ = weak_this_for_polling_factory_.GetWeakPtr();
}

V4L2VideoDecoder::~V4L2VideoDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(2);

  // Call all pending decode callback.
  if (backend_) {
    backend_->ClearPendingRequests(DecoderStatus::Codes::kAborted);
    backend_ = nullptr;
  }

  // Stop and Destroy device.
  if (!StopStreamV4L2Queue(true)) {
    LogAndRecordUMA(FROM_HERE, V4l2VideoDecoderFunctions::kStopStreamV4L2Queue);
  }

  if (input_queue_) {
    if (!input_queue_->DeallocateBuffers())
      VLOGF(1) << "Failed to deallocate V4L2 input buffers";
    input_queue_ = nullptr;
  }
  if (output_queue_) {
    if (!output_queue_->DeallocateBuffers())
      VLOGF(1) << "Failed to deallocate V4L2 output buffers";
    output_queue_ = nullptr;
  }

  weak_this_for_polling_factory_.InvalidateWeakPtrs();

  if (can_use_decoder_)
    num_instances_.Decrement();
}

void V4L2VideoDecoder::Initialize(const VideoDecoderConfig& config,
                                  bool /*low_delay*/,
                                  CdmContext* cdm_context,
                                  InitCB init_cb,
                                  const PipelineOutputCB& output_cb,
                                  const WaitingCB& /*waiting_cb*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK(config.IsValidConfig());
  DVLOGF(3);

  switch (state_) {
    case State::kUninitialized:
    case State::kInitialized:
    case State::kDecoding:
      // Expected state, do nothing.
      break;
    case State::kFlushing:
    case State::kError:
      VLOGF(1) << "V4L2 decoder should not be initialized at state: "
               << static_cast<int>(state_);
      std::move(init_cb).Run(DecoderStatus::Codes::kFailed);
      return;
  }

  cdm_context_ref_ = nullptr;

  if (config.is_encrypted()) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    VLOGF(1) << "Encrypted content is not supported";
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
#else
    if (!cdm_context || !cdm_context->GetChromeOsCdmContext()) {
      VLOGF(1) << "Cannot support encrypted stream w/out ChromeOsCdmContext";
      std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
      return;
    }
    if (config.codec() != VideoCodec::kH264 &&
        config.codec() != VideoCodec::kVP9 &&
        config.codec() != VideoCodec::kAV1 &&
        config.codec() != VideoCodec::kHEVC) {
      VLOGF(1) << GetCodecName(config.codec())
               << " is not supported for encrypted content";
      std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
      return;
    }
    cdm_context_ref_ = cdm_context->GetChromeOsCdmContext()->GetCdmContextRef();
#endif
  }

  // In the decoding state, we need to stop the queues since they have been
  // started already.
  if (state_ == State::kDecoding) {
    if (!StopStreamV4L2Queue(true)) {
      LogAndRecordUMA(FROM_HERE,
                      V4l2VideoDecoderFunctions::kStopStreamV4L2Queue);

      // TODO(crbug.com/40139291): Make StopStreamV4L2Queue return a StatusOr,
      // and pipe that back instead.
      std::move(init_cb).Run(
          DecoderStatus(DecoderStatus::Codes::kNotInitialized)
              .AddCause(
                  V4L2Status(V4L2Status::Codes::kFailedToStopStreamQueue)));
      return;
    }
  }
  // In the decoding or initialized state, we need to tear everything else down
  // as well.
  if (state_ == State::kDecoding || state_ == State::kInitialized) {
    if (!input_queue_->DeallocateBuffers() ||
        !output_queue_->DeallocateBuffers()) {
      VLOGF(1) << "Failed to deallocate V4L2 buffers";
      std::move(init_cb).Run(
          DecoderStatus(DecoderStatus::Codes::kNotInitialized)
              .AddCause(
                  V4L2Status(V4L2Status::Codes::kFailedToDestroyQueueBuffers)));
      return;
    }
    input_queue_ = nullptr;
    output_queue_ = nullptr;

    if (can_use_decoder_) {
      num_instances_.Decrement();
      can_use_decoder_ = false;
    }

    device_ = base::MakeRefCounted<V4L2Device>();
    continue_change_resolution_cb_.Reset();
    if (backend_)
      backend_ = nullptr;
  }
  if (config.is_encrypted()) {
    device_->set_secure_allocate_cb(
        base::BindRepeating(&V4L2VideoDecoder::AllocateSecureBuffer,
                            weak_this_for_callbacks_.GetWeakPtr()));
  } else {
    device_->set_secure_allocate_cb(AllocateSecureBufferAsCallback());
  }

  DCHECK(!input_queue_);
  DCHECK(!output_queue_);

  profile_ = config.profile();
  aspect_ratio_ = config.aspect_ratio();
  color_space_ = config.color_space_info();
  current_resolution_ = config.visible_rect().size();

  if (profile_ == VIDEO_CODEC_PROFILE_UNKNOWN) {
    VLOGF(1) << "Unknown profile.";
    SetState(State::kError);
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kNotInitialized)
            .AddCause(V4L2Status(V4L2Status::Codes::kNoProfile)));
    return;
  }
  if (VideoCodecProfileToVideoCodec(profile_) == VideoCodec::kAV1 &&
      !base::FeatureList::IsEnabled(kChromeOSHWAV1Decoder)) {
    VLOGF(1) << "AV1 hardware video decoding is disabled";
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kNotInitialized)
            .AddCause(V4L2Status(V4L2Status::Codes::kNoProfile)));
    return;
  }

  V4L2Status status = InitializeBackend();
  if (status != V4L2Status::Codes::kOk) {
    LogAndRecordUMA(FROM_HERE, V4l2VideoDecoderFunctions::kInitializeBackend);

    SetState(State::kError);
    std::move(init_cb).Run(DecoderStatus(DecoderStatus::Codes::kNotInitialized)
                               .AddCause(std::move(status)));
    return;
  }

  // Call init_cb.
  output_cb_ = std::move(output_cb);

  // Call init_cb
  if (pending_secure_allocate_callbacks_) {
    // We need to wait for these to complete before we invoke the init callback.
    pending_init_cb_ = std::move(init_cb);
    return;
  }
  SetState(State::kInitialized);
  std::move(init_cb).Run(DecoderStatus::Codes::kOk);
}

bool V4L2VideoDecoder::NeedsBitstreamConversion() const {
  DCHECK(output_cb_) << "V4L2VideoDecoder hasn't been initialized";
  NOTREACHED_IN_MIGRATION();
  return (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) ||
         (profile_ >= HEVCPROFILE_MIN && profile_ <= HEVCPROFILE_MAX);
}

bool V4L2VideoDecoder::CanReadWithoutStalling() const {
  NOTIMPLEMENTED();
  NOTREACHED();
}

int V4L2VideoDecoder::GetMaxDecodeRequests() const {
  NOTREACHED();
}

VideoDecoderType V4L2VideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kV4L2;
}

bool V4L2VideoDecoder::IsPlatformDecoder() const {
  return true;
}

V4L2Status V4L2VideoDecoder::InitializeBackend() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  can_use_decoder_ =
      num_instances_.Increment() < kMaxNumOfInstances ||
      !base::FeatureList::IsEnabled(media::kLimitConcurrentDecoderInstances);
  if (!can_use_decoder_) {
    VLOGF(1) << "Reached maximum number of decoder instances ("
             << kMaxNumOfInstances << ")";
    return V4L2Status::Codes::kMaxDecoderInstanceCount;
  }

  constexpr bool kStateful = false;
  constexpr bool kStateless = true;
  std::optional<std::pair<bool, uint32_t>> api_and_format;
  // Try both kStateful and kStateless APIs via |fourcc| and select the first
  // combination where Open()ing the |device_| works.
  for (const auto api : {kStateful, kStateless}) {
    const auto fourcc = VideoCodecProfileToV4L2PixFmt(profile_, api);
    if (fourcc == V4L2_PIX_FMT_INVALID ||
        !device_->Open(V4L2Device::Type::kDecoder, fourcc)) {
      continue;
    }
    api_and_format = std::make_pair(api, fourcc);
    break;
  }

  if (!api_and_format.has_value()) {
    num_instances_.Decrement();
    can_use_decoder_ = false;
    VLOGF(1) << "No V4L2 API found for profile: " << GetProfileName(profile_);
    return V4L2Status::Codes::kNoDriverSupportForFourcc;
  }

  // TODO(jkardatzke): Remove this when we switch to using the V4L2 secure
  // memory flag. This CTRL is not in the ToT kernel, so only set this when we
  // are doing secure playback (which has dependent code also not in the ToT
  // kernel).
#if BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)
  if (cdm_context_ref_) {
    // Set SVP (secure video pipeline) mode.
    struct v4l2_ext_control ctrl;
    struct v4l2_ext_controls ctrls;
    memset(&ctrls, 0, sizeof(ctrls));
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_MTK_SET_SECURE_MODE;
    ctrl.value = 1;

    ctrls.count = 1;
    ctrls.which = V4L2_CTRL_WHICH_CUR_VAL;
    ctrls.controls = &ctrl;

    VLOGF(1) << "Setting secure playback mode";
    if (device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ctrls)) {
      PLOG(ERROR) << "Failed setting secure playback mode";
    }
  }
#endif  // BUILDFLAG(USE_CHROMEOS_PROTECTED_MEDIA)

  struct v4l2_capability caps;
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
  if (device_->Ioctl(VIDIOC_QUERYCAP, &caps) ||
      (caps.capabilities & kCapsRequired) != kCapsRequired) {
    VLOGF(1) << "ioctl() failed: VIDIOC_QUERYCAP, "
             << "caps check failed: 0x" << std::hex << caps.capabilities;
    return V4L2Status::Codes::kFailedFileCapabilitiesCheck;
  }

  // Create Input/Output V4L2Queue
  input_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  output_queue_ = device_->GetQueue(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
  if (!input_queue_ || !output_queue_) {
    VLOGF(1) << "Failed to create V4L2 queue.";
    return V4L2Status::Codes::kFailedResourceAllocation;
  }

  const auto preferred_api_and_format = api_and_format.value();
  input_format_fourcc_ = preferred_api_and_format.second;
  if (preferred_api_and_format.first == kStateful) {
    VLOGF(1) << "Using a stateful API for profile: " << GetProfileName(profile_)
             << " and fourcc: " << FourccToString(input_format_fourcc_);
    backend_ = std::make_unique<V4L2StatefulVideoDecoderBackend>(
        this, device_, profile_, color_space_, decoder_task_runner_);
  } else {
    DCHECK_EQ(preferred_api_and_format.first, kStateless);
    VLOGF(1) << "Using a stateless API for profile: "
             << GetProfileName(profile_)
             << " and fourcc: " << FourccToString(input_format_fourcc_);
    backend_ = std::make_unique<V4L2StatelessVideoDecoderBackend>(
        this, device_, profile_, color_space_, decoder_task_runner_,
        cdm_context_ref_ ? cdm_context_ref_->GetCdmContext() : nullptr);
  }

  if (!backend_->Initialize()) {
    VLOGF(1) << "Failed to initialize backend.";
    return V4L2Status::Codes::kFailedResourceAllocation;
  }

  if (!SetupInputFormat()) {
    LogAndRecordUMA(FROM_HERE, V4l2VideoDecoderFunctions::kSetupInputFormat);
    return V4L2Status::Codes::kBadFormat;
  }

  if (!AllocateInputBuffers()) {
    VLOGF(1) << "Failed to allocate input buffer.";
    return V4L2Status::Codes::kFailedResourceAllocation;
  }

  return V4L2Status::Codes::kOk;
}

void V4L2VideoDecoder::AllocateSecureBuffer(uint32_t size,
                                            SecureBufferAllocatedCB callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  pending_secure_allocate_callbacks_++;
  // Wrap this with a default handler if it gets dropped somehow or otherwise we
  // could hang waiting to finish init.
  cdm_context_ref_->GetCdmContext()
      ->GetChromeOsCdmContext()
      ->AllocateSecureBuffer(
          size,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              base::BindPostTaskToCurrentDefault(base::BindOnce(
                  &V4L2VideoDecoder::AllocateSecureBufferCB,
                  weak_this_for_callbacks_.GetWeakPtr(), std::move(callback))),
              mojo::PlatformHandle()));
#else
  NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void V4L2VideoDecoder::AllocateSecureBufferCB(SecureBufferAllocatedCB callback,
                                              mojo::PlatformHandle mojo_fd) {
  if (state_ == State::kError) {
    // Drop this and return, we've already entered the error state from a prior
    // failed callback so we have nothing to do.
    return;
  }
  if (!mojo_fd.is_valid()) {
    LOG(ERROR) << "Invalid Mojo FD returned for secure buffer allocation";
    SetState(State::kError);
    std::move(pending_init_cb_)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }
  base::ScopedFD secure_fd = mojo_fd.TakeFD();
  if (!secure_fd.is_valid()) {
    LOG(ERROR) << "Invalid FD returned for secure buffer allocation";
    SetState(State::kError);
    std::move(pending_init_cb_)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  // Also resolve the secure handle in case failure occurs there, then we know
  // what we pass into the callback is all valid.
  struct v4l2_ext_control ctrl;
  struct v4l2_ext_controls ctrls;
  memset(&ctrls, 0, sizeof(ctrls));
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_MTK_GET_SECURE_HANDLE;
  ctrl.value = secure_fd.get();

  ctrls.count = 1;
  ctrls.which = V4L2_CTRL_WHICH_CUR_VAL;
  ctrls.controls = &ctrl;

  if (device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ctrls)) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocSExtCtrls);
    PLOG(ERROR) << "Failed getting secure buffer identifier for FD "
                << secure_fd.get();
    SetState(State::kError);
    std::move(pending_init_cb_)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }
  uint64_t secure_handle = static_cast<uint64_t>(ctrl.value);

  // We have the secure buffer and secure handle, pass it into the callback.
  std::move(callback).Run(std::move(secure_fd), secure_handle);

  CHECK_GT(pending_secure_allocate_callbacks_, 0u);
  pending_secure_allocate_callbacks_--;
  if (!pending_secure_allocate_callbacks_) {
    if (pending_init_cb_) {
      // This is from the initial secure buffer allocations during Initialize().
      SetState(State::kInitialized);
      std::move(pending_init_cb_).Run(DecoderStatus::Codes::kOk);
    } else {
      // This is from the secure buffer allocations due to a resolution change.
      OnChangeResolutionDone(pending_change_resolution_done_status_);
    }
  }
}

bool V4L2VideoDecoder::SetupInputFormat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  // Check if the format is supported.
  const auto v4l2_codecs_as_pix_fmts = EnumerateSupportedPixFmts(
      base::BindRepeating(&V4L2Device::Ioctl, device_),
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
  if (!base::Contains(v4l2_codecs_as_pix_fmts, input_format_fourcc_)) {
    DVLOGF(1) << FourccToString(input_format_fourcc_)
              << " not recognised, skipping...";
    return false;
  }
  VLOGF(1) << "Input (OUTPUT queue) Fourcc: "
           << FourccToString(input_format_fourcc_);

  // Determine the input buffer size, for backends that support stopping the
  // input queue on resolution changes this can be dynamic; otherwise we need to
  // calculate it based on the maximum frame size the decoder can handle.
  size_t input_size;
  if (backend_->StopInputQueueOnResChange()) {
    input_size = GetInputBufferSizeForResolution(current_resolution_);
  } else {
    gfx::Size max_size, min_size;
    GetSupportedResolution(base::BindRepeating(&V4L2Device::Ioctl, device_),
                           input_format_fourcc_, &min_size, &max_size);
    input_size = GetInputBufferSizeForResolution(max_size);
  }

  // Setup the input format.
  auto format =
      input_queue_->SetFormat(input_format_fourcc_, gfx::Size(), input_size);
  if (!format) {
    VPLOGF(1) << "Failed to call IOCTL to set input format.";
    return false;
  }
  DCHECK_EQ(format->fmt.pix_mp.pixelformat, input_format_fourcc_)
      << "The input (OUTPUT) queue must accept the requested pixel format "
      << FourccToString(input_format_fourcc_) << ", but it returned instead: "
      << FourccToString(format->fmt.pix_mp.pixelformat);

  return true;
}

bool V4L2VideoDecoder::AllocateInputBuffers() {
  const size_t num_OUTPUT_buffers =
      backend_->GetNumOUTPUTQueueBuffers(!!cdm_context_ref_);
  // Secure playback uses dmabufs for the OUTPUT queue, otherwise we use mmap
  // buffers.
  v4l2_memory input_queue_memory =
      !!cdm_context_ref_ ? V4L2_MEMORY_DMABUF : V4L2_MEMORY_MMAP;
  VLOGF(1) << "Requesting: " << num_OUTPUT_buffers << " OUTPUT buffers of type "
           << (input_queue_memory == V4L2_MEMORY_MMAP ? "V4L2_MEMORY_MMAP"
                                                      : "V4L2_MEMORY_DMABUF");
  return input_queue_->AllocateBuffers(num_OUTPUT_buffers, input_queue_memory,
                                       incoherent_) != 0;
}

CroStatus V4L2VideoDecoder::SetupOutputFormat(const gfx::Size& size,
                                              const gfx::Rect& visible_rect,
                                              size_t num_codec_reference_frames,
                                              uint8_t bit_depth) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << "size: " << size.ToString()
            << ", visible_rect: " << visible_rect.ToString();

  if (bit_depth == 10u) {
    VLOGF(1) << "10-bit format, need to set EXT_CTRLS first";
    CroStatus ext_status = SetExtCtrls10Bit(size);
    if (ext_status != CroStatus::Codes::kOk) {
      return ext_status;
    }
  }

  const auto v4l2_pix_fmts = EnumerateSupportedPixFmts(
      base::BindRepeating(&V4L2Device::Ioctl, device_),
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

  std::vector<PixelLayoutCandidate> candidates;
  for (const uint32_t& pixfmt : v4l2_pix_fmts) {
    const auto candidate = Fourcc::FromV4L2PixFmt(pixfmt);
    if (!candidate) {
      DVLOGF(1) << FourccToString(pixfmt) << " is not recognised, skipping...";
      continue;
    }
    VLOGF(1) << "Output (CAPTURE queue) candidate: " << candidate->ToString();

    // Some drivers will enumerate all possible formats for a video stream.
    // If the compressed video stream is 10 bits, the driver will enumerate both
    // P010 and NV12, and then down sample to NV12 if it is selected. This is
    // not desired, so drop the candidates that don't match the bit depth of the
    // stream.
    size_t candidate_bit_depth = BitDepth(candidate->ToVideoPixelFormat());
    if (candidate_bit_depth != bit_depth) {
      DVLOGF(1) << "Enumerated format " << candidate->ToString()
                << " with a bit depth of " << candidate_bit_depth
                << " removed from consideration because it does not match"
                << " the bit depth returned by the backend of "
                << base::strict_cast<size_t>(bit_depth);
      continue;
    }

    std::optional<struct v4l2_format> format =
        output_queue_->TryFormat(pixfmt, size, 0);
    if (!format)
      continue;

    gfx::Size adjusted_size(format->fmt.pix_mp.width,
                            format->fmt.pix_mp.height);

    candidates.emplace_back(
        PixelLayoutCandidate{.fourcc = *candidate, .size = adjusted_size});
  }

  // Ask the pipeline to pick the output format.
  CroStatus::Or<PixelLayoutCandidate> status_or_output_format =
      client_->PickDecoderOutputFormat(
          candidates, visible_rect, aspect_ratio_.GetNaturalSize(visible_rect),
          /*output_size=*/std::nullopt, num_codec_reference_frames,
          /*use_protected=*/!!cdm_context_ref_, /*need_aux_frame_pool=*/false,
          std::nullopt);
  if (!status_or_output_format.has_value()) {
    VLOGF(1) << "Failed to pick an output format.";
    return std::move(status_or_output_format).error().code();
  }
  const PixelLayoutCandidate output_format =
      std::move(status_or_output_format).value();
  Fourcc fourcc = std::move(output_format.fourcc);
  gfx::Size picked_size = std::move(output_format.size);

  // We successfully picked the output format. Now setup output format again.
  std::optional<struct v4l2_format> format =
      output_queue_->SetFormat(fourcc.ToV4L2PixFmt(), picked_size, 0);
  DCHECK(format);
  gfx::Size adjusted_size(format->fmt.pix_mp.width, format->fmt.pix_mp.height);
  if (!gfx::Rect(adjusted_size).Contains(gfx::Rect(picked_size))) {
    VLOGF(1) << "The adjusted coded size (" << adjusted_size.ToString()
             << ") should contains the original coded size("
             << picked_size.ToString() << ").";
    return CroStatus::Codes::kFailedToChangeResolution;
  }

  // Got the adjusted size from the V4L2 driver. Now setup the frame pool.
  // TODO(akahuang): It is possible there is an allocatable formats among
  // candidates, but PickDecoderOutputFormat() selects other non-allocatable
  // format. The correct flow is to attach an info to candidates if it is
  // created by VideoFramePool.
  DmabufVideoFramePool* pool = client_->GetVideoFramePool();
  if (pool) {
    std::optional<GpuBufferLayout> layout = pool->GetGpuBufferLayout();
    if (!layout.has_value()) {
      VLOGF(1) << "Failed to get format from VFPool";
      return CroStatus::Codes::kFailedToChangeResolution;
    }

    if (layout->size() != adjusted_size) {
      VLOGF(1) << "The size adjusted by VFPool is different from one "
               << "adjusted by a video driver. fourcc: " << fourcc.ToString()
               << ", (video driver v.s. VFPool) " << adjusted_size.ToString()
               << " != " << layout->size().ToString();
      return CroStatus::Codes::kFailedToChangeResolution;
    }

    VLOGF(1) << "buffer modifier: " << std::hex << layout->modifier();
    if (layout->modifier() != DRM_FORMAT_MOD_LINEAR &&
        layout->modifier() != gfx::NativePixmapHandle::kNoModifier) {
      std::optional<struct v4l2_format> modifier_format =
          output_queue_->SetModifierFormat(layout->modifier(), picked_size);
      if (!modifier_format)
        return CroStatus::Codes::kFailedToChangeResolution;

      gfx::Size size_for_modifier_format(format->fmt.pix_mp.width,
                                         format->fmt.pix_mp.height);
      if (size_for_modifier_format != adjusted_size) {
        VLOGF(1)
            << "Buffers were allocated for " << adjusted_size.ToString()
            << " but modifier format is expecting buffers to be allocated for "
            << size_for_modifier_format.ToString();
        return CroStatus::Codes::kFailedToChangeResolution;
      }
    }
  }

  return CroStatus::Codes::kOk;
}

CroStatus V4L2VideoDecoder::SetExtCtrls10Bit(const gfx::Size& size) {
  std::vector<struct v4l2_ext_control> ctrls;
  struct v4l2_ctrl_hevc_sps v4l2_sps;
  struct v4l2_ctrl_vp9_frame v4l2_vp9_frame;
#if BUILDFLAG(IS_CHROMEOS)
  struct v4l2_ctrl_av1_sequence v4l2_av1_sequence;
#endif

  struct v4l2_ext_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));

  // 10-bit formats require codec specific parameters be passed before the
  // CAPTURE queue will report the proper decoded formats.
  if (input_format_fourcc_ == V4L2_PIX_FMT_HEVC_SLICE) {
    // For HEVC the SPS data is sent in to indicate 10-bit content. We also set
    // the size and chroma format since that should be all the information
    // needed in order to know the format.
    VLOGF(1) << "Setting EXT_CTRLS for 10-bit HEVC";
    memset(&v4l2_sps, 0, sizeof(v4l2_sps));
    v4l2_sps.pic_width_in_luma_samples = size.width();
    v4l2_sps.pic_height_in_luma_samples = size.height();
    v4l2_sps.bit_depth_luma_minus8 = 2;
    v4l2_sps.bit_depth_chroma_minus8 = 2;
    v4l2_sps.chroma_format_idc = 1;  // 4:2:0

    ctrl.id = V4L2_CID_STATELESS_HEVC_SPS;
    ctrl.size = sizeof(v4l2_sps);
    ctrl.ptr = &v4l2_sps;

    ctrls.push_back(ctrl);
  } else if (input_format_fourcc_ == V4L2_PIX_FMT_VP9_FRAME) {
    // VP9 requires the profile (only profile 2), bit depth , and flags
    VLOGF(1) << "Setting EXT_CTRLS for 10-bit VP9.2";
    memset(&v4l2_vp9_frame, 0, sizeof(v4l2_vp9_frame));
    v4l2_vp9_frame.bit_depth = 10;
    v4l2_vp9_frame.profile = 2;
    v4l2_vp9_frame.flags =
        V4L2_VP9_FRAME_FLAG_X_SUBSAMPLING | V4L2_VP9_FRAME_FLAG_Y_SUBSAMPLING;

    ctrl.id = V4L2_CID_STATELESS_VP9_FRAME;
    ctrl.size = sizeof(v4l2_vp9_frame);
    ctrl.ptr = &v4l2_vp9_frame;

    ctrls.push_back(ctrl);
#if BUILDFLAG(IS_CHROMEOS)
  } else if (input_format_fourcc_ == V4L2_PIX_FMT_AV1_FRAME) {
    // AV1 only requires that the |bit_depth| parameter be set to enable
    // 10 bit formats on the CAPTURE queue.
    VLOGF(1) << "Setting EXT_CTRLS for 10-bit AV1";
    memset(&v4l2_av1_sequence, 0, sizeof(v4l2_av1_sequence));
    v4l2_av1_sequence.bit_depth = 10;

    ctrl.id = V4L2_CID_STATELESS_AV1_SEQUENCE;
    ctrl.size = sizeof(v4l2_av1_sequence);
    ctrl.ptr = &v4l2_av1_sequence;

    ctrls.push_back(ctrl);
#endif
  } else {
    // TODO(b/): Add other 10-bit codecs
    return CroStatus::Codes::kNoDecoderOutputFormatCandidates;
  }

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.count = ctrls.size();
  ext_ctrls.controls = ctrls.data();
  ext_ctrls.which = V4L2_CTRL_WHICH_CUR_VAL;
  ext_ctrls.request_fd = -1;
  if (device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_S_EXT_CTRLS";
    return CroStatus::Codes::kNoDecoderOutputFormatCandidates;
  }

  return CroStatus::Codes::kOk;
}

void V4L2VideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  // In order to preserve the order of the callbacks between Decode() and
  // Reset(), we also trampoline the callback of Reset().
  auto trampoline_reset_cb =
      base::BindOnce(&base::SequencedTaskRunner::PostTask,
                     base::SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
                     std::move(closure));

  if (state_ == State::kInitialized) {
    std::move(trampoline_reset_cb).Run();
    return;
  }
  if (!backend_) {
    VLOGF(1) << "Backend was destroyed while resetting.";
    SetState(State::kError);
    return;
  }

  // Reset callback for resolution change, because the pipeline won't notify
  // flushed after reset.
  if (continue_change_resolution_cb_) {
    continue_change_resolution_cb_.Reset();
    backend_->OnChangeResolutionDone(CroStatus::Codes::kResetRequired);
  }

  // Call all pending decode callback.
  backend_->ClearPendingRequests(DecoderStatus::Codes::kAborted);

  // Streamoff V4L2 queues to drop input and output buffers.
  RestartStream();

  // If during flushing, Reset() will abort the following flush tasks.
  // Now we are ready to decode new buffer. Go back to decoding state.
  SetState(State::kDecoding);

  std::move(trampoline_reset_cb).Run();
}

void V4L2VideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                              DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DCHECK_NE(state_, State::kUninitialized);

  // VideoDecoder interface: |decode_cb| can't be called from within Decode().
  auto trampoline_decode_cb = base::BindOnce(
      [](const scoped_refptr<base::SequencedTaskRunner>& this_sequence_runner,
         DecodeCB decode_cb, DecoderStatus status) {
        this_sequence_runner->PostTask(
            FROM_HERE, base::BindOnce(std::move(decode_cb), status));
      },
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(decode_cb));

  if (state_ == State::kError) {
    std::move(trampoline_decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (state_ == State::kInitialized) {
    // Start streaming input queue and polling. This is required for the
    // stateful decoder, and doesn't hurt for the stateless one.
    if (!StartStreamV4L2Queue(false)) {
      LogAndRecordUMA(FROM_HERE,
                      V4l2VideoDecoderFunctions::kStartStreamV4L2Queue);
      SetState(State::kError);
      std::move(trampoline_decode_cb)
          .Run(DecoderStatus(DecoderStatus::Codes::kFailed)
                   .AddCause(V4L2Status(
                       V4L2Status::Codes::kFailedToStartStreamQueue)));
      return;
    }
    SetState(State::kDecoding);
  }

  backend_->EnqueueDecodeTask(std::move(buffer),
                              std::move(trampoline_decode_cb));
}

bool V4L2VideoDecoder::StartStreamV4L2Queue(bool start_output_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  if (!input_queue_->Streamon() ||
      (start_output_queue && !output_queue_->Streamon())) {
    VLOGF(1) << "Failed to streamon V4L2 queue.";
    SetState(State::kError);
    return false;
  }

  if (!device_->StartPolling(
          base::BindRepeating(&V4L2VideoDecoder::ServiceDeviceTask,
                              weak_this_for_polling_),
          base::BindRepeating(&V4L2VideoDecoder::SetState,
                              weak_this_for_polling_, State::kError))) {
    SetState(State::kError);
    return false;
  }

  return true;
}

bool V4L2VideoDecoder::StopStreamV4L2Queue(bool stop_input_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  if (!device_->StopPolling()) {
    SetState(State::kError);
    return false;
  }

  // Invalidate the callback from the device.
  weak_this_for_polling_factory_.InvalidateWeakPtrs();
  weak_this_for_polling_ = weak_this_for_polling_factory_.GetWeakPtr();

  // Streamoff input and output queue.
  if (input_queue_ && stop_input_queue && !input_queue_->Streamoff()) {
    SetState(State::kError);
    return false;
  }

  if (output_queue_ && !output_queue_->Streamoff()) {
    SetState(State::kError);
    return false;
  }

  if (backend_)
    backend_->OnStreamStopped(stop_input_queue);

  return true;
}

void V4L2VideoDecoder::InitiateFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  SetState(State::kFlushing);
}

void V4L2VideoDecoder::CompleteFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  if (state_ != State::kFlushing) {
    VLOGF(1) << "Completed flush in the wrong state: "
             << static_cast<int>(state_);
    SetState(State::kError);
  } else {
    SetState(State::kDecoding);
  }
}

void V4L2VideoDecoder::RestartStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  // If the queues are streaming before reset, then we need to start streaming
  // them after stopping.
  const bool is_input_streaming = input_queue_->IsStreaming();
  const bool is_output_streaming = output_queue_->IsStreaming();

  if (!StopStreamV4L2Queue(true)) {
    LogAndRecordUMA(FROM_HERE, V4l2VideoDecoderFunctions::kStopStreamV4L2Queue);
    return;
  }

  if (is_input_streaming) {
    if (!StartStreamV4L2Queue(is_output_streaming)) {
      LogAndRecordUMA(FROM_HERE,
                      V4l2VideoDecoderFunctions::kStartStreamV4L2Queue);
      return;
    }
  }

  if (state_ != State::kDecoding)
    SetState(State::kDecoding);
}

void V4L2VideoDecoder::ChangeResolution(gfx::Size pic_size,
                                        gfx::Rect visible_rect,
                                        size_t num_codec_reference_frames,
                                        uint8_t bit_depth) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
  DCHECK(!continue_change_resolution_cb_);

  // After the pipeline flushes all frames, we can start changing resolution.
  // base::Unretained() is safe because |continue_change_resolution_cb_| is
  // called inside the class, so the pointer must be valid.
  continue_change_resolution_cb_ =
      base::BindOnce(&V4L2VideoDecoder::ContinueChangeResolution,
                     base::Unretained(this), pic_size, visible_rect,
                     num_codec_reference_frames, bit_depth)
          .Then(base::BindOnce(&V4L2VideoDecoder::OnChangeResolutionDone,
                               base::Unretained(this)));

  DCHECK(client_);
  client_->PrepareChangeResolution();
}

void V4L2VideoDecoder::ApplyResolutionChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);
  DCHECK(continue_change_resolution_cb_);

  std::move(continue_change_resolution_cb_).Run();
}

size_t V4L2VideoDecoder::GetMaxOutputFramePoolSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  // VIDEO_MAX_FRAME is used as a size in V4L2 decoder drivers like Qualcomm
  // Venus. We should not exceed this limit for the frame pool that the decoder
  // writes into.
  return VIDEO_MAX_FRAME;
}

bool V4L2VideoDecoder::NeedsTranscryption() {
  return !!cdm_context_ref_;
}

CroStatus V4L2VideoDecoder::AttachSecureBuffer(
    scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  if (!cdm_context_ref_) {
    return CroStatus::Codes::kOk;
  }

  if (state_ == State::kError) {
    return CroStatus::Codes::kUnableToAllocateSecureBuffer;
  }

  auto secure_handle_or_error = input_queue_->GetFreeSecureHandle();
  if (!secure_handle_or_error.has_value()) {
    // This may only mean we are currently out of buffers, it's not necessarily
    // a fatal error.
    return std::move(secure_handle_or_error).error().code();
  }
  buffer->WritableSideData().secure_handle =
      std::move(secure_handle_or_error).value();
  return CroStatus::Codes::kOk;
}

void V4L2VideoDecoder::ReleaseSecureBuffer(uint64_t secure_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  input_queue_->ReleaseSecureHandle(secure_handle);
}

CroStatus V4L2VideoDecoder::ContinueChangeResolution(
    const gfx::Size& pic_size,
    const gfx::Rect& visible_rect,
    const size_t num_codec_reference_frames,
    const uint8_t bit_depth) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  if (!backend_) {
    VLOGF(1) << "Backend was destroyed while changing resolution.";
    SetState(State::kError);
    return CroStatus::Codes::kFailedToChangeResolution;
  }

  // If we already reset, then skip it.
  // TODO(akahuang): Revisit to check if this condition may happen or not.
  if (state_ != State::kFlushing)
    return CroStatus::Codes::kResetRequired;

  // Stateful decoders require the input queue to keep running during resolution
  // changes, but stateless ones require it to be stopped.
  if (!StopStreamV4L2Queue(backend_->StopInputQueueOnResChange())) {
    LogAndRecordUMA(FROM_HERE, V4l2VideoDecoderFunctions::kStopStreamV4L2Queue);

    return CroStatus::Codes::kFailedToChangeResolution;
  }

  // See if we should also reallocate the input buffers. We only do this if the
  // backend supports stopping the input queue on resolution changes and the
  // buffer size would now be different.
  current_resolution_ = visible_rect.size();
  if (backend_->StopInputQueueOnResChange()) {
    size_t curr_buffer_size =
        input_queue_->GetMemoryUsage() / input_queue_->AllocatedBuffersCount();
    if (curr_buffer_size !=
        GetInputBufferSizeForResolution(current_resolution_)) {
      if (!input_queue_->DeallocateBuffers()) {
        SetState(State::kError);
        return CroStatus::Codes::kFailedToChangeResolution;
      }
      // Set the input format again, which will use the new buffer size.
      if (!SetupInputFormat()) {
        LogAndRecordUMA(FROM_HERE,
                        V4l2VideoDecoderFunctions::kSetupInputFormat);
        SetState(State::kError);
        return CroStatus::Codes::kFailedToChangeResolution;
      }
      if (!AllocateInputBuffers()) {
        VLOGF(1) << "Failed to allocate input buffers on resolution change";
        SetState(State::kError);
        return CroStatus::Codes::kFailedToChangeResolution;
      }
    }
  }

  if (!output_queue_->DeallocateBuffers()) {
    SetState(State::kError);
    return CroStatus::Codes::kFailedToChangeResolution;
  }

  if (!backend_->ApplyResolution(pic_size, visible_rect)) {
    SetState(State::kError);
    return CroStatus::Codes::kFailedToChangeResolution;
  }

  const CroStatus status = SetupOutputFormat(
      pic_size, visible_rect, num_codec_reference_frames, bit_depth);
  if (status == CroStatus::Codes::kResetRequired) {
    LogAndRecordUMA(FROM_HERE, V4l2VideoDecoderFunctions::kSetupOutputFormat,
                    "SetupOutputFormat is aborted");

    return CroStatus::Codes::kResetRequired;
  }
  if (status != CroStatus::Codes::kOk) {
    LogAndRecordUMA(FROM_HERE, V4l2VideoDecoderFunctions::kSetupOutputFormat,
                    "Failed to setup output format, status= " +
                        base::NumberToString(static_cast<int>(status.code())));

    SetState(State::kError);
    return CroStatus::Codes::kFailedToChangeResolution;
  }

  // If our |client_| has a VideoFramePool to allocate buffers for us, we'll
  // use it, otherwise we have to ask the driver.
  const bool use_v4l2_allocated_buffers = !client_->GetVideoFramePool();

  const v4l2_memory type =
      use_v4l2_allocated_buffers ? V4L2_MEMORY_MMAP : V4L2_MEMORY_DMABUF;
  // If we don't use driver-allocated buffers, request as many as possible
  // (VIDEO_MAX_FRAME) since they are shallow allocations. Otherwise, allocate
  // |num_codec_reference_frames| plus one for the video frame being decoded,
  // and one for our client (presumably an ImageProcessor).
  const size_t v4l2_num_buffers = use_v4l2_allocated_buffers
                                      ? num_codec_reference_frames + 2
                                      : VIDEO_MAX_FRAME;

  VLOGF(1) << "Requesting: " << v4l2_num_buffers << " CAPTURE buffers of type "
           << (use_v4l2_allocated_buffers ? "V4L2_MEMORY_MMAP"
                                          : "V4L2_MEMORY_DMABUF");

  const auto allocated_buffers =
      output_queue_->AllocateBuffers(v4l2_num_buffers, type, incoherent_);

  if (allocated_buffers < v4l2_num_buffers) {
    LOGF(ERROR) << "Failed to allocated enough CAPTURE buffers, requested: "
                << v4l2_num_buffers << " and got: " << allocated_buffers;
    SetState(State::kError);
    return CroStatus::Codes::kFailedToChangeResolution;
  }

  if (!StartStreamV4L2Queue(true)) {
    LogAndRecordUMA(FROM_HERE,
                    V4l2VideoDecoderFunctions::kStartStreamV4L2Queue);

    SetState(State::kError);
    return CroStatus::Codes::kFailedToChangeResolution;
  }

  return CroStatus::Codes::kOk;
}

void V4L2VideoDecoder::OnChangeResolutionDone(CroStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << static_cast<int>(status.code());

  if (!backend_) {
    // We don't need to set error state here because ContinueChangeResolution()
    // should have already done it if |backend_| is null.
    VLOGF(1) << "Backend was destroyed before resolution change finished.";
    return;
  }
  // If we still have secure buffer allocations pending, then wait until they
  // have completed before invoking the callback to complete the resolution
  // change.
  if (pending_secure_allocate_callbacks_) {
    pending_change_resolution_done_status_ = status;
    return;
  }
  backend_->OnChangeResolutionDone(status);
}

void V4L2VideoDecoder::ServiceDeviceTask(bool event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  if (input_queue_ && output_queue_) {
    DVLOGF(3) << "Number of queued input buffers: "
              << input_queue_->QueuedBuffersCount()
              << ", Number of queued output buffers: "
              << output_queue_->QueuedBuffersCount();
    TRACE_COUNTER_ID2(
        "media,gpu", "V4L2 queue sizes", this, "input (OUTPUT_queue)",
        input_queue_->QueuedBuffersCount(), "output (CAPTURE_queue)",
        output_queue_->QueuedBuffersCount());
  }

  if (backend_)
    backend_->OnServiceDeviceTask(event);

  // Dequeue V4L2 output buffer first to reduce output latency.
  bool success;
  while (output_queue_ && output_queue_->QueuedBuffersCount() > 0) {
    V4L2ReadableBufferRef dequeued_buffer;

    std::tie(success, dequeued_buffer) = output_queue_->DequeueBuffer();
    if (!success) {
      SetState(State::kError);
      return;
    }
    if (!dequeued_buffer)
      break;

    if (backend_)
      backend_->OnOutputBufferDequeued(std::move(dequeued_buffer));
  }

  // Dequeue V4L2 input buffer.
  while (input_queue_ && input_queue_->QueuedBuffersCount() > 0) {
    V4L2ReadableBufferRef dequeued_buffer;

    std::tie(success, dequeued_buffer) = input_queue_->DequeueBuffer();
    if (!success) {
      SetState(State::kError);
      return;
    }
    if (!dequeued_buffer)
      break;
  }
}

void V4L2VideoDecoder::OutputFrame(scoped_refptr<FrameResource> frame,
                                   const gfx::Rect& visible_rect,
                                   const VideoColorSpace& color_space,
                                   base::TimeDelta timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4) << "timestamp: " << timestamp.InMilliseconds() << " msec";

  // Set the timestamp at which the decode operation started on the
  // |frame|. If the frame has been outputted before (e.g. because of VP9
  // show-existing-frame feature) we can't overwrite the timestamp directly, as
  // the original frame might still be in use. Instead we wrap the frame in
  // another frame with a different timestamp.
  if (frame->timestamp().is_zero())
    frame->set_timestamp(timestamp);

  if (frame->visible_rect() != visible_rect ||
      frame->timestamp() != timestamp) {
    gfx::Size natural_size = aspect_ratio_.GetNaturalSize(visible_rect);
    scoped_refptr<FrameResource> wrapped_frame =
        frame->CreateWrappingFrame(visible_rect, natural_size);
    wrapped_frame->set_timestamp(timestamp);

    frame = std::move(wrapped_frame);
  }

  frame->set_color_space(color_space.ToGfxColorSpace());

  output_cb_.Run(std::move(frame));
}

DmabufVideoFramePool* V4L2VideoDecoder::GetVideoFramePool() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(4);

  return client_->GetVideoFramePool();
}

void V4L2VideoDecoder::SetDmaIncoherentV4L2(bool incoherent) {
  incoherent_ = incoherent;
}

void V4L2VideoDecoder::SetState(State new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3) << "Change state from " << static_cast<int>(state_) << " to "
            << static_cast<int>(new_state);

  if (state_ == new_state)
    return;
  if (state_ == State::kError) {
    DVLOGF(3) << "Already in kError state.";
    return;
  }

  // Check if the state transition is valid.
  switch (new_state) {
    case State::kUninitialized:
      VLOGF(1) << "Should not set to kUninitialized.";
      new_state = State::kError;
      break;

    case State::kInitialized:
      if ((state_ != State::kUninitialized) && (state_ != State::kDecoding)) {
        VLOGF(1) << "Can only transition to kInitialized from kUninitialized "
                    "or kDecoding";
        new_state = State::kError;
      }
      break;

    case State::kDecoding:
      break;

    case State::kFlushing:
      if (state_ != State::kDecoding) {
        VLOGF(1) << "kFlushing should only be set when kDecoding.";
        new_state = State::kError;
      }
      break;

    case State::kError:
      break;
  }

  // |StopStreamV4L2Queue()| can call |SetState()|.  Update |state_|
  // before calling so that calls to |SetState()| from
  // |StopStreamV4L2Queue()| return quickly.
  state_ = new_state;

  if (new_state == State::kError) {
    VLOGF(1) << "Error occurred, stopping queues.";
    if (!StopStreamV4L2Queue(true)) {
      LogAndRecordUMA(FROM_HERE,
                      V4l2VideoDecoderFunctions::kStopStreamV4L2Queue);
    }

    if (backend_)
      backend_->ClearPendingRequests(DecoderStatus::Codes::kFailed);
  }
}

void V4L2VideoDecoder::OnBackendError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(2);

  SetState(State::kError);
}

bool V4L2VideoDecoder::IsDecoding() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  DVLOGF(3);

  return state_ == State::kDecoding;
}

}  // namespace media
