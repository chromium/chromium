// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/vda_video_decoder.h"

#include <string.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/async_destroy_video_decoder.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/video/picture.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_image.h"

namespace media {

namespace {

// Generates nonnegative bitstream buffer IDs, which are assumed to be unique.
int32_t NextID(int32_t* counter) {
  int32_t value = *counter;
  *counter = (*counter + 1) & 0x3FFFFFFF;
  return value;
}

scoped_refptr<CommandBufferHelper> CreateCommandBufferHelper(
    VdaVideoDecoder::GetStubCB get_stub_cb) {
  gpu::CommandBufferStub* stub = std::move(get_stub_cb).Run();

  if (!stub) {
    DVLOG(1) << "Failed to obtain command buffer stub";
    return nullptr;
  }

  return CommandBufferHelper::Create(stub);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
bool BindDecoderManagedImage(
    scoped_refptr<CommandBufferHelper> command_buffer_helper,
    uint32_t client_texture_id,
    uint32_t texture_target,
    const scoped_refptr<gl::GLImage>& image) {
  return command_buffer_helper->BindDecoderManagedImage(client_texture_id,
                                                        image.get());
}
#else
bool BindClientManagedImage(
    scoped_refptr<CommandBufferHelper> command_buffer_helper,
    uint32_t client_texture_id,
    uint32_t texture_target,
    const scoped_refptr<gl::GLImage>& image) {
  return command_buffer_helper->BindClientManagedImage(client_texture_id,
                                                       image.get());
}
#endif

std::unique_ptr<VideoDecodeAccelerator> CreateAndInitializeVda(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    scoped_refptr<CommandBufferHelper> command_buffer_helper,
    VideoDecodeAccelerator::Client* client,
    MediaLog* media_log,
    const VideoDecodeAccelerator::Config& config) {
  GpuVideoDecodeGLClient gl_client;
  // |command_buffer_helper| is nullptr in IMPORT mode in which case, we
  // shouldn't need to do any GL calls.
  if (command_buffer_helper) {
    gl_client.get_context = base::BindRepeating(
        &CommandBufferHelper::GetGLContext, command_buffer_helper);
    gl_client.make_context_current = base::BindRepeating(
        &CommandBufferHelper::MakeContextCurrent, command_buffer_helper);
    // The semantics of |bind_image| vary per-platform: On Windows and Mac it
    // must mark the image as needing binding by the decoder, while on other
    // platforms it must mark the image as *not* needing binding by the decoder.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
    gl_client.bind_image =
        base::BindRepeating(&BindDecoderManagedImage, command_buffer_helper);
#else
    gl_client.bind_image =
        base::BindRepeating(&BindClientManagedImage, command_buffer_helper);
#endif
    gl_client.is_passthrough = command_buffer_helper->IsPassthrough();
    gl_client.supports_arb_texture_rectangle =
        command_buffer_helper->SupportsTextureRectangle();
  }

  std::unique_ptr<GpuVideoDecodeAcceleratorFactory> factory =
      GpuVideoDecodeAcceleratorFactory::Create(gl_client);
  // Note: GpuVideoDecodeAcceleratorFactory may create and initialize more than
  // one VDA. It is therefore important that VDAs do not call client methods
  // from Initialize().
  return factory->CreateVDA(client, config, gpu_workarounds, gpu_preferences,
                            media_log);
}

bool IsProfileSupported(
    const VideoDecodeAccelerator::SupportedProfiles& supported_profiles,
    VideoCodecProfile profile,
    gfx::Size coded_size) {
  for (const auto& supported_profile : supported_profiles) {
    if (supported_profile.profile == profile &&
        !supported_profile.encrypted_only &&
        gfx::Rect(supported_profile.max_resolution)
            .Contains(gfx::Rect(coded_size)) &&
        gfx::Rect(coded_size)
            .Contains(gfx::Rect(supported_profile.min_resolution))) {
      return true;
    }
  }
  return false;
}

}  // namespace

// static
std::unique_ptr<VideoDecoder> VdaVideoDecoder::Create(
    scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gfx::ColorSpace& target_color_space,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    GetStubCB get_stub_cb,
    VideoDecodeAccelerator::Config::OutputMode output_mode) {
  auto* decoder = new VdaVideoDecoder(
      std::move(parent_task_runner), std::move(gpu_task_runner),
      std::move(media_log), target_color_space,
      base::BindOnce(&PictureBufferManager::Create,
                     /*allocate_gpu_memory_buffers=*/output_mode ==
                         VideoDecodeAccelerator::Config::OutputMode::IMPORT),
      output_mode == VideoDecodeAccelerator::Config::OutputMode::ALLOCATE
          ? base::BindOnce(&CreateCommandBufferHelper, std::move(get_stub_cb))
          : base::NullCallback(),
      base::BindRepeating(&CreateAndInitializeVda, gpu_preferences,
                          gpu_workarounds),
      GpuVideoAcceleratorUtil::ConvertGpuToMediaDecodeCapabilities(
          GpuVideoDecodeAcceleratorFactory::GetDecoderCapabilities(
              gpu_preferences, gpu_workarounds)),
      output_mode);

  return std::make_unique<AsyncDestroyVideoDecoder<VdaVideoDecoder>>(
      base::WrapUnique(decoder));
}

VdaVideoDecoder::VdaVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> parent_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gfx::ColorSpace& target_color_space,
    CreatePictureBufferManagerCB create_picture_buffer_manager_cb,
    CreateCommandBufferHelperCB create_command_buffer_helper_cb,
    CreateAndInitializeVdaCB create_and_initialize_vda_cb,
    const VideoDecodeAccelerator::Capabilities& vda_capabilities,
    VideoDecodeAccelerator::Config::OutputMode output_mode)
    : parent_task_runner_(std::move(parent_task_runner)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      media_log_(std::move(media_log)),
      target_color_space_(target_color_space),
      create_command_buffer_helper_cb_(
          std::move(create_command_buffer_helper_cb)),
      create_and_initialize_vda_cb_(std::move(create_and_initialize_vda_cb)),
      vda_capabilities_(vda_capabilities),
      output_mode_(output_mode),
      timestamps_(128) {
  DVLOG(1) << __func__;
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(vda_capabilities_.flags, 0U);
  DCHECK(media_log_);

  gpu_weak_this_ = gpu_weak_this_factory_.GetWeakPtr();
  parent_weak_this_ = parent_weak_this_factory_.GetWeakPtr();

  picture_buffer_manager_ =
      std::move(create_picture_buffer_manager_cb)
          .Run(base::BindRepeating(&VdaVideoDecoder::ReusePictureBuffer,
                                   gpu_weak_this_));
}

void VdaVideoDecoder::DestroyAsync(std::unique_ptr<VdaVideoDecoder> decoder) {
  DVLOG(1) << __func__;
  DCHECK(decoder);
  DCHECK(decoder->parent_task_runner_->RunsTasksInCurrentSequence());

  // TODO(sandersd): The documentation says that DestroyAsync() fires any
  // pending callbacks.

  // Prevent any more callbacks to this thread.
  decoder->parent_weak_this_factory_.InvalidateWeakPtrs();

  // Pass ownership of the destruction process over to the GPU thread.
  auto* gpu_task_runner = decoder->gpu_task_runner_.get();
  gpu_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::CleanupOnGpuThread, std::move(decoder)));
}

