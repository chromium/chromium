// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/library_cdm/clear_key_cdm/cdm_video_decoder.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/optional.h"
// Necessary to convert async media::VideoDecoder to sync CdmVideoDecoder.
// Typically not recommended for production code, but is ok here since
// ClearKeyCdm is only for testing.
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/decode_status.h"
#include "media/base/media_util.h"
#include "media/cdm/cdm_type_conversion.h"
#include "media/cdm/library_cdm/cdm_host_proxy.h"
#include "media/media_buildflags.h"
#include "third_party/libaom/libaom_buildflags.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBAOM_DECODER)
#include "media/filters/aom_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_DAV1D_DECODER)
#include "media/filters/dav1d_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_FFMPEG)
#include "media/filters/ffmpeg_video_decoder.h"
#endif

namespace media {

namespace {

media::VideoDecoderConfig ToClearMediaVideoDecoderConfig(
    const cdm::VideoDecoderConfig_3& config) {
  gfx::Size coded_size(config.coded_size.width, config.coded_size.width);

  VideoDecoderConfig media_config(
      ToMediaVideoCodec(config.codec), ToMediaVideoCodecProfile(config.profile),
      VideoDecoderConfig::AlphaMode::kIsOpaque,
      ToMediaColorSpace(config.color_space), kNoTransformation, coded_size,
      gfx::Rect(coded_size), coded_size,
      std::vector<uint8_t>(config.extra_data,
                           config.extra_data + config.extra_data_size),
      EncryptionScheme::kUnencrypted);

  return media_config;
}

bool ToCdmVideoFrame(const VideoFrame& video_frame,
                     CdmHostProxy* cdm_host_proxy,
                     CdmVideoDecoder::CdmVideoFrame* cdm_video_frame) {
  DCHECK(cdm_video_frame);

  if (!video_frame.IsMappable()) {
    DVLOG(1) << "VideoFrame is not mappable";
    return false;
  }

  if (!IsYuvPlanar(video_frame.format())) {
    DVLOG(1) << "Only YUV planar format supported";
    return false;
  }

  if (VideoFrame::NumPlanes(video_frame.format()) != 3u) {
    DVLOG(1) << "Only 3-plane format supported";
    return false;
  }

  auto cdm_video_format = ToCdmVideoFormat(video_frame.format());
  if (cdm_video_format == cdm::kUnknownVideoFormat) {
    DVLOG(1) << "VideoFrame has unsupported format: " << video_frame.format();
    return false;
  }

  // Get required allocation size for a tightly packed frame.
  auto space_required = VideoFrame::AllocationSize(video_frame.format(),
                                                   video_frame.coded_size());
  auto* buffer = cdm_host_proxy->Allocate(space_required);
  if (!buffer) {
    LOG(ERROR) << __func__ << ": Buffer allocation failed.";
    return false;
  }

  buffer->SetSize(base::checked_cast<uint32_t>(space_required));
  cdm_video_frame->SetFrameBuffer(buffer);
  cdm_video_frame->SetFormat(cdm_video_format);
  cdm_video_frame->SetSize(
      {video_frame.coded_size().width(), video_frame.coded_size().height()});
  cdm_video_frame->SetTimestamp(video_frame.timestamp().InMicroseconds());
  // TODO(crbug.com/707127): Set ColorSpace here. It's not trivial to convert
  // a gfx::ColorSpace (from VideoFrame) to another other ColorSpace like
  // cdm::ColorSpace.

  static_assert(VideoFrame::kYPlane == cdm::kYPlane && cdm::kYPlane == 0, "");
  static_assert(VideoFrame::kUPlane == cdm::kUPlane && cdm::kUPlane == 1, "");
  static_assert(VideoFrame::kVPlane == cdm::kVPlane && cdm::kVPlane == 2, "");

  uint8_t* dst = buffer->Data();
  uint32_t offset = 0;
  for (int plane = 0; plane < 3; ++plane) {
    const uint8_t* src = video_frame.data(plane);
    int src_stride = video_frame.stride(plane);
    int row_bytes = video_frame.row_bytes(plane);
    int rows = video_frame.rows(plane);

    auto cdm_plane = static_cast<cdm::VideoPlane>(plane);
    cdm_video_frame->SetPlaneOffset(cdm_plane, offset);
    // Since it's tightly packed, the stride is the same as row bytes.
    cdm_video_frame->SetStride(cdm_plane, row_bytes);

    libyuv::CopyPlane(src, src_stride, dst, row_bytes, row_bytes, rows);
    dst += row_bytes * rows;
    offset += row_bytes * rows;
  }

  DCHECK_GE(space_required, offset) << ": Space mismatch";
  return true;
}

// Media VideoDecoders typically assumes a global environment where a lot of
// things are already setup in the process, e.g. base::ThreadTaskRunnerHandle
// and base::CommandLine. These will be available in the component build because
// the CDM and the host is depending on the same base/ target. In static build,
// they will not be available and we have to setup it by ourselves.
void SetupGlobalEnvironmentIfNeeded() {
  // Creating a base::SingleThreadTaskExecutor to setup
  // base::ThreadTaskRunnerHandle.
  if (!base::ThreadTaskRunnerHandle::IsSet()) {
    static base::NoDestructor<base::SingleThreadTaskExecutor> task_executor;
  }

  if (!base::CommandLine::InitializedForCurrentProcess())
    base::CommandLine::Init(0, nullptr);
}

// Adapts a media::VideoDecoder to a CdmVideoDecoder. Media VideoDecoders
// operations are asynchronous, often posting callbacks to the task runner. The
// CdmVideoDecoder operations are synchronous. Therefore, after calling
// media::VideoDecoder, we need to run a RunLoop manually and wait for the
// asynchronous operation to finish. The RunLoop must be of type
// |kNestableTasksAllowed| because we could be running the RunLoop in a task,
// e.g. in component builds when we share the same task runner as the host. In
// a static build, this is not necessary.
class VideoDecoderAdapter : public CdmVideoDecoder {
 public:
  VideoDecoderAdapter(CdmHostProxy* cdm_host_proxy,
                      std::unique_ptr<VideoDecoder> video_decoder)
      : cdm_host_proxy_(cdm_host_proxy),
        video_decoder_(std::move(video_decoder)) {
    DCHECK(cdm_host_proxy_);
  }

