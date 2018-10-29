// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gpu_video_decoder.h"

#include <algorithm>
#include <array>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_util.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_ANDROID) && BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/base/android/extract_sps_and_pps.h"
#endif

namespace media {
const char GpuVideoDecoder::kDecoderName[] = "GpuVideoDecoder";

// Maximum number of concurrent VDA::Decode() operations GVD will maintain.
// Higher values allow better pipelining in the GPU, but also require more
// resources.
enum { kMaxInFlightDecodes = 4 };

// Number of bitstream buffers returned before GC is attempted on shared memory
// segments. Value chosen arbitrarily.
enum { kBufferCountBeforeGC = 1024 };

struct GpuVideoDecoder::PendingDecoderBuffer {
  PendingDecoderBuffer(std::unique_ptr<base::SharedMemory> s,
                       const DecodeCB& done_cb)
      : shared_memory(std::move(s)), done_cb(done_cb) {}
  std::unique_ptr<base::SharedMemory> shared_memory;
  DecodeCB done_cb;
};

GpuVideoDecoder::BufferData::BufferData(int32_t bbid,
                                        base::TimeDelta ts,
                                        const gfx::Rect& vr,
                                        const gfx::Size& ns)
    : bitstream_buffer_id(bbid),
      timestamp(ts),
      visible_rect(vr),
      natural_size(ns) {}

GpuVideoDecoder::BufferData::~BufferData() = default;

GpuVideoDecoder::GpuVideoDecoder(
    GpuVideoAcceleratorFactories* factories,
    const RequestOverlayInfoCB& request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    MediaLog* media_log)
    : needs_bitstream_conversion_(false),
      factories_(factories),
      request_overlay_info_cb_(request_overlay_info_cb),
      overlay_info_requested_(false),
      target_color_space_(target_color_space),
      media_log_(media_log),
      vda_initialized_(false),
      state_(kNormal),
      next_picture_buffer_id_(0),
      next_bitstream_buffer_id_(0),
      needs_all_picture_buffers_to_decode_(false),
      supports_deferred_initialization_(false),
      requires_texture_copy_(false),
      cdm_id_(CdmContext::kInvalidCdmId),
      min_shared_memory_segment_size_(0),
      bitstream_buffer_id_of_last_gc_(0),
      weak_factory_(this) {
  DCHECK(factories_);
}

void GpuVideoDecoder::Reset(const base::Closure& closure) {
  DVLOG(3) << "Reset()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  if (state_ == kDrainingDecoder) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&GpuVideoDecoder::Reset,
                                  weak_factory_.GetWeakPtr(), closure));
    return;
  }

  if (!vda_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, closure);
    return;
  }

  DCHECK(!pending_reset_cb_);
  pending_reset_cb_ = BindToCurrentLoop(closure);

  vda_->Reset();
}

static bool IsCodedSizeSupported(const gfx::Size& coded_size,
                                 const gfx::Size& min_resolution,
                                 const gfx::Size& max_resolution) {
  return (coded_size.width() <= max_resolution.width() &&
          coded_size.height() <= max_resolution.height() &&
          coded_size.width() >= min_resolution.width() &&
          coded_size.height() >= min_resolution.height());
}

// Report |success| to UMA and run |cb| with it.  This is super-specific to the
// UMA stat reported because the UMA_HISTOGRAM_ENUMERATION API requires a
// callsite to always be called with the same stat name (can't parameterize it).
static void ReportGpuVideoDecoderInitializeStatusToUMAAndRunCB(
    const VideoDecoder::InitCB& cb,
    MediaLog* media_log,
    bool success) {
  // TODO(xhwang): Report |success| directly.
  PipelineStatus status = success ? PIPELINE_OK : DECODER_ERROR_NOT_SUPPORTED;
  UMA_HISTOGRAM_ENUMERATION("Media.GpuVideoDecoderInitializeStatus", status,
                            PIPELINE_STATUS_MAX + 1);

  if (!success) {
    media_log->RecordRapporWithSecurityOrigin(
        "Media.OriginUrl.GpuVideoDecoderInitFailure");
  }

  cb.Run(success);
}

