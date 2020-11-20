// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/canvas_capture_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/limits.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/color_space.h"

using media::VideoFrame;

namespace blink {

namespace {

// Return the gfx::ColorSpace that the pixels resulting from calling
// ConvertToYUVFrame on |image| will be in.
gfx::ColorSpace GetImageYUVColorSpace(scoped_refptr<StaticBitmapImage> image) {
  // TODO: Determine the ColorSpace::MatrixID and ColorSpace::RangeID that the
  // calls to libyuv are assuming.
  return gfx::ColorSpace();
}

}  // namespace

// Implementation VideoCapturerSource that is owned by
// MediaStreamVideoCapturerSource and delegates the Start/Stop calls to
// CanvasCaptureHandler.
// This class is single threaded and pinned to main render thread.
class VideoCapturerSource : public media::VideoCapturerSource {
 public:
  VideoCapturerSource(base::WeakPtr<CanvasCaptureHandler> canvas_handler,
                      const blink::WebSize& size,
                      double frame_rate)
      : size_(size),
        frame_rate_(static_cast<float>(
            std::min(static_cast<double>(media::limits::kMaxFramesPerSecond),
                     frame_rate))),
        canvas_handler_(canvas_handler) {
    DCHECK_LE(0, frame_rate_);
  }

 protected:
  media::VideoCaptureFormats GetPreferredFormats() override {
    DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
    media::VideoCaptureFormats formats;
    formats.push_back(media::VideoCaptureFormat(gfx::Size(size_), frame_rate_,
                                                media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(gfx::Size(size_), frame_rate_,
                                                media::PIXEL_FORMAT_I420A));
    return formats;
  }
  void StartCapture(const media::VideoCaptureParams& params,
                    const blink::VideoCaptureDeliverFrameCB& frame_callback,
                    const RunningCallback& running_callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
    if (canvas_handler_.get()) {
      canvas_handler_->StartVideoCapture(params, frame_callback,
                                         running_callback);
    }
  }
  void RequestRefreshFrame() override {
    DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
    if (canvas_handler_.get())
      canvas_handler_->RequestRefreshFrame();
  }
  void StopCapture() override {
    DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
    if (canvas_handler_.get())
      canvas_handler_->StopVideoCapture();
  }

 private:
  const blink::WebSize size_;
  const float frame_rate_;
  // Bound to Main Render thread.
  THREAD_CHECKER(main_render_thread_checker_);
  // CanvasCaptureHandler is owned by CanvasDrawListener in blink. It is
  // guaranteed to be destroyed on Main Render thread and it would happen
  // independently of this class. Therefore, WeakPtr should always be checked
  // before use.
  base::WeakPtr<CanvasCaptureHandler> canvas_handler_;
};

class CanvasCaptureHandler::CanvasCaptureHandlerDelegate {
 public:
  explicit CanvasCaptureHandlerDelegate(
      media::VideoCapturerSource::VideoCaptureDeliverFrameCB new_frame_callback)
      : new_frame_callback_(new_frame_callback) {
    DETACH_FROM_THREAD(io_thread_checker_);
  }
  ~CanvasCaptureHandlerDelegate() {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  }

  void SendNewFrameOnIOThread(scoped_refptr<VideoFrame> video_frame,
                              base::TimeTicks current_time) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    new_frame_callback_.Run(std::move(video_frame), current_time);
  }

  base::WeakPtr<CanvasCaptureHandlerDelegate> GetWeakPtrForIOThread() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const media::VideoCapturerSource::VideoCaptureDeliverFrameCB
      new_frame_callback_;
  // Bound to IO thread.
  THREAD_CHECKER(io_thread_checker_);
  base::WeakPtrFactory<CanvasCaptureHandlerDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CanvasCaptureHandlerDelegate);
};

CanvasCaptureHandler::CanvasCaptureHandler(
    LocalFrame* frame,
    const blink::WebSize& size,
    double frame_rate,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    MediaStreamComponent** component)
    : ask_for_new_frame_(false), io_task_runner_(std::move(io_task_runner)) {
  std::unique_ptr<media::VideoCapturerSource> video_source(
      new VideoCapturerSource(weak_ptr_factory_.GetWeakPtr(), size,
                              frame_rate));
  AddVideoCapturerSourceToVideoTrack(frame, std::move(video_source), component);
}

