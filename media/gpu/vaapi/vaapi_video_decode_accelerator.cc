// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decode_accelerator.h"

#include <string.h>
#include <va/va.h>

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/cpu.h"
#include "base/files/scoped_file.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/format_utils.h"
#include "media/base/media_log.h"
#include "media/base/video_util.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/h264_vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_picture.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vp8_vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/vp9_vaapi_video_decoder_delegate.h"
#include "media/gpu/vp8_decoder.h"
#include "media/gpu/vp9_decoder.h"
#include "media/video/picture.h"

namespace media {

namespace {

// Returns the preferred VA_RT_FORMAT for the given |profile|.
unsigned int GetVaFormatForVideoCodecProfile(VideoCodecProfile profile) {
  if (profile == VP9PROFILE_PROFILE2 || profile == VP9PROFILE_PROFILE3)
    return VA_RT_FORMAT_YUV420_10BPP;
  return VA_RT_FORMAT_YUV420;
}

// Returns true if the CPU is an Intel Gemini Lake or later (including Kaby
// Lake) Cpu platform id's are referenced from the following file in kernel
// source arch/x86/include/asm/intel-family.h
bool IsGeminiLakeOrLater() {
  constexpr int kPentiumAndLaterFamily = 0x06;
  constexpr int kGeminiLakeModelId = 0x7A;
  static base::CPU cpuid;
  static bool is_geminilake_or_later =
      cpuid.family() == kPentiumAndLaterFamily &&
      cpuid.model() >= kGeminiLakeModelId;
  return is_geminilake_or_later;
}

}  // namespace

#define RETURN_AND_NOTIFY_ON_FAILURE(result, log, error_code, ret) \
  do {                                                             \
    if (!(result)) {                                               \
      LOG(ERROR) << log;                                           \
      NotifyError(error_code);                                     \
      return ret;                                                  \
    }                                                              \
  } while (0)

#define RETURN_AND_NOTIFY_ON_STATUS(status, ret) \
  do {                                           \
    if (!status.is_ok()) {                       \
      NotifyStatus(status);                      \
      return ret;                                \
    }                                            \
  } while (0)

class VaapiVideoDecodeAccelerator::InputBuffer {
 public:
  InputBuffer() : buffer_(nullptr) {}
  InputBuffer(int32_t id,
              scoped_refptr<DecoderBuffer> buffer,
              base::OnceCallback<void(int32_t id)> release_cb)
      : id_(id),
        buffer_(std::move(buffer)),
        release_cb_(std::move(release_cb)) {}

  InputBuffer(const InputBuffer&) = delete;
  InputBuffer& operator=(const InputBuffer&) = delete;

  ~InputBuffer() {
    DVLOGF(4) << "id = " << id_;
    if (release_cb_)
      std::move(release_cb_).Run(id_);
  }

  // Indicates this is a dummy buffer for flush request.
  bool IsFlushRequest() const { return !buffer_; }
  int32_t id() const { return id_; }
  const scoped_refptr<DecoderBuffer>& buffer() const { return buffer_; }

 private:
  const int32_t id_ = -1;
  const scoped_refptr<DecoderBuffer> buffer_;
  base::OnceCallback<void(int32_t id)> release_cb_;
};

void VaapiVideoDecodeAccelerator::NotifyStatus(VaapiStatus status) {
  DCHECK(!status.is_ok());
  // Send a platform notification error
  NotifyError(PLATFORM_FAILURE);

  // TODO(crbug.com/1103510) there is no MediaLog here, we should change that.
  std::string output_str;
  base::JSONWriter::Write(MediaSerialize(status), &output_str);
  DLOG(ERROR) << output_str;
}

void VaapiVideoDecodeAccelerator::NotifyError(Error error) {
  if (!task_runner_->BelongsToCurrentThread()) {
    DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiVideoDecodeAccelerator::NotifyError,
                                  weak_this_, error));
    return;
  }

  VLOGF(1) << "Notifying of error " << error;
  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

VaapiVideoDecodeAccelerator::VaapiVideoDecodeAccelerator(
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb)
    : state_(kUninitialized),
      input_ready_(&lock_),
      buffer_allocation_mode_(BufferAllocationMode::kNormal),
      surfaces_available_(&lock_),
      va_surface_format_(VA_INVALID_ID),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      decoder_thread_("VaapiDecoderThread"),
      finish_flush_pending_(false),
      awaiting_va_surfaces_recycle_(false),
      requested_num_pics_(0),
      requested_num_reference_frames_(0),
      previously_requested_num_reference_frames_(0),
      profile_(VIDEO_CODEC_PROFILE_UNKNOWN),
      make_context_current_cb_(make_context_current_cb),
      bind_image_cb_(bind_image_cb),
      weak_this_factory_(this) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
  va_surface_recycle_cb_ = BindToCurrentLoop(base::BindRepeating(
      &VaapiVideoDecodeAccelerator::RecycleVASurface, weak_this_));
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "media::VaapiVideoDecodeAccelerator",
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