void VdaVideoDecoder::CleanupOnGpuThread(
    std::unique_ptr<VdaVideoDecoder> decoder) {
  DVLOG(2) << __func__;
  DCHECK(decoder);
  DCHECK(decoder->gpu_task_runner_->BelongsToCurrentThread());

  // VDA destruction is likely to result in reentrant calls to
  // NotifyEndOfBitstreamBuffer(). Invalidating |gpu_weak_vda_| ensures that we
  // don't call back into |vda_| during its destruction.
  decoder->gpu_weak_vda_factory_ = nullptr;
  decoder->vda_ = nullptr;
  decoder->media_log_ = nullptr;

  // Because |parent_weak_this_| was invalidated in Destroy(), picture buffer
  // dismissals since then have been dropped on the floor.
  decoder->picture_buffer_manager_->DismissAllPictureBuffers();
}

VdaVideoDecoder::~VdaVideoDecoder() {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(!gpu_weak_vda_);
}

VideoDecoderType VdaVideoDecoder::GetDecoderType() const {
  DVLOG(3) << __func__;
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  // TODO(tmathmeyer) query the accelerator for it's implementation type and
  // return that instead.
  return VideoDecoderType::kVda;
}

void VdaVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 bool low_delay,
                                 CdmContext* cdm_context,
                                 InitCB init_cb,
                                 const OutputCB& output_cb,
                                 const WaitingCB& waiting_cb) {
  DVLOG(1) << __func__ << "(" << config.AsHumanReadableString() << ")";
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(config.IsValidConfig());
  DCHECK(!init_cb_);
  DCHECK(!flush_cb_);
  DCHECK(!reset_cb_);
  DCHECK(decode_cbs_.empty());

  if (has_error_) {
    parent_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(init_cb), DecoderStatus::Codes::kFailed));
    return;
  }

  bool reinitializing = config_.IsValidConfig();

  // Store |init_cb| ASAP so that EnterErrorState() can use it. Leave |config_|
  // alone for now so that the checks can inspect it.
  init_cb_ = std::move(init_cb);
  output_cb_ = output_cb;

  // Verify that the configuration is supported.
  if (reinitializing && config.codec() != config_.codec()) {
    MEDIA_LOG(ERROR, media_log_) << "Codec cannot be changed";
    EnterErrorState();
    return;
  }

  if (!IsProfileSupported(vda_capabilities_.supported_profiles,
                          config.profile(), config.coded_size())) {
    MEDIA_LOG(INFO, media_log_) << "Unsupported profile";
    EnterErrorState();
    return;
  }

  // TODO(sandersd): Change this to a capability if any VDA starts supporting
  // alpha channels. This is believed to be impossible right now because VPx
  // alpha channel data is passed in side data, which isn't sent to VDAs.
  // HEVC is the codec that only has platform hardware decoder support, and
  // macOS currently support HEVC with alpha, so don't block HEVC here.
  if (config.alpha_mode() != VideoDecoderConfig::AlphaMode::kIsOpaque &&
      config.codec() != VideoCodec::kHEVC) {
    MEDIA_LOG(INFO, media_log_) << "Alpha formats are not supported";
    EnterErrorState();
    return;
  }

  // Encrypted streams are not supported by design. To support encrypted stream,
  // use a hardware VideoDecoder directly.
  if (config.is_encrypted()) {
    MEDIA_LOG(INFO, media_log_) << "Encrypted streams are not supported";
    EnterErrorState();
    return;
  }

  // VaapiVideoDecodeAccelerator doesn't support profile change, the different
  // profiles from the initial profile will causes an issue in AMD driver
  // (https://crbug.com/929565). We should support reinitialization for profile
  // changes. We limit this support as small as possible for safety.
  const bool is_profile_change =
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_VAAPI)
      config_.profile() != config.profile();