  ~VideoDecoderAdapter() final = default;

  // CdmVideoDecoder implementation.
  bool Initialize(const cdm::VideoDecoderConfig_3& config) final {
    auto clear_config = ToClearMediaVideoDecoderConfig(config);
    DVLOG(1) << __func__ << ": " << clear_config.AsHumanReadableString();
    DCHECK(!last_init_result_.has_value());

    // Initialize |video_decoder_| and wait for completion.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    video_decoder_->Initialize(
        clear_config,
        /* low_delay = */ false,
        /* cdm_context = */ nullptr,
        base::BindRepeating(&VideoDecoderAdapter::OnInitialized,
                            weak_factory_.GetWeakPtr(), run_loop.QuitClosure()),
        base::BindRepeating(&VideoDecoderAdapter::OnVideoFrameReady,
                            weak_factory_.GetWeakPtr()),
        /* waiting_cb = */ base::DoNothing());
    run_loop.Run();

    auto result = last_init_result_.value();
    last_init_result_.reset();

    return result;
  }

  void Deinitialize() final {
    // Do nothing since |video_decoder_| supports reinitialization without
    // the need to deinitialize first.
  }

  void Reset() final {
    DVLOG(2) << __func__;

    // Reset |video_decoder_| and wait for completion.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    video_decoder_->Reset(base::BindRepeating(&VideoDecoderAdapter::OnReset,
                                              weak_factory_.GetWeakPtr(),
                                              run_loop.QuitClosure()));
    run_loop.Run();
  }

  cdm::Status Decode(scoped_refptr<DecoderBuffer> buffer,
                     CdmVideoFrame* decoded_frame) final {
    DVLOG(3) << __func__;
    DCHECK(!last_decode_status_.has_value());

    // Call |video_decoder_| Decode() and wait for completion.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    video_decoder_->Decode(std::move(buffer),
                           base::BindRepeating(&VideoDecoderAdapter::OnDecoded,
                                               weak_factory_.GetWeakPtr(),
                                               run_loop.QuitClosure()));
    run_loop.Run();

    auto decode_status = last_decode_status_.value();
    last_decode_status_.reset();

    if (decode_status == DecodeStatus::DECODE_ERROR)
      return cdm::kDecodeError;

    // "ABORTED" shouldn't happen during a sync decode, so treat it as an error.
    DCHECK_EQ(decode_status, DecodeStatus::OK);

    if (decoded_video_frames_.empty())
      return cdm::kNeedMoreData;

    auto video_frame = decoded_video_frames_.front();
    decoded_video_frames_.pop();

    return ToCdmVideoFrame(*video_frame, cdm_host_proxy_, decoded_frame)
               ? cdm::kSuccess
               : cdm::kDecodeError;
  }

 private:
  void OnInitialized(base::OnceClosure quit_closure, bool success) {
    DVLOG(1) << __func__ << " success = " << success;
    DCHECK(!last_init_result_.has_value());
    last_init_result_ = success;
    std::move(quit_closure).Run();
  }

  void OnVideoFrameReady(scoped_refptr<VideoFrame> video_frame) {
    // Do not queue EOS frames, which is not needed.
    if (video_frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM))
      return;

    decoded_video_frames_.push(std::move(video_frame));
  }

  void OnReset(base::OnceClosure quit_closure) {
    VideoFrameQueue empty_queue;
    std::swap(decoded_video_frames_, empty_queue);
    std::move(quit_closure).Run();
  }

  void OnDecoded(base::OnceClosure quit_closure, DecodeStatus decode_status) {
    DCHECK(!last_decode_status_.has_value());
    last_decode_status_ = decode_status;
    std::move(quit_closure).Run();
  }

  CdmHostProxy* const cdm_host_proxy_;
  std::unique_ptr<VideoDecoder> video_decoder_;

  // Results of |video_decoder_| operations. Set iff the callback of the
  // operation has been called.
  base::Optional<bool> last_init_result_;
  base::Optional<DecodeStatus> last_decode_status_;

  // Queue of decoded video frames.
  using VideoFrameQueue = base::queue<scoped_refptr<VideoFrame>>;
  VideoFrameQueue decoded_video_frames_;

  base::WeakPtrFactory<VideoDecoderAdapter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderAdapter);
};

}  // namespace