VaapiVideoDecodeAccelerator::~VaapiVideoDecodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool VaapiVideoDecodeAccelerator::Initialize(const Config& config,
                                             Client* client) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  vaapi_picture_factory_ = std::make_unique<VaapiPictureFactory>();

  if (config.is_encrypted()) {
    NOTREACHED() << "Encrypted streams are not supported for this VDA";
    return false;
  }

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  VideoCodecProfile profile = config.profile;

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kUninitialized);
  VLOGF(2) << "Initializing VAVDA, profile: " << GetProfileName(profile);

  vaapi_wrapper_ = VaapiWrapper::CreateForVideoCodec(
      VaapiWrapper::kDecode, profile, EncryptionScheme::kUnencrypted,
      base::BindRepeating(&ReportVaapiErrorToUMA,
                          "Media.VaapiVideoDecodeAccelerator.VAAPIError"),
      /*enforce_sequence_affinity=*/false);

  UMA_HISTOGRAM_BOOLEAN("Media.VAVDA.VaapiWrapperCreationSuccess",
                        vaapi_wrapper_.get());
  if (!vaapi_wrapper_.get()) {
    VLOGF(1) << "Failed initializing VAAPI for profile "
             << GetProfileName(profile);
    return false;
  }

  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    auto accelerator =
        std::make_unique<H264VaapiVideoDecoderDelegate>(this, vaapi_wrapper_);
    decoder_delegate_ = accelerator.get();
    decoder_.reset(new H264Decoder(std::move(accelerator), profile,
                                   config.container_color_space));
  } else if (profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX) {
    auto accelerator =
        std::make_unique<VP8VaapiVideoDecoderDelegate>(this, vaapi_wrapper_);
    decoder_delegate_ = accelerator.get();
    decoder_.reset(new VP8Decoder(std::move(accelerator)));
  } else if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    auto accelerator =
        std::make_unique<VP9VaapiVideoDecoderDelegate>(this, vaapi_wrapper_);
    decoder_delegate_ = accelerator.get();
    decoder_.reset(new VP9Decoder(std::move(accelerator), profile,
                                  config.container_color_space));
  } else {
    VLOGF(1) << "Unsupported profile " << GetProfileName(profile);
    return false;
  }

  CHECK(decoder_thread_.Start());
  decoder_thread_task_runner_ = decoder_thread_.task_runner();

  state_ = kIdle;
  profile_ = profile;
  output_mode_ = config.output_mode;
  buffer_allocation_mode_ = DecideBufferAllocationMode();
  previously_requested_num_reference_frames_ = 0;
  return true;
}

void VaapiVideoDecodeAccelerator::OutputPicture(
    scoped_refptr<VASurface> va_surface,
    int32_t input_id,
    gfx::Rect visible_rect,
    const VideoColorSpace& picture_color_space) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  const VASurfaceID va_surface_id = va_surface->id();

  VaapiPicture* picture = nullptr;
  {
    base::AutoLock auto_lock(lock_);
    int32_t picture_buffer_id = available_picture_buffers_.front();
    if (buffer_allocation_mode_ == BufferAllocationMode::kNone) {
      // Find the |pictures_| entry matching |va_surface_id|.
      for (const auto& id_and_picture : pictures_) {
        if (id_and_picture.second->va_surface_id() == va_surface_id) {
          picture_buffer_id = id_and_picture.first;
          break;
        }
      }
    }
    picture = pictures_[picture_buffer_id].get();
    DCHECK(base::Contains(available_picture_buffers_, picture_buffer_id));
    base::Erase(available_picture_buffers_, picture_buffer_id);
  }

  DCHECK(picture) << " could not find " << va_surface_id << " available";
  const int32_t output_id = picture->picture_buffer_id();

  DVLOGF(4) << "Outputting VASurface " << va_surface->id()
            << " into pixmap bound to picture buffer id " << output_id;

  if (buffer_allocation_mode_ != BufferAllocationMode::kNone) {
    TRACE_EVENT2("media,gpu", "VAVDA::DownloadFromSurface", "input_id",
                 input_id, "output_id", output_id);
    RETURN_AND_NOTIFY_ON_FAILURE(picture->DownloadFromSurface(va_surface),
                                 "Failed putting surface into pixmap",
                                 PLATFORM_FAILURE, );
  }

  {
    base::AutoLock auto_lock(lock_);
    TRACE_COUNTER_ID2("media,gpu", "Vaapi frames at client", this, "used",
                      pictures_.size() - available_picture_buffers_.size(),
                      "available", available_picture_buffers_.size());
  }

  DVLOGF(4) << "Notifying output picture id " << output_id << " for input "
            << input_id
            << " is ready. visible rect: " << visible_rect.ToString();
  if (!client_)
    return;

  Picture client_picture(output_id, input_id, visible_rect,
                         picture_color_space.ToGfxColorSpace(),
                         picture->AllowOverlay());
  client_picture.set_read_lock_fences_enabled(true);
  // Notify the |client_| a picture is ready to be consumed.
  client_->PictureReady(client_picture);
}

void VaapiVideoDecodeAccelerator::TryOutputPicture() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Handle Destroy() arriving while pictures are queued for output.
  if (!client_)
    return;

  {
    base::AutoLock auto_lock(lock_);
    if (pending_output_cbs_.empty() || available_picture_buffers_.empty())
      return;
  }

  auto output_cb = std::move(pending_output_cbs_.front());
  pending_output_cbs_.pop();
  std::move(output_cb).Run();

  if (finish_flush_pending_ && pending_output_cbs_.empty())
    FinishFlush();
}

void VaapiVideoDecodeAccelerator::QueueInputBuffer(
    scoped_refptr<DecoderBuffer> buffer,
    int32_t bitstream_id) {
  DVLOGF(4) << "Queueing new input buffer id: " << bitstream_id
            << " size: " << (buffer->end_of_stream() ? 0 : buffer->data_size());
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media,gpu", "QueueInputBuffer", "input_id", bitstream_id);

  base::AutoLock auto_lock(lock_);
  if (buffer->end_of_stream()) {
    auto flush_buffer = std::make_unique<InputBuffer>();
    DCHECK(flush_buffer->IsFlushRequest());
    input_buffers_.push(std::move(flush_buffer));
  } else {
    auto input_buffer = std::make_unique<InputBuffer>(
        bitstream_id, std::move(buffer),
        BindToCurrentLoop(
            base::BindOnce(&Client::NotifyEndOfBitstreamBuffer, client_)));
    input_buffers_.push(std::move(input_buffer));
  }

  TRACE_COUNTER1("media,gpu", "Vaapi input buffers", input_buffers_.size());
  input_ready_.Signal();

  switch (state_) {
    case kIdle:
      state_ = kDecoding;
      decoder_thread_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VaapiVideoDecodeAccelerator::DecodeTask,
                                    base::Unretained(this)));
      break;

    case kDecoding:
      // Decoder already running.
      break;

    case kResetting:
      // When resetting, allow accumulating bitstream buffers, so that
      // the client can queue after-seek-buffers while we are finishing with
      // the before-seek one.
      break;

    default:
      LOG(ERROR) << "Decode/Flush request from client in invalid state: "
                 << state_;
      NotifyError(PLATFORM_FAILURE);
      break;
  }
}