#else
      false;
#endif

  // Hardware decoders require ColorSpace to be set beforehand to provide
  // correct HDR output.
  const bool is_hdr_color_space_change =
      config_.profile() == media::VP9PROFILE_PROFILE2 &&
      config_.color_space_info() != config.color_space_info();

  // The configuration is supported.
  config_ = config;

  if (reinitializing) {
    if (is_profile_change || is_hdr_color_space_change) {
      MEDIA_LOG(INFO, media_log_) << "Reinitializing video decode accelerator "
                                  << "for profile change";
      gpu_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VdaVideoDecoder::ReinitializeOnGpuThread,
                                    gpu_weak_this_));
    } else {
      parent_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&VdaVideoDecoder::InitializeDone, parent_weak_this_,
                         DecoderStatus::Codes::kOk));
    }
    return;
  }

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::InitializeOnGpuThread, gpu_weak_this_));
}

void VdaVideoDecoder::ReinitializeOnGpuThread() {
  DVLOG(2) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(vda_initialized_);
  DCHECK(vda_);
  DCHECK(!reinitializing_);

  reinitializing_ = true;
  gpu_weak_vda_factory_ = nullptr;
  vda_ = nullptr;
  vda_initialized_ = false;
  InitializeOnGpuThread();
}

void VdaVideoDecoder::InitializeOnGpuThread() {
  DVLOG(2) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(!vda_);
  DCHECK(!vda_initialized_);

  // Set up |command_buffer_helper_|.
  if (!reinitializing_) {
    if (output_mode_ == VideoDecodeAccelerator::Config::OutputMode::ALLOCATE) {
      command_buffer_helper_ =
          std::move(create_command_buffer_helper_cb_).Run();
      if (!command_buffer_helper_) {
        parent_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&VdaVideoDecoder::InitializeDone, parent_weak_this_,
                           DecoderStatus::Codes::kFailed));
        return;
      }
    }

    picture_buffer_manager_->Initialize(gpu_task_runner_,
                                        command_buffer_helper_);
  }

  // Convert the configuration.
  VideoDecodeAccelerator::Config vda_config;
  vda_config.profile = config_.profile();
  // vda_config.cdm_id = [Encrypted streams are not supported]
  // vda_config.overlay_info = [Only used by AVDA]
  vda_config.encryption_scheme = config_.encryption_scheme();
  vda_config.is_deferred_initialization_allowed = false;
  vda_config.initial_expected_coded_size = config_.coded_size();
  vda_config.container_color_space = config_.color_space_info();
  vda_config.target_color_space = target_color_space_;
  vda_config.hdr_metadata = config_.hdr_metadata();
  // vda_config.sps = [Only used by AVDA]
  // vda_config.pps = [Only used by AVDA]
  vda_config.output_mode = output_mode_;
  // vda_config.supported_output_formats = [Only used by PPAPI]

  // Create and initialize the VDA.
  vda_ = create_and_initialize_vda_cb_.Run(command_buffer_helper_, this,
                                           media_log_.get(), vda_config);
  if (!vda_) {
    parent_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VdaVideoDecoder::InitializeDone, parent_weak_this_,
                       DecoderStatus::Codes::kFailedToCreateDecoder));
    return;
  }

  gpu_weak_vda_factory_ =
      std::make_unique<base::WeakPtrFactory<VideoDecodeAccelerator>>(
          vda_.get());
  gpu_weak_vda_ = gpu_weak_vda_factory_->GetWeakPtr();

  vda_initialized_ = true;
  decode_on_parent_thread_ = vda_->TryToSetupDecodeOnSeparateSequence(
      parent_weak_this_, parent_task_runner_);

  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VdaVideoDecoder::InitializeDone,
                                parent_weak_this_, DecoderStatus::Codes::kOk));
}