bool GpuVideoDecoder::IsPlatformDecoder() const {
  return true;
}

std::string GpuVideoDecoder::GetDisplayName() const {
  return kDecoderName;
}

void GpuVideoDecoder::Initialize(
    const VideoDecoderConfig& config,
    bool /* low_delay */,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingForDecryptionKeyCB& /* waiting_for_decryption_key_cb */) {
  DVLOG(3) << "Initialize()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb =
      base::Bind(&ReportGpuVideoDecoderInitializeStatusToUMAAndRunCB,
                 BindToCurrentLoop(init_cb), media_log_);

  bool previously_initialized = config_.IsValidConfig();
  DVLOG(1) << (previously_initialized ? "Reinitializing" : "Initializing")
           << " GVD with config: " << config.AsHumanReadableString();

  auto encryption_mode = config.encryption_scheme().mode();
  if (encryption_mode != EncryptionScheme::CIPHER_MODE_UNENCRYPTED &&
      encryption_mode != EncryptionScheme::CIPHER_MODE_AES_CTR) {
    DVLOG(1) << "VDAs only support clear or cenc encrypted streams.";
    bound_init_cb.Run(false);
    return;
  }

  // Disallow codec changes between configuration changes.
  if (previously_initialized && config_.codec() != config.codec()) {
    DVLOG(1) << "Codec changed, cannot reinitialize.";
    bound_init_cb.Run(false);
    return;
  }

  // TODO(sandersd): This should be moved to capabilities if we ever have a
  // hardware decoder which supports alpha formats.
  if (config.format() == PIXEL_FORMAT_I420A) {
    DVLOG(1) << "Alpha transparency formats are not supported.";
    bound_init_cb.Run(false);
    return;
  }

  VideoDecodeAccelerator::Capabilities capabilities =
      factories_->GetVideoDecodeAcceleratorCapabilities();

  const bool supports_encrypted_streams =
      capabilities.flags &
      VideoDecodeAccelerator::Capabilities::SUPPORTS_ENCRYPTED_STREAMS;
  if (config.is_encrypted() && (!cdm_context || !supports_encrypted_streams)) {
    DVLOG(1) << "Encrypted stream not supported.";
    bound_init_cb.Run(false);
    return;
  }

  if (!IsProfileSupported(capabilities, config.profile(), config.coded_size(),
                          config.is_encrypted())) {
    DVLOG(1) << "Unsupported profile " << GetProfileName(config.profile())
             << ", unsupported coded size " << config.coded_size().ToString()
             << ", or accelerator should only be used for encrypted content. "
             << " is_encrypted: " << (config.is_encrypted() ? "yes." : "no.");
    bound_init_cb.Run(false);
    return;
  }

  config_ = config;
  needs_all_picture_buffers_to_decode_ =
      capabilities.flags &
      VideoDecodeAccelerator::Capabilities::NEEDS_ALL_PICTURE_BUFFERS_TO_DECODE;
  needs_bitstream_conversion_ =
      (config.codec() == kCodecH264) || (config.codec() == kCodecHEVC);
  requires_texture_copy_ =
      !!(capabilities.flags &
         VideoDecodeAccelerator::Capabilities::REQUIRES_TEXTURE_COPY);
  supports_deferred_initialization_ = !!(
      capabilities.flags &
      VideoDecodeAccelerator::Capabilities::SUPPORTS_DEFERRED_INITIALIZATION);
  output_cb_ = output_cb;

  // Attempt to choose a reasonable size for the shared memory segments based on
  // the size of video. These values are chosen based on experiments with common
  // videos from the web. Too small and you'll end up creating too many segments
  // too large and you end up wasting significant amounts of memory.
  const int height = config.coded_size().height();
  if (height >= 4000)  // ~4320p
    min_shared_memory_segment_size_ = 384 * 1024;
  else if (height >= 2000)  // ~2160p
    min_shared_memory_segment_size_ = 192 * 1024;
  else if (height >= 1000)  // ~1080p
    min_shared_memory_segment_size_ = 96 * 1024;
  else if (height >= 700)  // ~720p
    min_shared_memory_segment_size_ = 72 * 1024;
  else if (height >= 400)  // ~480p
    min_shared_memory_segment_size_ = 48 * 1024;
  else  // ~360p or less
    min_shared_memory_segment_size_ = 32 * 1024;

  if (config.is_encrypted() && !supports_deferred_initialization_) {
    DVLOG(1) << __func__
             << " Encrypted stream requires deferred initialialization.";
    bound_init_cb.Run(false);
    return;
  }

  if (previously_initialized) {
    DVLOG(3) << __func__
             << " Expecting initialized VDA to detect in-stream config change.";
    // Reinitialization with a different config (but same codec and profile).
    // VDA should handle it by detecting this in-stream by itself,
    // no need to notify it.
    bound_init_cb.Run(true);
    return;
  }

  vda_ = factories_->CreateVideoDecodeAccelerator();
  if (!vda_) {
    DVLOG(1) << "Failed to create a VDA.";
    bound_init_cb.Run(false);
    return;
  }

  if (cdm_context)
    cdm_id_ = cdm_context->GetCdmId();

  if (config.is_encrypted() && cdm_id_ == CdmContext::kInvalidCdmId) {
    DVLOG(1) << "CDM ID not available.";
    bound_init_cb.Run(false);
    return;
  }

  init_cb_ = bound_init_cb;

  const bool supports_external_output_surface = !!(
      capabilities.flags &
      VideoDecodeAccelerator::Capabilities::SUPPORTS_EXTERNAL_OUTPUT_SURFACE);
  if (supports_external_output_surface && request_overlay_info_cb_) {
    const bool requires_restart_for_external_output_surface =
        !(capabilities.flags & VideoDecodeAccelerator::Capabilities::
                                   SUPPORTS_SET_EXTERNAL_OUTPUT_SURFACE);

    // If we have a surface request callback we should call it and complete
    // initialization with the returned surface.
    request_overlay_info_cb_.Run(
        requires_restart_for_external_output_surface,
        BindToCurrentLoop(base::Bind(&GpuVideoDecoder::OnOverlayInfoAvailable,
                                     weak_factory_.GetWeakPtr())));
    overlay_info_requested_ = true;
    return;
  }

  // If external surfaces are not supported we can complete initialization now.
  CompleteInitialization(OverlayInfo());
}