bool VaapiVideoDecodeAccelerator::GetCurrInputBuffer_Locked() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  if (curr_input_buffer_.get())
    return true;

  // Will only wait if it is expected that in current state new buffers will
  // be queued from the client via Decode(). The state can change during wait.
  while (input_buffers_.empty() && (state_ == kDecoding || state_ == kIdle))
    input_ready_.Wait();

  // We could have got woken up in a different state or never got to sleep
  // due to current state.
  if (state_ != kDecoding && state_ != kIdle)
    return false;

  DCHECK(!input_buffers_.empty());
  curr_input_buffer_ = std::move(input_buffers_.front());
  input_buffers_.pop();
  TRACE_COUNTER1("media,gpu", "Vaapi input buffers", input_buffers_.size());

  if (curr_input_buffer_->IsFlushRequest()) {
    DVLOGF(4) << "New flush buffer";
    return true;
  }

  DVLOGF(4) << "New |curr_input_buffer_|, id: " << curr_input_buffer_->id()
            << " size: " << curr_input_buffer_->buffer()->data_size() << "B";
  decoder_->SetStream(curr_input_buffer_->id(), *curr_input_buffer_->buffer());
  return true;
}

void VaapiVideoDecodeAccelerator::ReturnCurrInputBuffer_Locked() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();
  DCHECK(curr_input_buffer_.get());
  curr_input_buffer_.reset();
}

// TODO(posciak): refactor the whole class to remove sleeping in wait for
// surfaces, and reschedule DecodeTask instead.
bool VaapiVideoDecodeAccelerator::WaitForSurfaces_Locked() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  lock_.AssertAcquired();

  while (available_va_surfaces_.empty() &&
         (state_ == kDecoding || state_ == kIdle)) {
    surfaces_available_.Wait();
  }

  return state_ == kDecoding || state_ == kIdle;
}

void VaapiVideoDecodeAccelerator::DecodeTask() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  if (state_ != kDecoding)
    return;
  DVLOGF(4) << "Decode task";

  // Try to decode what stream data is (still) in the decoder until we run out
  // of it.
  while (GetCurrInputBuffer_Locked()) {
    DCHECK(curr_input_buffer_.get());

    if (curr_input_buffer_->IsFlushRequest()) {
      FlushTask();
      break;
    }

    AcceleratedVideoDecoder::DecodeResult res;
    {
      // We are OK releasing the lock here, as decoder never calls our methods
      // directly and we will reacquire the lock before looking at state again.
      // This is the main decode function of the decoder and while keeping
      // the lock for its duration would be fine, it would defeat the purpose
      // of having a separate decoder thread.
      base::AutoUnlock auto_unlock(lock_);
      TRACE_EVENT0("media,gpu", "VAVDA::Decode");
      res = decoder_->Decode();
    }

    switch (res) {
      case AcceleratedVideoDecoder::kConfigChange: {
        const uint8_t bit_depth = decoder_->GetBitDepth();
        RETURN_AND_NOTIFY_ON_FAILURE(
            bit_depth == 8u,
            "Unsupported bit depth: " << base::strict_cast<int>(bit_depth),
            PLATFORM_FAILURE, );
        // The visible rect should be a subset of the picture size. Otherwise,
        // the encoded stream is bad.
        const gfx::Size pic_size = decoder_->GetPicSize();
        const gfx::Rect visible_rect = decoder_->GetVisibleRect();
        RETURN_AND_NOTIFY_ON_FAILURE(
            gfx::Rect(pic_size).Contains(visible_rect),
            "The visible rectangle is not contained by the picture size",
            UNREADABLE_INPUT, );
        VLOGF(2) << "Decoder requesting a new set of surfaces";
        size_t required_num_of_pictures = decoder_->GetRequiredNumOfPictures();
        if (buffer_allocation_mode_ == BufferAllocationMode::kNone &&
            profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX) {
          // For H.264, the decoder might request too few pictures. In
          // BufferAllocationMode::kNone, this can cause us to do a lot of busy
          // work waiting for picture buffers to come back from the client (see
          // crbug.com/910986#c32). This is a workaround to increase the
          // likelihood that we don't have to wait on buffers to come back from
          // the client. |kNumOfPics| is picked to mirror the value returned by
          // VP9Decoder::GetRequiredNumOfPictures().
          constexpr size_t kMinNumOfPics = 13u;
          required_num_of_pictures =
              std::max(kMinNumOfPics, required_num_of_pictures);
        }

        // Notify |decoder_delegate_| of an imminent VAContextID destruction, so
        // it can destroy any internal structures making use of it.
        decoder_delegate_->OnVAContextDestructionSoon();

        task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(
                &VaapiVideoDecodeAccelerator::InitiateSurfaceSetChange,
                weak_this_, required_num_of_pictures, pic_size,
                decoder_->GetNumReferenceFrames(), visible_rect));
        // We'll get rescheduled once ProvidePictureBuffers() finishes.
        return;
      }
      case AcceleratedVideoDecoder::kRanOutOfStreamData:
        ReturnCurrInputBuffer_Locked();
        break;

      case AcceleratedVideoDecoder::kRanOutOfSurfaces:
        // No more output buffers in the decoder, try getting more or go to
        // sleep waiting for them.
        if (!WaitForSurfaces_Locked())
          return;

        break;

      case AcceleratedVideoDecoder::kNeedContextUpdate:
        // This should not happen as we return false from
        // NeedsCompressedHeaderParsed().
        NOTREACHED() << "Context updates not supported";
        return;

      case AcceleratedVideoDecoder::kDecodeError:
        RETURN_AND_NOTIFY_ON_FAILURE(false, "Error decoding stream",
                                     PLATFORM_FAILURE, );
        return;

      case AcceleratedVideoDecoder::kTryAgain:
        NOTREACHED() << "Should not reach here unless this class accepts "
                        "encrypted streams.";
        RETURN_AND_NOTIFY_ON_FAILURE(false, "Error decoding stream",
                                     PLATFORM_FAILURE, );
        return;
    }
  }
}