void VdaVideoDecoder::InitializeDone(DecoderStatus status) {
  DVLOG(1) << __func__ << " success = (" << static_cast<int>(status.code())
           << ")";
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  if (has_error_)
    return;

  if (!status.is_ok()) {
    // TODO(sandersd): This adds an unnecessary PostTask().
    EnterErrorState();
    return;
  }

  reinitializing_ = false;
  std::move(init_cb_).Run(std::move(status));
}

void VdaVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                             DecodeCB decode_cb) {
  DVLOG(3) << __func__ << "(" << (buffer->end_of_stream() ? "EOS" : "") << ")";
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!init_cb_);
  DCHECK(!flush_cb_);
  DCHECK(!reset_cb_);
  DCHECK(buffer->end_of_stream() || !buffer->decrypt_config());

  if (has_error_) {
    parent_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(decode_cb), DecoderStatus::Codes::kFailed));
    return;
  }

  // Convert EOS frame to Flush().
  if (buffer->end_of_stream()) {
    flush_cb_ = std::move(decode_cb);
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoDecodeAccelerator::Flush, gpu_weak_vda_));
    return;
  }

  // Assign a bitstream buffer ID and record the decode request.
  int32_t bitstream_buffer_id = NextID(&bitstream_buffer_id_);
  timestamps_.Put(bitstream_buffer_id, buffer->timestamp());
  decode_cbs_[bitstream_buffer_id] = std::move(decode_cb);

  if (decode_on_parent_thread_) {
    vda_->Decode(std::move(buffer), bitstream_buffer_id);
    return;
  }

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::DecodeOnGpuThread, gpu_weak_this_,
                     std::move(buffer), bitstream_buffer_id));
}