// OnOverlayInfoAvailable() might be called at any time between Initialize() and
// ~GpuVideoDecoder() so we have to be careful to not make assumptions about
// the current state.
// At most one of |surface_id| and |token| should be provided.  The other will
// be kNoSurfaceID or an empty token, respectively.
void GpuVideoDecoder::OnOverlayInfoAvailable(const OverlayInfo& overlay_info) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  if (!vda_)
    return;

  // If the VDA has not been initialized, we were waiting for the first surface
  // so it can be passed to Initialize() via the config. We can't call
  // SetSurface() before initializing because there is no remote VDA to handle
  // the call yet.
  if (!vda_initialized_) {
    CompleteInitialization(overlay_info);
    return;
  }

  // The VDA must be already initialized (or async initialization is in
  // progress) so we can call SetSurface().
  vda_->SetOverlayInfo(overlay_info);
}

void GpuVideoDecoder::CompleteInitialization(const OverlayInfo& overlay_info) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK(vda_);
  DCHECK(init_cb_);
  DCHECK(!vda_initialized_);

  VideoDecodeAccelerator::Config vda_config;
  vda_config.profile = config_.profile();
  vda_config.cdm_id = cdm_id_;
  vda_config.overlay_info = overlay_info;
  vda_config.encryption_scheme = config_.encryption_scheme();
  vda_config.is_deferred_initialization_allowed = true;
  vda_config.initial_expected_coded_size = config_.coded_size();
  vda_config.container_color_space = config_.color_space_info();
  vda_config.target_color_space = target_color_space_;
  vda_config.hdr_metadata = config_.hdr_metadata();

#if defined(OS_ANDROID) && BUILDFLAG(USE_PROPRIETARY_CODECS)
  // We pass the SPS and PPS on Android because it lets us initialize
  // MediaCodec more reliably (http://crbug.com/649185).
  if (config_.codec() == kCodecH264)
    ExtractSpsAndPps(config_.extra_data(), &vda_config.sps, &vda_config.pps);