void VaapiVideoDecodeAccelerator::InitiateSurfaceSetChange(
    size_t num_pics,
    gfx::Size size,
    size_t num_reference_frames,
    const gfx::Rect& visible_rect) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!awaiting_va_surfaces_recycle_);
  DCHECK_GT(num_pics, num_reference_frames);

  // At this point decoder has stopped running and has already posted onto our
  // loop any remaining output request callbacks, which executed before we got
  // here. Some of them might have been pended though, because we might not have
  // had enough PictureBuffers to output surfaces to. Initiate a wait cycle,
  // which will wait for client to return enough PictureBuffers to us, so that
  // we can finish all pending output callbacks, releasing associated surfaces.
  awaiting_va_surfaces_recycle_ = true;

  requested_pic_size_ = size;
  requested_visible_rect_ = visible_rect;
  if (buffer_allocation_mode_ == BufferAllocationMode::kSuperReduced) {
    // Add one to the reference frames for the one being currently egressed.
    requested_num_reference_frames_ = num_reference_frames + 1;
    requested_num_pics_ = num_pics - num_reference_frames;
  } else if (buffer_allocation_mode_ == BufferAllocationMode::kReduced) {
    // Add one to the reference frames for the one being currently egressed,
    // and an extra allocation for both |client_| and |decoder_|.
    requested_num_reference_frames_ = num_reference_frames + 2;
    requested_num_pics_ = num_pics - num_reference_frames + 1;
  } else {
    requested_num_reference_frames_ = 0;
    requested_num_pics_ = num_pics + num_extra_pics_;
  }

  VLOGF(2) << " |requested_num_pics_| = " << requested_num_pics_
           << "; |requested_num_reference_frames_| = "
           << requested_num_reference_frames_;

  TryFinishSurfaceSetChange();
}

void VaapiVideoDecodeAccelerator::TryFinishSurfaceSetChange() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!awaiting_va_surfaces_recycle_)
    return;

  base::AutoLock auto_lock(lock_);
  const size_t expected_max_available_va_surfaces =
      IsBufferAllocationModeReducedOrSuperReduced()
          ? previously_requested_num_reference_frames_
          : pictures_.size();
  if (!pending_output_cbs_.empty() ||
      expected_max_available_va_surfaces != available_va_surfaces_.size()) {
    // If we're here the stream resolution has changed; we need to wait until:
    // - all |pending_output_cbs_| have been executed
    // - all VASurfaces are back to |available_va_surfaces_|; we can't use
    //   |requested_num_reference_frames_| for comparison, since it might have
    //   changed in the previous call to InitiateSurfaceSetChange(), so we use
    //   |previously_requested_num_reference_frames_| instead.
    DVLOGF(2) << "Awaiting pending output/surface release callbacks to finish";
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VaapiVideoDecodeAccelerator::TryFinishSurfaceSetChange,
                       weak_this_));
    return;
  }

  previously_requested_num_reference_frames_ = requested_num_reference_frames_;

  // All surfaces released, destroy them and dismiss all PictureBuffers.
  awaiting_va_surfaces_recycle_ = false;

  const VideoCodecProfile new_profile = decoder_->GetProfile();
  if (profile_ != new_profile) {
    profile_ = new_profile;
    auto new_vaapi_wrapper = VaapiWrapper::CreateForVideoCodec(
        VaapiWrapper::kDecode, profile_, EncryptionScheme::kUnencrypted,
        base::BindRepeating(&ReportVaapiErrorToUMA,
                            "Media.VaapiVideoDecodeAccelerator.VAAPIError"),
        /*enforce_sequence_affinity=*/false);
    RETURN_AND_NOTIFY_ON_FAILURE(new_vaapi_wrapper.get(),
                                 "Failed creating VaapiWrapper",
                                 INVALID_ARGUMENT, );
    decoder_delegate_->set_vaapi_wrapper(new_vaapi_wrapper.get());
    vaapi_wrapper_ = std::move(new_vaapi_wrapper);
  } else {
    vaapi_wrapper_->DestroyContext();
  }

  available_va_surfaces_.clear();

  for (auto iter = pictures_.begin(); iter != pictures_.end(); ++iter) {
    VLOGF(2) << "Dismissing picture id: " << iter->first;
    if (client_)
      client_->DismissPictureBuffer(iter->first);
  }
  pictures_.clear();

  // And ask for a new set as requested.
  VLOGF(2) << "Requesting " << requested_num_pics_
           << " pictures of size: " << requested_pic_size_.ToString()
           << " and visible rectangle = " << requested_visible_rect_.ToString();

  const absl::optional<VideoPixelFormat> format =
      GfxBufferFormatToVideoPixelFormat(
          vaapi_picture_factory_->GetBufferFormat());
  CHECK(format);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Client::ProvidePictureBuffersWithVisibleRect,
                                client_, requested_num_pics_, *format, 1,
                                requested_pic_size_, requested_visible_rect_,
                                vaapi_picture_factory_->GetGLTextureTarget()));
  // |client_| may respond via AssignPictureBuffers().
}

void VaapiVideoDecodeAccelerator::Decode(BitstreamBuffer bitstream_buffer) {
  Decode(bitstream_buffer.ToDecoderBuffer(), bitstream_buffer.id());
}

void VaapiVideoDecodeAccelerator::Decode(scoped_refptr<DecoderBuffer> buffer,
                                         int32_t bitstream_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media,gpu", "VAVDA::Decode", "Buffer id", bitstream_id);

  if (bitstream_id < 0) {
    LOG(ERROR) << "Invalid bitstream_buffer, id: " << bitstream_id;
    NotifyError(INVALID_ARGUMENT);
    return;
  }

  if (!buffer) {
    if (client_)
      client_->NotifyEndOfBitstreamBuffer(bitstream_id);
    return;
  }

  QueueInputBuffer(std::move(buffer), bitstream_id);
}

void VaapiVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  DCHECK(pictures_.empty());

  available_picture_buffers_.clear();

  RETURN_AND_NOTIFY_ON_FAILURE(
      buffers.size() >= requested_num_pics_,
      "Got an invalid number of picture buffers. (Got " << buffers.size()
      << ", requested " << requested_num_pics_ << ")", INVALID_ARGUMENT, );
  // requested_pic_size_ can be adjusted by VDA client. We should update
  // |requested_pic_size_| by buffers[0].size(). But AMD driver doesn't decode
  // frames correctly if the surface stride is different from the width of a
  // coded size.
  // TODO(b/139460315): Save buffers[0].size() as |adjusted_size_| once the
  // AMD driver issue is resolved.

  va_surface_format_ = GetVaFormatForVideoCodecProfile(profile_);
  std::vector<VASurfaceID> va_surface_ids;
  scoped_refptr<VaapiWrapper> vaapi_wrapper_for_picture = vaapi_wrapper_;

  const bool requires_vpp =
      vaapi_picture_factory_->NeedsProcessingPipelineForDownloading();
  // If we aren't in BufferAllocationMode::kNone mode and the VaapiPicture
  // implementation we get from |vaapi_picture_factory_| requires the video
  // processing pipeline for downloading the decoded frame from the internal
  // surface, we need to create a |vpp_vaapi_wrapper_|.
  if (requires_vpp && buffer_allocation_mode_ != BufferAllocationMode::kNone) {
    if (!vpp_vaapi_wrapper_) {
      vpp_vaapi_wrapper_ = VaapiWrapper::Create(
          VaapiWrapper::kVideoProcess, VAProfileNone,
          EncryptionScheme::kUnencrypted,
          base::BindRepeating(
              &ReportVaapiErrorToUMA,
              "Media.VaapiVideoDecodeAccelerator.Vpp.VAAPIError"),
          /*enforce_sequence_affinity=*/false);
      RETURN_AND_NOTIFY_ON_FAILURE(vpp_vaapi_wrapper_,
                                   "Failed to initialize VppVaapiWrapper",
                                   PLATFORM_FAILURE, );
      // Size is irrelevant for a VPP context.
      RETURN_AND_NOTIFY_ON_FAILURE(
          vpp_vaapi_wrapper_->CreateContext(gfx::Size()),
          "Failed to create Context", PLATFORM_FAILURE, );
    }
    vaapi_wrapper_for_picture = vpp_vaapi_wrapper_;
  }

  for (size_t i = 0; i < buffers.size(); ++i) {
    // TODO(b/139460315): Create with buffers[i] once the AMD driver issue is
    // resolved.
    PictureBuffer buffer = buffers[i];
    buffer.set_size(requested_pic_size_);

    // Note that the |size_to_bind| is not relevant in IMPORT mode.
    const gfx::Size size_to_bind =
        (output_mode_ == Config::OutputMode::ALLOCATE)
            ? GetRectSizeFromOrigin(requested_visible_rect_)
            : gfx::Size();

    std::unique_ptr<VaapiPicture> picture = vaapi_picture_factory_->Create(
        vaapi_wrapper_for_picture, make_context_current_cb_, bind_image_cb_,
        buffer, size_to_bind);
    RETURN_AND_NOTIFY_ON_FAILURE(picture, "Failed creating a VaapiPicture",
                                 PLATFORM_FAILURE, );

    if (output_mode_ == Config::OutputMode::ALLOCATE) {
      RETURN_AND_NOTIFY_ON_STATUS(
          picture->Allocate(vaapi_picture_factory_->GetBufferFormat()), );

      available_picture_buffers_.push_back(buffers[i].id());
      VASurfaceID va_surface_id = picture->va_surface_id();
      if (va_surface_id != VA_INVALID_ID)
        va_surface_ids.push_back(va_surface_id);
    }

    DCHECK(!base::Contains(pictures_, buffers[i].id()));
    pictures_[buffers[i].id()] = std::move(picture);

    surfaces_available_.Signal();
  }

  base::RepeatingCallback<void(VASurfaceID)> va_surface_release_cb;

  // If we aren't in BufferAllocationMode::kNone, we use |va_surface_ids| for
  // decode, otherwise ask |vaapi_wrapper_| to allocate them for us.
  if (buffer_allocation_mode_ == BufferAllocationMode::kNone) {
    DCHECK(!va_surface_ids.empty());
    RETURN_AND_NOTIFY_ON_FAILURE(
        vaapi_wrapper_->CreateContext(requested_pic_size_),
        "Failed creating VA Context", PLATFORM_FAILURE, );
    DCHECK_EQ(va_surface_ids.size(), buffers.size());

    va_surface_release_cb = base::DoNothing();
  } else {
    const size_t requested_num_surfaces =
        IsBufferAllocationModeReducedOrSuperReduced()
            ? requested_num_reference_frames_
            : pictures_.size();
    CHECK_NE(requested_num_surfaces, 0u);
    va_surface_ids.clear();

    RETURN_AND_NOTIFY_ON_FAILURE(
        vaapi_wrapper_->CreateContextAndSurfaces(
            va_surface_format_, requested_pic_size_,
            {VaapiWrapper::SurfaceUsageHint::kVideoDecoder},
            requested_num_surfaces, &va_surface_ids),
        "Failed creating VA Surfaces", PLATFORM_FAILURE, );

    va_surface_release_cb =
        base::BindRepeating(&VaapiWrapper::DestroySurface, vaapi_wrapper_);
  }

  for (const VASurfaceID va_surface_id : va_surface_ids) {
    available_va_surfaces_.emplace_back(std::make_unique<ScopedVASurfaceID>(
        va_surface_id, va_surface_release_cb));
  }

  // Resume DecodeTask if it is still in decoding state.
  if (state_ == kDecoding) {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiVideoDecodeAccelerator::DecodeTask,
                                  base::Unretained(this)));
  }
}