CanvasCaptureHandler::~CanvasCaptureHandler() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

// static
std::unique_ptr<CanvasCaptureHandler>
CanvasCaptureHandler::CreateCanvasCaptureHandler(
    LocalFrame* frame,
    const blink::WebSize& size,
    double frame_rate,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    MediaStreamComponent** component) {
  // Save histogram data so we can see how much CanvasCapture is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(RTCAPIName::kCanvasCaptureStream);

  return std::unique_ptr<CanvasCaptureHandler>(new CanvasCaptureHandler(
      frame, size, frame_rate, std::move(io_task_runner), component));
}

void CanvasCaptureHandler::SendNewFrame(
    scoped_refptr<StaticBitmapImage> image,
    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
        context_provider) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  TRACE_EVENT0("webrtc", "CanvasCaptureHandler::SendNewFrame");
  if (!image)
    return;

  if (!image->IsTextureBacked()) {
    // Initially try accessing pixels directly if they are in memory.
    sk_sp<SkImage> sk_image = image->PaintImageForCurrentFrame().GetSwSkImage();
    SkPixmap pixmap;
    if (sk_image->peekPixels(&pixmap) &&
        (pixmap.colorType() == kRGBA_8888_SkColorType ||
         pixmap.colorType() == kBGRA_8888_SkColorType) &&
        (pixmap.alphaType() == kUnpremul_SkAlphaType || sk_image->isOpaque())) {
      const base::TimeTicks timestamp = base::TimeTicks::Now();
      SendFrame(ConvertToYUVFrame(
                    sk_image->isOpaque(), false,
                    static_cast<const uint8_t*>(pixmap.addr(0, 0)),
                    gfx::Size(pixmap.width(), pixmap.height()),
                    static_cast<int>(pixmap.rowBytes()), pixmap.colorType()),
                timestamp, GetImageYUVColorSpace(image));
      return;
    }

    // Copy the pixels into memory synchronously. This call may block the main
    // render thread.
    ReadARGBPixelsSync(image);
    return;
  }

  if (!context_provider) {
    DLOG(ERROR) << "Context lost, skipping frame";
    return;
  }

  // Try async reading if image is texture backed.
  if (image->CurrentFrameKnownToBeOpaque()) {
    ReadYUVPixelsAsync(image, context_provider);
  } else {
    ReadARGBPixelsAsync(image, context_provider->ContextProvider());
  }
}

bool CanvasCaptureHandler::NeedsNewFrame() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  return ask_for_new_frame_;
}

void CanvasCaptureHandler::StartVideoCapture(
    const media::VideoCaptureParams& params,
    const media::VideoCapturerSource::VideoCaptureDeliverFrameCB&
        new_frame_callback,
    const media::VideoCapturerSource::RunningCallback& running_callback) {
  DVLOG(3) << __func__ << " requested "
           << media::VideoCaptureFormat::ToString(params.requested_format);
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(params.requested_format.IsValid());
  capture_format_ = params.requested_format;
  delegate_.reset(new CanvasCaptureHandlerDelegate(new_frame_callback));
  DCHECK(delegate_);
  ask_for_new_frame_ = true;
  running_callback.Run(true);
}

void CanvasCaptureHandler::RequestRefreshFrame() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (last_frame_ && delegate_) {
    // If we're currently reading out pixels from GL memory, we risk
    // emitting frames with non-incrementally increasing timestamps.
    // Defer sending the refresh frame until we have completed those async
    // reads.
    if (num_ongoing_async_pixel_readouts_ > 0) {
      deferred_request_refresh_frame_ = true;
      return;
    }
    SendRefreshFrame();
  }
}

void CanvasCaptureHandler::StopVideoCapture() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  ask_for_new_frame_ = false;
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