#endif

  vda_initialized_ = true;
  if (!vda_->Initialize(vda_config, this)) {
    DVLOG(1) << "VDA::Initialize failed.";
    // It's important to set |vda_| to null so that OnSurfaceAvailable() will
    // not call SetSurface() on a nonexistent remote VDA.
    DestroyVDA();
    std::move(init_cb_).Run(false);
    return;
  }

  // If deferred initialization is not supported, initialization is complete.
  // Otherwise, a call to NotifyInitializationComplete will follow with the
  // result of deferred initialization.
  if (!supports_deferred_initialization_)
    std::move(init_cb_).Run(true);
}

void GpuVideoDecoder::NotifyInitializationComplete(bool success) {
  DVLOG_IF(1, !success) << __func__ << " Deferred initialization failed.";

  if (init_cb_)
    std::move(init_cb_).Run(success);
}

void GpuVideoDecoder::DestroyPictureBuffers() {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  for (const auto& kv : assigned_picture_buffers_) {
    int64_t picture_buffer_id = kv.first;
    PictureBuffer::TextureIds texture_ids = kv.second.client_texture_ids();

    // Not destroying PictureBuffers in |picture_buffers_at_display_| yet, since
    // their textures may still be in use by the user of this GpuVideoDecoder.
    if (picture_buffers_at_display_.find(picture_buffer_id) ==
        picture_buffers_at_display_.end()) {
      for (uint32_t id : texture_ids)
        factories_->DeleteTexture(id);
    }
  }
  factories_->ShallowFlushCHROMIUM();
  assigned_picture_buffers_.clear();
}

void GpuVideoDecoder::DestroyVDA() {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  vda_.reset();

  DestroyPictureBuffers();
}

void GpuVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                             const DecodeCB& decode_cb) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK(!pending_reset_cb_);

  DVLOG(3) << __func__ << " " << buffer->AsHumanReadableString();

  DecodeCB bound_decode_cb = BindToCurrentLoop(decode_cb);

  if (state_ == kError || !vda_) {
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  switch (state_) {
    case kDecoderDrained:
      state_ = kNormal;
      FALLTHROUGH;
    case kNormal:
      break;
    case kDrainingDecoder:
    case kError:
      NOTREACHED();
      return;
  }

  DCHECK_EQ(state_, kNormal);

  if (buffer->end_of_stream()) {
    DVLOG(3) << __func__ << " Initiating Flush for EOS.";
    state_ = kDrainingDecoder;
    eos_decode_cb_ = bound_decode_cb;
    vda_->Flush();
    return;
  }

  size_t size = buffer->data_size();
  auto shared_memory = GetSharedMemory(size);
  if (!shared_memory) {
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  memcpy(shared_memory->memory(), buffer->data(), size);
  // AndroidVideoDecodeAccelerator needs the timestamp to output frames in
  // presentation order.
  BitstreamBuffer bitstream_buffer(next_bitstream_buffer_id_,
                                   shared_memory->handle(), size, 0,
                                   buffer->timestamp());

  if (buffer->decrypt_config()) {
    bitstream_buffer.SetDecryptionSettings(
        buffer->decrypt_config()->key_id(), buffer->decrypt_config()->iv(),
        buffer->decrypt_config()->subsamples());
  }

  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;
  DCHECK(
      !base::ContainsKey(bitstream_buffers_in_decoder_, bitstream_buffer.id()));
  RecordBufferData(bitstream_buffer, *buffer);

  bitstream_buffers_in_decoder_.emplace(
      bitstream_buffer.id(),
      PendingDecoderBuffer(std::move(shared_memory), decode_cb));
  DCHECK_LE(static_cast<int>(bitstream_buffers_in_decoder_.size()),
            kMaxInFlightDecodes);

  vda_->Decode(bitstream_buffer);
}

void GpuVideoDecoder::RecordBufferData(const BitstreamBuffer& bitstream_buffer,
                                       const DecoderBuffer& buffer) {
  input_buffer_data_.push_front(
      BufferData(bitstream_buffer.id(), buffer.timestamp(),
                 config_.visible_rect(), config_.natural_size()));
  // Why this value?  Because why not.  avformat.h:MAX_REORDER_DELAY is 16, but
  // that's too small for some pathological B-frame test videos.  The cost of
  // using too-high a value is low (192 bits per extra slot).
  static const size_t kMaxInputBufferDataSize = 128;
  // Pop from the back of the list, because that's the oldest and least likely
  // to be useful in the future data.
  if (input_buffer_data_.size() > kMaxInputBufferDataSize)
    input_buffer_data_.pop_back();
}

void GpuVideoDecoder::GetBufferData(int32_t id,
                                    base::TimeDelta* timestamp,
                                    gfx::Rect* visible_rect,
                                    gfx::Size* natural_size) {
  for (std::list<BufferData>::const_iterator it = input_buffer_data_.begin();
       it != input_buffer_data_.end(); ++it) {
    if (it->bitstream_buffer_id != id)
      continue;
    *timestamp = it->timestamp;
    *visible_rect = it->visible_rect;
    *natural_size = it->natural_size;
    return;
  }
  NOTREACHED() << "Missing bitstreambuffer id: " << id;
}

bool GpuVideoDecoder::NeedsBitstreamConversion() const {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  return needs_bitstream_conversion_;
}

bool GpuVideoDecoder::CanReadWithoutStalling() const {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  size_t available_pictures = AvailablePictures();
  return next_picture_buffer_id_ ==
             0 ||  // Decode() will ProvidePictureBuffers().
         (!needs_all_picture_buffers_to_decode_ && available_pictures > 0) ||
         available_pictures == assigned_picture_buffers_.size();
}

size_t GpuVideoDecoder::AvailablePictures() const {
  size_t ret = 0;
  for (const auto& kv : assigned_picture_buffers_) {
    if (picture_buffers_at_display_.find(kv.first) ==
        picture_buffers_at_display_.end()) {
      ++ret;
    }
  }
  return ret;
}

int GpuVideoDecoder::GetMaxDecodeRequests() const {
  return kMaxInFlightDecodes;
}

void GpuVideoDecoder::ProvidePictureBuffers(uint32_t count,
                                            VideoPixelFormat format,
                                            uint32_t textures_per_buffer,
                                            const gfx::Size& size,
                                            uint32_t texture_target) {
  DVLOG(3) << "ProvidePictureBuffers(" << count << ", " << size.width() << "x"
           << size.height() << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  std::vector<uint32_t> texture_ids;
  std::vector<gpu::Mailbox> texture_mailboxes;

  if (format == PIXEL_FORMAT_UNKNOWN) {
    format = IsOpaque(config_.format()) ? PIXEL_FORMAT_XRGB : PIXEL_FORMAT_ARGB;
  }

  if (!factories_->CreateTextures(count * textures_per_buffer, size,
                                  &texture_ids, &texture_mailboxes,
                                  texture_target)) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  sync_token_ = factories_->CreateSyncToken();
  DCHECK_EQ(count * textures_per_buffer, texture_ids.size());
  DCHECK_EQ(count * textures_per_buffer, texture_mailboxes.size());

  if (!vda_)
    return;

  std::vector<PictureBuffer> picture_buffers;
  size_t index = 0;
  for (size_t i = 0; i < count; ++i) {
    PictureBuffer::TextureIds ids;
    std::vector<gpu::Mailbox> mailboxes;
    for (size_t j = 0; j < textures_per_buffer; j++) {
      ids.push_back(texture_ids[index]);
      mailboxes.push_back(texture_mailboxes[index]);
      index++;
    }

    picture_buffers.push_back(PictureBuffer(next_picture_buffer_id_++, size,
                                            ids, mailboxes, texture_target,
                                            format));
    bool inserted = assigned_picture_buffers_
                        .insert(std::make_pair(picture_buffers.back().id(),
                                               picture_buffers.back()))
                        .second;
    DCHECK(inserted);
  }

  vda_->AssignPictureBuffers(picture_buffers);
}

void GpuVideoDecoder::DismissPictureBuffer(int32_t id) {
  DVLOG(3) << "DismissPictureBuffer(" << id << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  auto it = assigned_picture_buffers_.find(id);
  if (it == assigned_picture_buffers_.end()) {
    NOTREACHED() << "Missing picture buffer: " << id;
    return;
  }

  // If it's in |picture_buffers_at_display_|, postpone deletion of it until
  // it's returned to us.
  if (picture_buffers_at_display_.find(id) ==
      picture_buffers_at_display_.end()) {
    for (const auto texture_id : (it->second).client_texture_ids())
      factories_->DeleteTexture(texture_id);
  }

  assigned_picture_buffers_.erase(it);
}

void GpuVideoDecoder::PictureReady(const media::Picture& picture) {
  DVLOG(3) << "PictureReady()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  auto it = assigned_picture_buffers_.find(picture.picture_buffer_id());
  if (it == assigned_picture_buffers_.end()) {
    DLOG(ERROR) << "Missing picture buffer: " << picture.picture_buffer_id();
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  PictureBuffer& pb = it->second;
  if (picture.size_changed()) {
    // Update the PictureBuffer size to match that of the Picture. Some VDA's
    // (e.g. Android) will handle resolution changes internally without
    // requesting new PictureBuffers. Sending a Picture of unmatched size is
    // the signal that we should update the size of our PictureBuffer.
    DCHECK(pb.size() != picture.visible_rect().size());
    DVLOG(3) << __func__ << " Updating size of PictureBuffer[" << pb.id()
             << "] from:" << pb.size().ToString()
             << " to:" << picture.visible_rect().size().ToString();
    pb.set_size(picture.visible_rect().size());
  }

  // Update frame's timestamp.
  base::TimeDelta timestamp;
  // Some of the VDAs like DXVA, and VTVDA don't support and thus don't provide
  // us with visible size in picture.size, passing (0, 0) instead, so for those
  // cases drop it and use config information instead.
  gfx::Rect visible_rect;
  gfx::Size natural_size;
  GetBufferData(picture.bitstream_buffer_id(), &timestamp, &visible_rect,
                &natural_size);

  double pixel_aspect_ratio = GetPixelAspectRatio(visible_rect, natural_size);
  if (!picture.visible_rect().IsEmpty()) {
    visible_rect = picture.visible_rect();
    natural_size = GetNaturalSize(visible_rect, pixel_aspect_ratio);
  }
  if (!gfx::Rect(pb.size()).Contains(visible_rect)) {
    LOG(WARNING) << "Visible size " << visible_rect.ToString()
                 << " is larger than coded size " << pb.size().ToString();
    visible_rect.Intersect(gfx::Rect(pb.size()));
    natural_size = GetNaturalSize(visible_rect, pixel_aspect_ratio);
  }

  DCHECK(pb.texture_target());

  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  for (size_t i = 0; i < pb.client_texture_ids().size(); ++i) {
    mailbox_holders[i] = gpu::MailboxHolder(pb.texture_mailbox(i), sync_token_,
                                            pb.texture_target());
  }

  scoped_refptr<VideoFrame> frame(VideoFrame::WrapNativeTextures(
      pb.pixel_format(), mailbox_holders,
      // Always post ReleaseMailbox to avoid deadlock with the compositor when
      // releasing video frames on the media thread; http://crbug.com/710209.
      BindToCurrentLoop(base::Bind(
          &GpuVideoDecoder::ReleaseMailbox, weak_factory_.GetWeakPtr(),
          factories_, picture.picture_buffer_id(), pb.client_texture_ids())),
      pb.size(), visible_rect, natural_size, timestamp));
  if (!frame) {
    DLOG(ERROR) << "Create frame failed for: " << picture.picture_buffer_id();
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  frame->set_color_space(picture.color_space());
  if (picture.allow_overlay())
    frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY, true);
  if (picture.texture_owner())
    frame->metadata()->SetBoolean(VideoFrameMetadata::TEXTURE_OWNER, true);
  if (picture.wants_promotion_hint()) {
    frame->metadata()->SetBoolean(VideoFrameMetadata::WANTS_PROMOTION_HINT,
                                  true);
  }

  if (requires_texture_copy_)
    frame->metadata()->SetBoolean(VideoFrameMetadata::COPY_REQUIRED, true);

  picture_buffers_at_display_.insert(
      std::make_pair(picture.picture_buffer_id(), pb.client_texture_ids()));

  DeliverFrame(frame);
}

void GpuVideoDecoder::DeliverFrame(const scoped_refptr<VideoFrame>& frame) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  // During a pending vda->Reset(), we don't accumulate frames.  Drop it on the
  // floor and return.
  if (pending_reset_cb_)
    return;

  frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, true);

  output_cb_.Run(frame);
}