#if BUILDFLAG(IS_OZONE)
void VaapiVideoDecodeAccelerator::ImportBufferForPicture(
    int32_t picture_buffer_id,
    VideoPixelFormat pixel_format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  VLOGF(2) << "Importing picture id: " << picture_buffer_id;
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (output_mode_ != Config::OutputMode::IMPORT) {
    LOG(ERROR) << "Cannot import in non-import mode";
    NotifyError(INVALID_ARGUMENT);
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    if (!pictures_.count(picture_buffer_id)) {
      // It's possible that we've already posted a DismissPictureBuffer for this
      // picture, but it has not yet executed when this ImportBufferForPicture
      // was posted to us by the client. In that case just ignore this (we've
      // already dismissed it and accounted for that).
      DVLOGF(3) << "got picture id=" << picture_buffer_id
                << " not in use (anymore?).";
      return;
    }

    auto buffer_format = VideoPixelFormatToGfxBufferFormat(pixel_format);
    if (!buffer_format) {
      LOG(ERROR) << "Unsupported format: " << pixel_format;
      NotifyError(INVALID_ARGUMENT);
      return;
    }

    VaapiPicture* picture = pictures_[picture_buffer_id].get();
    if (!picture->ImportGpuMemoryBufferHandle(
            *buffer_format, std::move(gpu_memory_buffer_handle))) {
      // ImportGpuMemoryBufferHandle will close the handles even on failure, so
      // we don't need to do this ourselves.
      LOG(ERROR) << "Failed to import GpuMemoryBufferHandle";
      NotifyError(PLATFORM_FAILURE);
      return;
    }
  }

  ReusePictureBuffer(picture_buffer_id);
}
#endif

void VaapiVideoDecodeAccelerator::ReusePictureBuffer(
    int32_t picture_buffer_id) {
  DVLOGF(4) << "picture id=" << picture_buffer_id;
  DCHECK(task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("media,gpu", "VAVDA::ReusePictureBuffer", "Picture id",
               picture_buffer_id);

  {
    base::AutoLock auto_lock(lock_);

    if (!pictures_.count(picture_buffer_id)) {
      // It's possible that we've already posted a DismissPictureBuffer for this
      // picture, but it has not yet executed when this ReusePictureBuffer
      // was posted to us by the client. In that case just ignore this (we've
      // already dismissed it and accounted for that).
      DVLOGF(3) << "got picture id=" << picture_buffer_id
                << " not in use (anymore?).";
      return;
    }

    available_picture_buffers_.push_back(picture_buffer_id);
    TRACE_COUNTER_ID2("media,gpu", "Vaapi frames at client", this, "used",
                      pictures_.size() - available_picture_buffers_.size(),
                      "available", available_picture_buffers_.size());
  }

  TryOutputPicture();
}

void VaapiVideoDecodeAccelerator::FlushTask() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK(curr_input_buffer_ && curr_input_buffer_->IsFlushRequest());

  curr_input_buffer_.reset();

  // First flush all the pictures that haven't been outputted, notifying the
  // client to output them.
  bool res = decoder_->Flush();
  RETURN_AND_NOTIFY_ON_FAILURE(res, "Failed flushing the decoder.",
                               PLATFORM_FAILURE, );

  // Put the decoder in idle state, ready to resume.
  decoder_->Reset();

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiVideoDecodeAccelerator::FinishFlush, weak_this_));
}

void VaapiVideoDecodeAccelerator::Flush() {
  VLOGF(2) << "Got flush request";
  DCHECK(task_runner_->BelongsToCurrentThread());

  QueueInputBuffer(DecoderBuffer::CreateEOSBuffer(), -1);
}

void VaapiVideoDecodeAccelerator::FinishFlush() {
  VLOGF(2);
  DCHECK(task_runner_->BelongsToCurrentThread());

  finish_flush_pending_ = false;

  base::AutoLock auto_lock(lock_);
  if (state_ != kDecoding) {
    DCHECK(state_ == kDestroying || state_ == kResetting) << state_;
    return;
  }

  // Still waiting for textures from client to finish outputting all pending
  // frames. Try again later.
  if (!pending_output_cbs_.empty()) {
    finish_flush_pending_ = true;
    return;
  }

  // Resume decoding if necessary.
  if (input_buffers_.empty()) {
    state_ = kIdle;
  } else {
    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiVideoDecodeAccelerator::DecodeTask,
                                  base::Unretained(this)));
  }

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&Client::NotifyFlushDone, client_));
}

void VaapiVideoDecodeAccelerator::ResetTask() {
  VLOGF(2);
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());

  // All the decoding tasks from before the reset request from client are done
  // by now, as this task was scheduled after them and client is expected not
  // to call Decode() after Reset() and before NotifyResetDone.
  decoder_->Reset();

  base::AutoLock auto_lock(lock_);

  // Return current input buffer, if present.
  if (curr_input_buffer_)
    ReturnCurrInputBuffer_Locked();

  // And let client know that we are done with reset.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiVideoDecodeAccelerator::FinishReset, weak_this_));
}

void VaapiVideoDecodeAccelerator::Reset() {
  VLOGF(2) << "Got reset request";
  DCHECK(task_runner_->BelongsToCurrentThread());

  // This will make any new decode tasks exit early.
  base::AutoLock auto_lock(lock_);
  state_ = kResetting;
  finish_flush_pending_ = false;

  // Drop all remaining input buffers, if present.
  while (!input_buffers_.empty())
    input_buffers_.pop();
  TRACE_COUNTER1("media,gpu", "Vaapi input buffers", input_buffers_.size());

  decoder_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiVideoDecodeAccelerator::ResetTask,
                                base::Unretained(this)));

  input_ready_.Signal();
  surfaces_available_.Signal();
}