void CanvasCaptureHandler::ReadARGBPixelsSync(
    scoped_refptr<StaticBitmapImage> image) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  PaintImage paint_image = image->PaintImageForCurrentFrame();
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::Size image_size(paint_image.width(), paint_image.height());
  scoped_refptr<VideoFrame> temp_argb_frame = frame_pool_.CreateFrame(
      media::PIXEL_FORMAT_ARGB, image_size, gfx::Rect(image_size), image_size,
      base::TimeDelta());
  if (!temp_argb_frame) {
    DLOG(ERROR) << "Couldn't allocate video frame";
    return;
  }
  const bool is_opaque = paint_image.IsOpaque();
  SkImageInfo image_info = SkImageInfo::MakeN32(
      image_size.width(), image_size.height(),
      is_opaque ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  if (!paint_image.readPixels(
          image_info, temp_argb_frame->visible_data(VideoFrame::kARGBPlane),
          temp_argb_frame->stride(VideoFrame::kARGBPlane), 0 /*srcX*/,
          0 /*srcY*/)) {
    DLOG(ERROR) << "Couldn't read pixels from PaintImage";
    return;
  }
  SendFrame(
      ConvertToYUVFrame(
          is_opaque, false /* flip */,
          temp_argb_frame->visible_data(VideoFrame::kARGBPlane), image_size,
          temp_argb_frame->stride(VideoFrame::kARGBPlane), kN32_SkColorType),
      timestamp, GetImageYUVColorSpace(image));
}

void CanvasCaptureHandler::ReadARGBPixelsAsync(
    scoped_refptr<StaticBitmapImage> image,
    blink::WebGraphicsContext3DProvider* context_provider) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(context_provider);

  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::Size image_size(image->width(), image->height());
  scoped_refptr<VideoFrame> temp_argb_frame = frame_pool_.CreateFrame(
      media::PIXEL_FORMAT_ARGB, image_size, gfx::Rect(image_size), image_size,
      base::TimeDelta());
  if (!temp_argb_frame) {
    DLOG(ERROR) << "Couldn't allocate video frame";
    return;
  }

  static_assert(kN32_SkColorType == kRGBA_8888_SkColorType ||
                    kN32_SkColorType == kBGRA_8888_SkColorType,
                "CanvasCaptureHandler::ReadARGBPixelsAsync supports only "
                "kRGBA_8888_SkColorType and kBGRA_8888_SkColorType.");
  GLenum format;
  if (kN32_SkColorType == kRGBA_8888_SkColorType)
    format = GL_RGBA;
  else
    format = GL_BGRA_EXT;

  IncrementOngoingAsyncPixelReadouts();
  gpu::MailboxHolder mailbox_holder = image->GetMailboxHolder();
  DCHECK(context_provider->RasterInterface());
  context_provider->RasterInterface()->WaitSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetConstData());
  context_provider->RasterInterface()->ReadbackARGBPixelsAsync(
      mailbox_holder.mailbox, mailbox_holder.texture_target, image_size,
      temp_argb_frame->visible_data(VideoFrame::kARGBPlane), format,
      WTF::Bind(&CanvasCaptureHandler::OnARGBPixelsReadAsync,
                weak_ptr_factory_.GetWeakPtr(), image, temp_argb_frame,
                timestamp, !image->IsOriginTopLeft()));
}

void CanvasCaptureHandler::ReadYUVPixelsAsync(
    scoped_refptr<StaticBitmapImage> image,
    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
        context_provider) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(context_provider);

  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::Size image_size(image->width(), image->height());
  scoped_refptr<VideoFrame> output_frame = frame_pool_.CreateFrame(
      media::PIXEL_FORMAT_I420, image_size, gfx::Rect(image_size), image_size,
      base::TimeDelta());
  if (!output_frame) {
    DLOG(ERROR) << "Couldn't allocate video frame";
    return;
  }

  gpu::MailboxHolder mailbox_holder = image->GetMailboxHolder();
  context_provider->ContextProvider()->RasterInterface()->WaitSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetConstData());
  context_provider->ContextProvider()
      ->RasterInterface()
      ->ReadbackYUVPixelsAsync(
          mailbox_holder.mailbox, mailbox_holder.texture_target, image_size,
          gfx::Rect(image_size), !image->IsOriginTopLeft(),
          output_frame->stride(media::VideoFrame::kYPlane),
          output_frame->visible_data(media::VideoFrame::kYPlane),
          output_frame->stride(media::VideoFrame::kUPlane),
          output_frame->visible_data(media::VideoFrame::kUPlane),
          output_frame->stride(media::VideoFrame::kVPlane),
          output_frame->visible_data(media::VideoFrame::kVPlane),
          gfx::Point(0, 0),
          WTF::Bind(&CanvasCaptureHandler::OnReleaseMailbox,
                    weak_ptr_factory_.GetWeakPtr(), image),
          WTF::Bind(&CanvasCaptureHandler::OnYUVPixelsReadAsync,
                    weak_ptr_factory_.GetWeakPtr(), output_frame, timestamp));
}