void VdaVideoDecoder::DecodeOnGpuThread(scoped_refptr<DecoderBuffer> buffer,
                                        int32_t bitstream_id) {
  DVLOG(3) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  if (!gpu_weak_vda_)
    return;

  vda_->Decode(std::move(buffer), bitstream_id);
}

void VdaVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DVLOG(2) << __func__;
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!init_cb_);
  // Note: |flush_cb_| may be non-null. If so, the flush can be completed by
  // NotifyResetDone().
  DCHECK(!reset_cb_);

  if (has_error_) {
    parent_task_runner_->PostTask(FROM_HERE, std::move(reset_cb));
    return;
  }

  reset_cb_ = std::move(reset_cb);
  gpu_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecodeAccelerator::Reset, gpu_weak_vda_));
}

bool VdaVideoDecoder::NeedsBitstreamConversion() const {
  DVLOG(3) << __func__;
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  // TODO(sandersd): Can we move bitstream conversion into VdaVideoDecoder and
  // always return false?
  return config_.codec() == VideoCodec::kH264 ||
         config_.codec() == VideoCodec::kHEVC;
}

bool VdaVideoDecoder::CanReadWithoutStalling() const {
  DVLOG(3) << __func__;
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  return picture_buffer_manager_->CanReadWithoutStalling();
}

int VdaVideoDecoder::GetMaxDecodeRequests() const {
  DVLOG(3) << __func__;
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  return 4;
}

bool VdaVideoDecoder::FramesHoldExternalResources() const {
  DVLOG(3) << __func__;
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  return output_mode_ == VideoDecodeAccelerator::Config::OutputMode::IMPORT;
}

void VdaVideoDecoder::NotifyInitializationComplete(DecoderStatus status) {
  DVLOG(2) << __func__ << "(" << static_cast<int>(status.code()) << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(vda_initialized_);

  NOTIMPLEMENTED();
}

void VdaVideoDecoder::ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                                            VideoPixelFormat format,
                                            uint32_t textures_per_buffer,
                                            const gfx::Size& dimensions,
                                            uint32_t texture_target) {
  DVLOG(2) << __func__ << "(" << requested_num_of_buffers << ", " << format
           << ", " << textures_per_buffer << ", " << dimensions.ToString()
           << ", " << texture_target << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(vda_initialized_);

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::ProvidePictureBuffersAsync,
                     gpu_weak_this_, requested_num_of_buffers, format,
                     textures_per_buffer, dimensions, texture_target));
}

void VdaVideoDecoder::ProvidePictureBuffersWithVisibleRect(
    uint32_t requested_num_of_buffers,
    VideoPixelFormat format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    const gfx::Rect& visible_rect,
    uint32_t texture_target) {
  if (output_mode_ == VideoDecodeAccelerator::Config::OutputMode::IMPORT) {
    // In IMPORT mode, we (as the client of the underlying VDA) are responsible
    // for buffer allocation with no textures (i.e., |texture_target| is
    // irrelevant). Therefore, the logic in the base version of
    // ProvidePictureBuffersWithVisibleRect() is not applicable.
    ProvidePictureBuffers(requested_num_of_buffers, format, textures_per_buffer,
                          dimensions, texture_target);
    return;
  }
  VideoDecodeAccelerator::Client::ProvidePictureBuffersWithVisibleRect(
      requested_num_of_buffers, format, textures_per_buffer, dimensions,
      visible_rect, texture_target);
}