void VaapiVideoDecodeAccelerator::FinishReset() {
  VLOGF(2);
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  if (state_ != kResetting) {
    DCHECK(state_ == kDestroying || state_ == kUninitialized) << state_;
    return;  // We could've gotten destroyed already.
  }

  // Drop pending outputs.
  while (!pending_output_cbs_.empty())
    pending_output_cbs_.pop();

  if (awaiting_va_surfaces_recycle_) {
    // Decoder requested a new surface set while we were waiting for it to
    // finish the last DecodeTask, running at the time of Reset().
    // Let the surface set change finish first before resetting.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VaapiVideoDecodeAccelerator::FinishReset, weak_this_));
    return;
  }

  state_ = kIdle;

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&Client::NotifyResetDone, client_));

  // The client might have given us new buffers via Decode() while we were
  // resetting and might be waiting for our move, and not call Decode() anymore
  // until we return something. Post a DecodeTask() so that we won't
  // sleep forever waiting for Decode() in that case. Having two of them
  // in the pipe is harmless, the additional one will return as soon as it sees
  // that we are back in kDecoding state.
  if (!input_buffers_.empty()) {
    state_ = kDecoding;
    decoder_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiVideoDecodeAccelerator::DecodeTask,
                                  base::Unretained(this)));
  }
}

void VaapiVideoDecodeAccelerator::Cleanup() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(lock_);
  if (state_ == kUninitialized || state_ == kDestroying)
    return;

  VLOGF(2) << "Destroying VAVDA";
  state_ = kDestroying;

  // Call DismissPictureBuffer() to notify |client_| that the picture buffers
  // are no longer used and thus |client_| shall release them. If |client_| has
  // been invalidated in NotifyError(),|client_| will be destroyed shortly. The
  // destruction should release all the PictureBuffers.
  if (client_) {
    for (const auto& id_and_picture : pictures_)
      client_->DismissPictureBuffer(id_and_picture.first);
  }
  pictures_.clear();

  client_ptr_factory_.reset();
  weak_this_factory_.InvalidateWeakPtrs();

  // TODO(mcasas): consider deleting |decoder_| on
  // |decoder_thread_task_runner_|, https://crbug.com/789160.

  // Signal all potential waiters on the decoder_thread_, let them early-exit,
  // as we've just moved to the kDestroying state, and wait for all tasks
  // to finish.
  input_ready_.Signal();
  surfaces_available_.Signal();
  {
    base::AutoUnlock auto_unlock(lock_);
    decoder_thread_.Stop();
  }
  if (buffer_allocation_mode_ != BufferAllocationMode::kNone)
    available_va_surfaces_.clear();

  // Notify |decoder_delegate_| of an imminent VAContextID destruction, so it
  // can destroy any internal structures making use of it. At this point
  // |decoder_thread_| is stopped so we can access |decoder_delegate_| from
  // |task_runner_|.
  decoder_delegate_->OnVAContextDestructionSoon();
  vaapi_wrapper_->DestroyContext();

  if (vpp_vaapi_wrapper_)
    vpp_vaapi_wrapper_->DestroyContext();
  state_ = kUninitialized;
}

void VaapiVideoDecodeAccelerator::Destroy() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  Cleanup();
  delete this;
}

bool VaapiVideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  return false;
}

void VaapiVideoDecodeAccelerator::SurfaceReady(
    scoped_refptr<VASurface> dec_surface,
    int32_t bitstream_id,
    const gfx::Rect& visible_rect,
    const VideoColorSpace& color_space) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiVideoDecodeAccelerator::SurfaceReady,
                                  weak_this_, std::move(dec_surface),
                                  bitstream_id, visible_rect, color_space));
    return;
  }

  DCHECK(!awaiting_va_surfaces_recycle_);

  {
    base::AutoLock auto_lock(lock_);
    // Drop any requests to output if we are resetting or being destroyed.
    if (state_ == kResetting || state_ == kDestroying)
      return;
  }
  pending_output_cbs_.push(base::BindOnce(
      &VaapiVideoDecodeAccelerator::OutputPicture, weak_this_,
      std::move(dec_surface), bitstream_id, visible_rect, color_space));

  TryOutputPicture();
}

scoped_refptr<VASurface> VaapiVideoDecodeAccelerator::CreateSurface() {
  DCHECK(decoder_thread_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);

  if (available_va_surfaces_.empty())
    return nullptr;

  DCHECK_NE(VA_INVALID_ID, va_surface_format_);
  DCHECK(!awaiting_va_surfaces_recycle_);
  if (buffer_allocation_mode_ != BufferAllocationMode::kNone) {
    auto va_surface_id = std::move(available_va_surfaces_.front());
    const VASurfaceID id = va_surface_id->id();
    available_va_surfaces_.pop_front();

    TRACE_COUNTER_ID2("media,gpu", "Vaapi VASurfaceIDs", this, "used",
                      (IsBufferAllocationModeReducedOrSuperReduced()
                           ? requested_num_reference_frames_
                           : pictures_.size()) -
                          available_va_surfaces_.size(),
                      "available", available_va_surfaces_.size());

    return new VASurface(
        id, requested_pic_size_, va_surface_format_,
        base::BindOnce(va_surface_recycle_cb_, std::move(va_surface_id)));
  }

  // Find the first |available_va_surfaces_| id such that the associated
  // |pictures_| entry is marked as |available_picture_buffers_|. In practice,
  // we will quickly find an available |va_surface_id|.
  for (auto it = available_va_surfaces_.begin();
       it != available_va_surfaces_.end(); ++it) {
    const VASurfaceID va_surface_id = (*it)->id();
    for (const auto& id_and_picture : pictures_) {
      if (id_and_picture.second->va_surface_id() == va_surface_id &&
          base::Contains(available_picture_buffers_, id_and_picture.first)) {
        // Remove |va_surface_id| from the list of availables, and use the id
        // to return a new VASurface.
        auto va_surface = std::move(*it);
        available_va_surfaces_.erase(it);
        return new VASurface(
            va_surface_id, requested_pic_size_, va_surface_format_,
            base::BindOnce(va_surface_recycle_cb_, std::move(va_surface)));
      }
    }
  }
  return nullptr;
}