// static
void GpuVideoDecoder::ReleaseMailbox(
    base::WeakPtr<GpuVideoDecoder> decoder,
    media::GpuVideoAcceleratorFactories* factories,
    int64_t picture_buffer_id,
    PictureBuffer::TextureIds ids,
    const gpu::SyncToken& release_sync_token) {
  DCHECK(factories->GetTaskRunner()->BelongsToCurrentThread());
  factories->WaitSyncToken(release_sync_token);

  if (decoder) {
    decoder->ReusePictureBuffer(picture_buffer_id);
    return;
  }
  // It's the last chance to delete the texture after display,
  // because GpuVideoDecoder was destructed.
  for (uint32_t id : ids)
    factories->DeleteTexture(id);

  // Flush the delete(s) to the server, to avoid crbug.com/737992 .
  factories->ShallowFlushCHROMIUM();
}

void GpuVideoDecoder::ReusePictureBuffer(int64_t picture_buffer_id) {
  DVLOG(3) << "ReusePictureBuffer(" << picture_buffer_id << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  auto iter_range = picture_buffers_at_display_.equal_range(picture_buffer_id);
  DCHECK(iter_range.first != iter_range.second);
  bool only_one_element = (std::next(iter_range.first) == iter_range.second);
  PictureBuffer::TextureIds ids = iter_range.first->second;
  picture_buffers_at_display_.erase(iter_range.first);

  if (only_one_element && assigned_picture_buffers_.find(picture_buffer_id) ==
                              assigned_picture_buffers_.end()) {
    // This picture was dismissed while in display, so we postponed deletion.
    for (const auto id : ids)
      factories_->DeleteTexture(id);
    return;
  }

  // DestroyVDA() might already have been called.
  if (vda_)
    vda_->ReusePictureBuffer(picture_buffer_id);
}