void VdaVideoDecoder::ProvidePictureBuffersAsync(uint32_t count,
                                                 VideoPixelFormat pixel_format,
                                                 uint32_t planes,
                                                 gfx::Size texture_size,
                                                 GLenum texture_target) {
  DVLOG(2) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK_GT(count, 0U);

  if (!gpu_weak_vda_)
    return;

  // TODO(sandersd): VDAs should always be explicit.
  if (pixel_format == PIXEL_FORMAT_UNKNOWN)
    pixel_format = PIXEL_FORMAT_XRGB;

  std::vector<std::pair<PictureBuffer, gfx::GpuMemoryBufferHandle>>
      picture_buffers_and_gmbs = picture_buffer_manager_->CreatePictureBuffers(
          count, pixel_format, planes, texture_size, texture_target,
          output_mode_ == VideoDecodeAccelerator::Config::OutputMode::IMPORT
              ? VideoDecodeAccelerator::TextureAllocationMode::
                    kDoNotAllocateGLTextures
              : vda_->GetSharedImageTextureAllocationMode());
  if (picture_buffers_and_gmbs.empty()) {
    parent_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VdaVideoDecoder::EnterErrorState, parent_weak_this_));
    return;
  }

  DCHECK(gpu_weak_vda_);
  std::vector<PictureBuffer> picture_buffers;
  for (const auto& picture_buffer_and_gmb : picture_buffers_and_gmbs) {
    picture_buffers.push_back(picture_buffer_and_gmb.first);
  }
  vda_->AssignPictureBuffers(std::move(picture_buffers));

  if (output_mode_ == VideoDecodeAccelerator::Config::OutputMode::IMPORT) {
    for (auto& picture_buffer_and_gmb : picture_buffers_and_gmbs) {
      vda_->ImportBufferForPicture(picture_buffer_and_gmb.first.id(),
                                   pixel_format,
                                   std::move(picture_buffer_and_gmb.second));
    }
  }
}

void VdaVideoDecoder::DismissPictureBuffer(int32_t picture_buffer_id) {
  DVLOG(2) << __func__ << "(" << picture_buffer_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(vda_initialized_);

  if (parent_task_runner_->RunsTasksInCurrentSequence()) {
    if (!parent_weak_this_)
      return;
    DismissPictureBufferOnParentThread(picture_buffer_id);
    return;
  }

  parent_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::DismissPictureBufferOnParentThread,
                     parent_weak_this_, picture_buffer_id));
}

void VdaVideoDecoder::DismissPictureBufferOnParentThread(
    int32_t picture_buffer_id) {
  DVLOG(2) << __func__ << "(" << picture_buffer_id << ")";
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  if (!picture_buffer_manager_->DismissPictureBuffer(picture_buffer_id))
    EnterErrorState();
}

void VdaVideoDecoder::PictureReady(const Picture& picture) {
  DVLOG(3) << __func__ << "(" << picture.picture_buffer_id() << ")";
  DCHECK(vda_initialized_);

  if (parent_task_runner_->RunsTasksInCurrentSequence()) {
    if (!parent_weak_this_)
      return;
    // Note: This optimization is only correct if the output callback does not
    // reentrantly call Decode(). MojoVideoDecoderService is safe, but there is
    // no guarantee in the media::VideoDecoder interface definition.
    PictureReadyOnParentThread(picture);
    return;
  }

  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VdaVideoDecoder::PictureReadyOnParentThread,
                                parent_weak_this_, picture));
}