void VaapiVideoDecodeAccelerator::RecycleVASurface(
    std::unique_ptr<ScopedVASurfaceID> va_surface,
    // We don't use |va_surface_id| but it must be here because this method is
    // bound as VASurface::ReleaseCB.
    VASurfaceID /*va_surface_id*/) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock auto_lock(lock_);
    available_va_surfaces_.push_back(std::move(va_surface));

    if (buffer_allocation_mode_ != BufferAllocationMode::kNone) {
      TRACE_COUNTER_ID2("media,gpu", "Vaapi VASurfaceIDs", this, "used",
                        (IsBufferAllocationModeReducedOrSuperReduced()
                             ? requested_num_reference_frames_
                             : pictures_.size()) -
                            available_va_surfaces_.size(),
                        "available", available_va_surfaces_.size());
    }
    surfaces_available_.Signal();
  }

  TryOutputPicture();
}

// static
VideoDecodeAccelerator::SupportedProfiles
VaapiVideoDecodeAccelerator::GetSupportedProfiles() {
  VideoDecodeAccelerator::SupportedProfiles profiles =
      VaapiWrapper::GetSupportedDecodeProfiles();
  // VaVDA never supported VP9 Profile 2, AV1 and HEVC, but VaapiWrapper does.
  // Filter them out.
  base::EraseIf(profiles, [](const auto& profile) {
    VideoCodec codec = VideoCodecProfileToVideoCodec(profile.profile);
    return profile.profile == VP9PROFILE_PROFILE2 ||
           codec == VideoCodec::kAV1 || codec == VideoCodec::kHEVC;
  });
  return profiles;
}

VaapiVideoDecodeAccelerator::BufferAllocationMode
VaapiVideoDecodeAccelerator::DecideBufferAllocationMode() {
#if BUILDFLAG(USE_VAAPI_X11)
  // The IMPORT mode is used for Android on Chrome OS, so this doesn't apply
  // here.
  DCHECK_NE(output_mode_, VideoDecodeAccelerator::Config::OutputMode::IMPORT);
  // TODO(crbug/1116701): get video decode acceleration working with ozone.
  // For H.264 on older devices, another +1 is experimentally needed for
  // high-to-high resolution changes.
  // TODO(mcasas): Figure out why and why only H264, see crbug.com/912295 and
  // http://crrev.com/c/1363807/9/media/gpu/h264_decoder.cc#1449.
  if (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX)
    return BufferAllocationMode::kReduced;
  return BufferAllocationMode::kSuperReduced;
#else
  // TODO(crbug.com/912295): Enable a better BufferAllocationMode for IMPORT
  // |output_mode_| as well.
  if (output_mode_ == VideoDecodeAccelerator::Config::OutputMode::IMPORT)
    return BufferAllocationMode::kNormal;

  // On Gemini Lake, Kaby Lake and later we can pass to libva the client's
  // PictureBuffers to decode onto, which skips the use of the Vpp unit and its
  // associated format reconciliation copy, avoiding all internal buffer
  // allocations.
  // TODO(crbug.com/911754): Enable for VP9 Profile 2.
  if (IsGeminiLakeOrLater() &&
      (profile_ == VP9PROFILE_PROFILE0 || profile_ == VP8PROFILE_ANY ||
       (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX))) {
    // Add one to the reference frames for the one being currently egressed, and
    // an extra allocation for both |client_| and |decoder_|, see
    // crrev.com/c/1576560.
    if (profile_ == VP8PROFILE_ANY)
      num_extra_pics_ = 3;
    return BufferAllocationMode::kNone;
  }

  // For H.264 on older devices, another +1 is experimentally needed for
  // high-to-high resolution changes.
  // TODO(mcasas): Figure out why and why only H264, see crbug.com/912295 and
  // http://crrev.com/c/1363807/9/media/gpu/h264_decoder.cc#1449.
  if (profile_ >= H264PROFILE_MIN && profile_ <= H264PROFILE_MAX)
    return BufferAllocationMode::kReduced;

  // If we're here, we have to use the Vpp unit and allocate buffers for
  // |decoder_|; usually we'd have to allocate the |decoder_|s
  // GetRequiredNumOfPictures() internally, we can allocate just |decoder_|s
  // GetNumReferenceFrames() + 1. Moreover, we also request the |client_| to
  // allocate less than the usual |decoder_|s GetRequiredNumOfPictures().
  return BufferAllocationMode::kSuperReduced;
#endif
}

bool VaapiVideoDecodeAccelerator::IsBufferAllocationModeReducedOrSuperReduced()
    const {
  return buffer_allocation_mode_ == BufferAllocationMode::kSuperReduced ||
         buffer_allocation_mode_ == BufferAllocationMode::kReduced;
}

bool VaapiVideoDecodeAccelerator::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  base::AutoLock auto_lock(lock_);
  if (buffer_allocation_mode_ == BufferAllocationMode::kNone ||
      !requested_num_reference_frames_) {
    return false;
  }

  auto dump_name = base::StringPrintf("gpu/vaapi/decoder/0x%" PRIxPTR,
                                      reinterpret_cast<uintptr_t>(this));
  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);

  constexpr float kNumBytesPerPixelYUV420 = 12.0 / 8;
  constexpr float kNumBytesPerPixelYUV420_10bpp = 2 * kNumBytesPerPixelYUV420;
  DCHECK(va_surface_format_ == VA_RT_FORMAT_YUV420 ||
         va_surface_format_ == VA_RT_FORMAT_YUV420_10BPP);
  const float va_surface_bytes_per_pixel =
      va_surface_format_ == VA_RT_FORMAT_YUV420 ? kNumBytesPerPixelYUV420
                                                : kNumBytesPerPixelYUV420_10bpp;
  // Report |requested_num_surfaces| and the associated memory size. The
  // calculated size is an estimation since we don't know the internal VA
  // strides, texture compression, headers, etc, but is a good lower boundary.
  const size_t requested_num_surfaces =
      IsBufferAllocationModeReducedOrSuperReduced()
          ? requested_num_reference_frames_
          : pictures_.size();
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(requested_num_surfaces *
                                        requested_pic_size_.GetArea() *
                                        va_surface_bytes_per_pixel));
  dump->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                  MemoryAllocatorDump::kUnitsObjects,
                  static_cast<uint64_t>(requested_num_surfaces));

  return true;
}

}  // namespace media