std::unique_ptr<base::SharedMemory> GpuVideoDecoder::GetSharedMemory(
    size_t min_size) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  auto it = std::lower_bound(available_shm_segments_.begin(),
                             available_shm_segments_.end(), min_size,
                             [](const ShMemEntry& entry, const size_t size) {
                               return entry.first->mapped_size() < size;
                             });
  if (it != available_shm_segments_.end()) {
    auto ret = std::move(it->first);
    available_shm_segments_.erase(it);
    return ret;
  }

  return factories_->CreateSharedMemory(
      std::max(min_shared_memory_segment_size_, min_size));
}

void GpuVideoDecoder::PutSharedMemory(
    std::unique_ptr<base::SharedMemory> shared_memory,
    int32_t last_bitstream_buffer_id) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  available_shm_segments_.emplace(std::move(shared_memory),
                                  last_bitstream_buffer_id);

  if (next_bitstream_buffer_id_ < bitstream_buffer_id_of_last_gc_ ||
      next_bitstream_buffer_id_ - bitstream_buffer_id_of_last_gc_ >
          kBufferCountBeforeGC) {
    base::EraseIf(available_shm_segments_, [this](const ShMemEntry& entry) {
      // Check for overflow rollover...
      if (next_bitstream_buffer_id_ < entry.second)
        return next_bitstream_buffer_id_ > kBufferCountBeforeGC;

      return next_bitstream_buffer_id_ - entry.second > kBufferCountBeforeGC;
    });
    bitstream_buffer_id_of_last_gc_ = next_bitstream_buffer_id_;
  }
}