void VdaVideoDecoder::PictureReadyOnParentThread(Picture picture) {
  DVLOG(3) << __func__ << "(" << picture.picture_buffer_id() << ")";
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  if (has_error_)
    return;

  // Substitute the container's visible rect if the VDA didn't specify one.
  gfx::Rect visible_rect = picture.visible_rect();
  if (visible_rect.IsEmpty())
    visible_rect = config_.visible_rect();

  // Look up the decode timestamp.
  base::TimeDelta timestamp;
  int32_t bitstream_buffer_id = picture.bitstream_buffer_id();
  const auto timestamp_it = timestamps_.Peek(bitstream_buffer_id);
  if (timestamp_it == timestamps_.end()) {
    DLOG(ERROR) << "Unknown bitstream buffer " << bitstream_buffer_id;
    // TODO(sandersd): This should be fatal but DXVA VDA is triggering it, and
    // playback works if we ignore the error (use a zero timestamp).
    //
    // EnterErrorState();
    // return;
  } else {
    timestamp = timestamp_it->second;
  }

  // Create a VideoFrame for the picture.
  scoped_refptr<VideoFrame> frame = picture_buffer_manager_->CreateVideoFrame(
      picture, timestamp, visible_rect,
      config_.aspect_ratio().GetNaturalSize(visible_rect));
  if (!frame) {
    EnterErrorState();
    return;
  }
  // Some streams may have varying metadata, so bitstream metadata should be
  // preferred over metadata provide by the configuration.
  frame->set_hdr_metadata(picture.hdr_metadata() ? picture.hdr_metadata()
                                                 : config_.hdr_metadata());

  output_cb_.Run(std::move(frame));
}

void VdaVideoDecoder::NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) {
  DVLOG(3) << __func__ << "(" << bitstream_buffer_id << ")";
  DCHECK(vda_initialized_);

  if (parent_task_runner_->RunsTasksInCurrentSequence()) {
    if (!parent_weak_this_)
      return;
    // Note: This optimization is only correct if the decode callback does not
    // reentrantly call Decode(). MojoVideoDecoderService is safe, but there is
    // no guarantee in the media::VideoDecoder interface definition.
    NotifyEndOfBitstreamBufferOnParentThread(bitstream_buffer_id);
    return;
  }

  parent_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::NotifyEndOfBitstreamBufferOnParentThread,
                     parent_weak_this_, bitstream_buffer_id));
}

void VdaVideoDecoder::NotifyEndOfBitstreamBufferOnParentThread(
    int32_t bitstream_buffer_id) {
  DVLOG(3) << __func__ << "(" << bitstream_buffer_id << ")";
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  if (has_error_)
    return;

  // Look up the decode callback.
  const auto decode_cb_it = decode_cbs_.find(bitstream_buffer_id);
  if (decode_cb_it == decode_cbs_.end()) {
    DLOG(ERROR) << "Unknown bitstream buffer " << bitstream_buffer_id;
    EnterErrorState();
    return;
  }

  // Run a local copy in case the decode callback modifies |decode_cbs_|.
  DecodeCB decode_cb = std::move(decode_cb_it->second);
  decode_cbs_.erase(decode_cb_it);
  std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
}

void VdaVideoDecoder::NotifyFlushDone() {
  DVLOG(2) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(vda_initialized_);

  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VdaVideoDecoder::NotifyFlushDoneOnParentThread,
                                parent_weak_this_));
}

void VdaVideoDecoder::NotifyFlushDoneOnParentThread() {
  DVLOG(2) << __func__;
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  if (has_error_)
    return;

  // Protect against incorrect calls from the VDA.
  if (!flush_cb_)
    return;

  DCHECK(decode_cbs_.empty());
  std::move(flush_cb_).Run(DecoderStatus::Codes::kOk);
}

void VdaVideoDecoder::NotifyResetDone() {
  DVLOG(2) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(vda_initialized_);

  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VdaVideoDecoder::NotifyResetDoneOnParentThread,
                                parent_weak_this_));
}