std::unique_ptr<CdmVideoDecoder> CreateVideoDecoder(
    CdmHostProxy* cdm_host_proxy,
    const cdm::VideoDecoderConfig_3& config) {
  SetupGlobalEnvironmentIfNeeded();

  static base::NoDestructor<media::NullMediaLog> null_media_log;
  std::unique_ptr<VideoDecoder> video_decoder;

#if BUILDFLAG(ENABLE_LIBVPX)
  if (config.codec == cdm::kCodecVp8 || config.codec == cdm::kCodecVp9)
    video_decoder.reset(new VpxVideoDecoder());
#endif

#if BUILDFLAG(ENABLE_DAV1D_DECODER)
  if (config.codec == cdm::kCodecAv1)
    video_decoder.reset(new Dav1dVideoDecoder(null_media_log.get()));
#elif BUILDFLAG(ENABLE_LIBAOM_DECODER)
  if (config.codec == cdm::kCodecAv1)
    video_decoder.reset(new AomVideoDecoder(null_media_log.get()));
#endif

#if BUILDFLAG(ENABLE_FFMPEG)
  if (!video_decoder)
    video_decoder.reset(new FFmpegVideoDecoder(null_media_log.get()));
#endif

  if (!video_decoder)
    return nullptr;

  return std::make_unique<VideoDecoderAdapter>(cdm_host_proxy,
                                               std::move(video_decoder));
}

}  // namespace media