void GpuVideoDecoder::NotifyEndOfBitstreamBuffer(int32_t id) {
  DVLOG(3) << "NotifyEndOfBitstreamBuffer(" << id << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  auto it = bitstream_buffers_in_decoder_.find(id);
  if (it == bitstream_buffers_in_decoder_.end()) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    NOTREACHED() << "Missing bitstream buffer: " << id;
    return;
  }

  PutSharedMemory(std::move(it->second.shared_memory), id);
  it->second.done_cb.Run(state_ == kError ? DecodeStatus::DECODE_ERROR
                                          : DecodeStatus::OK);
  bitstream_buffers_in_decoder_.erase(it);
}

GpuVideoDecoder::~GpuVideoDecoder() {
  DVLOG(3) << __func__;
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  if (vda_)
    DestroyVDA();
  DCHECK(assigned_picture_buffers_.empty());

  if (init_cb_)
    std::move(init_cb_).Run(false);
  if (request_overlay_info_cb_ && overlay_info_requested_) {
    std::move(request_overlay_info_cb_).Run(false, ProvideOverlayInfoCB());
  }

  for (auto it = bitstream_buffers_in_decoder_.begin();
       it != bitstream_buffers_in_decoder_.end(); ++it) {
    it->second.done_cb.Run(DecodeStatus::ABORTED);
  }
  bitstream_buffers_in_decoder_.clear();

  if (pending_reset_cb_)
    std::move(pending_reset_cb_).Run();
}

void GpuVideoDecoder::NotifyFlushDone() {
  DVLOG(3) << "NotifyFlushDone()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK_EQ(state_, kDrainingDecoder);
  state_ = kDecoderDrained;
  std::move(eos_decode_cb_).Run(DecodeStatus::OK);

  // Assume flush is for a config change, so drop shared memory segments in
  // anticipation of a resize occurring.
  available_shm_segments_.clear();
}

void GpuVideoDecoder::NotifyResetDone() {
  DVLOG(3) << "NotifyResetDone()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK(bitstream_buffers_in_decoder_.empty());

  // This needs to happen after the Reset() on vda_ is done to ensure pictures
  // delivered during the reset can find their time data.
  input_buffer_data_.clear();

  if (pending_reset_cb_)
    std::move(pending_reset_cb_).Run();
}

void GpuVideoDecoder::NotifyError(media::VideoDecodeAccelerator::Error error) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  if (!vda_)
    return;

  if (init_cb_)
    std::move(init_cb_).Run(false);

  // If we have any bitstream buffers, then notify one that an error has
  // occurred.  This guarantees that somebody finds out about the error.  If
  // we don't do this, and if the max decodes are already in flight, then there
  // won't be another decode request to report the error.
  if (!bitstream_buffers_in_decoder_.empty()) {
    auto it = bitstream_buffers_in_decoder_.begin();
    it->second.done_cb.Run(DecodeStatus::DECODE_ERROR);
    bitstream_buffers_in_decoder_.erase(it);
  }

  if (state_ == kDrainingDecoder)
    std::move(eos_decode_cb_).Run(DecodeStatus::DECODE_ERROR);

  state_ = kError;

  DLOG(ERROR) << "VDA Error: " << error;
  UMA_HISTOGRAM_ENUMERATION("Media.GpuVideoDecoderError", error,
                            media::VideoDecodeAccelerator::ERROR_MAX + 1);

  DestroyVDA();
}

bool GpuVideoDecoder::IsProfileSupported(
    const VideoDecodeAccelerator::Capabilities& capabilities,
    VideoCodecProfile profile,
    const gfx::Size& coded_size,
    bool is_encrypted) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  for (const auto& supported_profile : capabilities.supported_profiles) {
    if (profile == supported_profile.profile &&
        !(supported_profile.encrypted_only && !is_encrypted) &&
        IsCodedSizeSupported(coded_size, supported_profile.min_resolution,
                             supported_profile.max_resolution)) {
      return true;
    }
  }
  return false;
}

void GpuVideoDecoder::DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent()
    const {
  DCHECK(factories_->GetTaskRunner()->BelongsToCurrentThread());
}

}  // namespace media