void VdaVideoDecoder::NotifyResetDoneOnParentThread() {
  DVLOG(2) << __func__;
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  if (has_error_)
    return;

  // If NotifyFlushDone() has not been called yet, it never will be.
  //
  // We use an on-stack WeakPtr to detect Destroy() being called. A correct
  // client should not call Decode() or Reset() while there is a reset pending,
  // but we should handle that safely as well.
  //
  // TODO(sandersd): This is similar to DestroyCallbacks(); see about merging
  // them.
  base::WeakPtr<VdaVideoDecoder> weak_this = parent_weak_this_;

  std::map<int32_t, DecodeCB> local_decode_cbs = std::move(decode_cbs_);
  decode_cbs_.clear();
  for (auto& it : local_decode_cbs) {
    std::move(it.second).Run(DecoderStatus::Codes::kAborted);
    if (!weak_this)
      return;
  }

  if (weak_this && flush_cb_)
    std::move(flush_cb_).Run(DecoderStatus::Codes::kAborted);

  if (weak_this)
    std::move(reset_cb_).Run();
}

void VdaVideoDecoder::NotifyError(VideoDecodeAccelerator::Error error) {
  DVLOG(1) << __func__ << "(" << error << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(vda_initialized_);

  // Invalidate |gpu_weak_vda_| so that we won't make any more |vda_| calls.
  gpu_weak_vda_factory_ = nullptr;

  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VdaVideoDecoder::NotifyErrorOnParentThread,
                                parent_weak_this_, error));
}

gpu::SharedImageStub* VdaVideoDecoder::GetSharedImageStub() const {
  DCHECK_EQ(output_mode_, VideoDecodeAccelerator::Config::OutputMode::ALLOCATE);
  return command_buffer_helper_->GetSharedImageStub();
}

CommandBufferHelper* VdaVideoDecoder::GetCommandBufferHelper() const {
  DCHECK_EQ(output_mode_, VideoDecodeAccelerator::Config::OutputMode::ALLOCATE);
  return command_buffer_helper_.get();
}

void VdaVideoDecoder::NotifyErrorOnParentThread(
    VideoDecodeAccelerator::Error error) {
  DVLOG(1) << __func__ << "(" << error << ")";
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());

  MEDIA_LOG(ERROR, media_log_) << "VDA Error " << error;

  EnterErrorState();
}

void VdaVideoDecoder::ReusePictureBuffer(int32_t picture_buffer_id) {
  DVLOG(3) << __func__ << "(" << picture_buffer_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  if (!gpu_weak_vda_)
    return;

  vda_->ReusePictureBuffer(picture_buffer_id);
}

void VdaVideoDecoder::EnterErrorState() {
  DVLOG(1) << __func__;
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(parent_weak_this_);

  if (has_error_)
    return;

  // Start rejecting client calls immediately.
  has_error_ = true;

  // Destroy callbacks aynchronously to avoid calling them on a client stack.
  parent_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::DestroyCallbacks, parent_weak_this_));
}

void VdaVideoDecoder::DestroyCallbacks() {
  DVLOG(3) << __func__;
  DCHECK(parent_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(parent_weak_this_);
  DCHECK(has_error_);

  // We use an on-stack WeakPtr to detect Destroy() being called. Note that any
  // calls to Initialize(), Decode(), or Reset() are asynchronously rejected
  // when |has_error_| is set.
  base::WeakPtr<VdaVideoDecoder> weak_this = parent_weak_this_;

  std::map<int32_t, DecodeCB> local_decode_cbs = std::move(decode_cbs_);
  decode_cbs_.clear();
  for (auto& it : local_decode_cbs) {
    std::move(it.second).Run(DecoderStatus::Codes::kFailed);
    if (!weak_this)
      return;
  }

  if (weak_this && flush_cb_)
    std::move(flush_cb_).Run(DecoderStatus::Codes::kFailed);

  // Note: |reset_cb_| cannot return failure, so the client won't actually find
  // out about the error until another operation is attempted.
  if (weak_this && reset_cb_)
    std::move(reset_cb_).Run();

  if (weak_this && init_cb_)
    std::move(init_cb_).Run(DecoderStatus::Codes::kFailed);
}

}  // namespace media