void CanvasCaptureHandler::OnARGBPixelsReadAsync(
    scoped_refptr<StaticBitmapImage> image,
    scoped_refptr<media::VideoFrame> temp_argb_frame,
    base::TimeTicks this_frame_ticks,
    bool flip,
    bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DecrementOngoingAsyncPixelReadouts();
  if (!success) {
    DLOG(ERROR) << "Couldn't read SkImage using async callback";
    // Async reading is not supported on some platforms, see
    // http://crbug.com/788386.
    ReadARGBPixelsSync(image);
    return;
  }
  // Let |image| fall out of scope after we are done reading.
  const bool is_opaque = image->CurrentFrameKnownToBeOpaque();
  const auto color_space = GetImageYUVColorSpace(image);

  SendFrame(
      ConvertToYUVFrame(is_opaque, flip,
                        temp_argb_frame->visible_data(VideoFrame::kARGBPlane),
                        temp_argb_frame->visible_rect().size(),
                        temp_argb_frame->stride(VideoFrame::kARGBPlane),
                        kN32_SkColorType),
      this_frame_ticks, color_space);
  if (num_ongoing_async_pixel_readouts_ == 0 && deferred_request_refresh_frame_)
    SendRefreshFrame();
}

void CanvasCaptureHandler::OnYUVPixelsReadAsync(
    scoped_refptr<media::VideoFrame> yuv_frame,
    base::TimeTicks this_frame_ticks,
    bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  if (!success) {
    DLOG(ERROR) << "Couldn't read SkImage using async callback";
    return;
  }
  SendFrame(yuv_frame, this_frame_ticks, gfx::ColorSpace());
}

void CanvasCaptureHandler::OnReleaseMailbox(
    scoped_refptr<StaticBitmapImage> image) {
  // All shared image operations have been completed, stop holding the ref.
  image = nullptr;
}

scoped_refptr<media::VideoFrame> CanvasCaptureHandler::ConvertToYUVFrame(
    bool is_opaque,
    bool flip,
    const uint8_t* source_ptr,
    const gfx::Size& image_size,
    int stride,
    SkColorType source_color_type) {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  TRACE_EVENT0("webrtc", "CanvasCaptureHandler::ConvertToYUVFrame");

  scoped_refptr<VideoFrame> video_frame = frame_pool_.CreateFrame(
      is_opaque ? media::PIXEL_FORMAT_I420 : media::PIXEL_FORMAT_I420A,
      image_size, gfx::Rect(image_size), image_size, base::TimeDelta());
  if (!video_frame) {
    DLOG(ERROR) << "Couldn't allocate video frame";
    return nullptr;
  }

  int (*ConvertToI420)(const uint8_t* src_argb, int src_stride_argb,
                       uint8_t* dst_y, int dst_stride_y, uint8_t* dst_u,
                       int dst_stride_u, uint8_t* dst_v, int dst_stride_v,
                       int width, int height) = nullptr;
  switch (source_color_type) {
    case kRGBA_8888_SkColorType:
      ConvertToI420 = libyuv::ABGRToI420;
      break;
    case kBGRA_8888_SkColorType:
      ConvertToI420 = libyuv::ARGBToI420;
      break;
    default:
      NOTIMPLEMENTED() << "Unexpected SkColorType.";
      return nullptr;
  }

  if (ConvertToI420(source_ptr, stride,
                    video_frame->visible_data(media::VideoFrame::kYPlane),
                    video_frame->stride(media::VideoFrame::kYPlane),
                    video_frame->visible_data(media::VideoFrame::kUPlane),
                    video_frame->stride(media::VideoFrame::kUPlane),
                    video_frame->visible_data(media::VideoFrame::kVPlane),
                    video_frame->stride(media::VideoFrame::kVPlane),
                    image_size.width(),
                    (flip ? -1 : 1) * image_size.height()) != 0) {
    DLOG(ERROR) << "Couldn't convert to I420";
    return nullptr;
  }
  if (!is_opaque) {
    // It is ok to use ARGB function because alpha has the same alignment for
    // both ABGR and ARGB.
    libyuv::ARGBExtractAlpha(
        source_ptr, stride, video_frame->visible_data(VideoFrame::kAPlane),
        video_frame->stride(VideoFrame::kAPlane), image_size.width(),
        (flip ? -1 : 1) * image_size.height());
  }

  return video_frame;
}

void CanvasCaptureHandler::SendFrame(scoped_refptr<VideoFrame> video_frame,
                                     base::TimeTicks this_frame_ticks,
                                     const gfx::ColorSpace& color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  // If this function is called asynchronously, |delegate_| might have been
  // released already in StopVideoCapture().
  if (!delegate_ || !video_frame)
    return;

  if (!first_frame_ticks_)
    first_frame_ticks_ = this_frame_ticks;
  video_frame->set_timestamp(this_frame_ticks - *first_frame_ticks_);
  if (color_space.IsValid())
    video_frame->set_color_space(color_space);

  last_frame_ = video_frame;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CanvasCaptureHandler::CanvasCaptureHandlerDelegate::
                         SendNewFrameOnIOThread,
                     delegate_->GetWeakPtrForIOThread(), std::move(video_frame),
                     this_frame_ticks));
}

void CanvasCaptureHandler::AddVideoCapturerSourceToVideoTrack(
    LocalFrame* frame,
    std::unique_ptr<media::VideoCapturerSource> source,
    MediaStreamComponent** component) {
  uint8_t track_id_bytes[64];
  base::RandBytes(track_id_bytes, sizeof(track_id_bytes));
  String track_id = Base64Encode(track_id_bytes);
  media::VideoCaptureFormats preferred_formats = source->GetPreferredFormats();
  auto stream_video_source = std::make_unique<MediaStreamVideoCapturerSource>(
      frame, WebPlatformMediaStreamSource::SourceStoppedCallback(),
      std::move(source));
  auto* stream_video_source_ptr = stream_video_source.get();
  auto* stream_source = MakeGarbageCollected<MediaStreamSource>(
      track_id, MediaStreamSource::kTypeVideo, track_id, false);
  stream_source->SetPlatformSource(std::move(stream_video_source));
  stream_source->SetCapabilities(ComputeCapabilitiesForVideoSource(
      track_id, preferred_formats,
      media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE,
      false /* is_device_capture */));

  *component = MakeGarbageCollected<MediaStreamComponent>(stream_source);
  (*component)
      ->SetPlatformTrack(std::make_unique<MediaStreamVideoTrack>(
          stream_video_source_ptr,
          MediaStreamVideoSource::ConstraintsOnceCallback(), true));
}

void CanvasCaptureHandler::IncrementOngoingAsyncPixelReadouts() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  ++num_ongoing_async_pixel_readouts_;
}

void CanvasCaptureHandler::DecrementOngoingAsyncPixelReadouts() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  --num_ongoing_async_pixel_readouts_;
  DCHECK_GE(num_ongoing_async_pixel_readouts_, 0);
}

void CanvasCaptureHandler::SendRefreshFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK_EQ(num_ongoing_async_pixel_readouts_, 0);
  if (last_frame_ && delegate_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CanvasCaptureHandler::CanvasCaptureHandlerDelegate::
                           SendNewFrameOnIOThread,
                       delegate_->GetWeakPtrForIOThread(), last_frame_,
                       base::TimeTicks::Now()));
  }
  deferred_request_refresh_frame_ = false;
}

}  // namespace blink
