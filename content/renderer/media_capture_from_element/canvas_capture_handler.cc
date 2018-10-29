// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media_capture_from_element/canvas_capture_handler.h"

#include <utility>

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/viz/common/gl_helper.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/media/stream/media_stream_constraints_util.h"
#include "content/renderer/media/stream/media_stream_video_capturer_source.h"
#include "content/renderer/media/stream/media_stream_video_source.h"
#include "content/renderer/media/stream/media_stream_video_track.h"
#include "content/renderer/media/webrtc/webrtc_uma_histograms.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/limits.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"

using media::VideoFrame;

namespace content {

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
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    media::VideoCaptureFormats formats;
    formats.push_back(media::VideoCaptureFormat(size_, frame_rate_,
                                                media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(size_, frame_rate_,
                                                media::PIXEL_FORMAT_I420A));
    return formats;
  }
  void StartCapture(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& frame_callback,
                    const RunningCallback& running_callback) override {
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    if (canvas_handler_.get()) {
      canvas_handler_->StartVideoCapture(params, frame_callback,
                                         running_callback);
    }
  }
  void RequestRefreshFrame() override {
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    if (canvas_handler_.get())
      canvas_handler_->RequestRefreshFrame();
  }
  void StopCapture() override {
    DCHECK(main_render_thread_checker_.CalledOnValidThread());
    if (canvas_handler_.get())
      canvas_handler_->StopVideoCapture();
  }

 private:
  const blink::WebSize size_;
  const float frame_rate_;
  // Bound to Main Render thread.
  base::ThreadChecker main_render_thread_checker_;
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
      : new_frame_callback_(new_frame_callback), weak_ptr_factory_(this) {
    io_thread_checker_.DetachFromThread();
  }
  ~CanvasCaptureHandlerDelegate() {
    DCHECK(io_thread_checker_.CalledOnValidThread());
  }

  void SendNewFrameOnIOThread(scoped_refptr<VideoFrame> video_frame,
                              base::TimeTicks current_time) {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    new_frame_callback_.Run(std::move(video_frame), current_time);
  }

  base::WeakPtr<CanvasCaptureHandlerDelegate> GetWeakPtrForIOThread() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const media::VideoCapturerSource::VideoCaptureDeliverFrameCB
      new_frame_callback_;
  // Bound to IO thread.
  base::ThreadChecker io_thread_checker_;
  base::WeakPtrFactory<CanvasCaptureHandlerDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CanvasCaptureHandlerDelegate);
};

CanvasCaptureHandler::CanvasCaptureHandler(
    const blink::WebSize& size,
    double frame_rate,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    blink::WebMediaStreamTrack* track)
    : ask_for_new_frame_(false),
      io_task_runner_(std::move(io_task_runner)),
      weak_ptr_factory_(this) {
  std::unique_ptr<media::VideoCapturerSource> video_source(
      new VideoCapturerSource(weak_ptr_factory_.GetWeakPtr(), size,
                              frame_rate));
  AddVideoCapturerSourceToVideoTrack(std::move(video_source), track);
}

CanvasCaptureHandler::~CanvasCaptureHandler() {
  DVLOG(3) << __func__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

// static
std::unique_ptr<CanvasCaptureHandler>
CanvasCaptureHandler::CreateCanvasCaptureHandler(
    const blink::WebSize& size,
    double frame_rate,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    blink::WebMediaStreamTrack* track) {
  // Save histogram data so we can see how much CanvasCapture is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(blink::WebRTCAPIName::kCanvasCaptureStream);

  return std::unique_ptr<CanvasCaptureHandler>(new CanvasCaptureHandler(
      size, frame_rate, std::move(io_task_runner), track));
}

void CanvasCaptureHandler::SendNewFrame(
    sk_sp<SkImage> image,
    blink::WebGraphicsContext3DProvider* context_provider) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(image);
  TRACE_EVENT0("webrtc", "CanvasCaptureHandler::SendNewFrame");

  // Initially try accessing pixels directly if they are in memory.
  SkPixmap pixmap;
  if (image->peekPixels(&pixmap) &&
      (pixmap.colorType() == kRGBA_8888_SkColorType ||
       pixmap.colorType() == kBGRA_8888_SkColorType) &&
      (pixmap.alphaType() == kUnpremul_SkAlphaType || image->isOpaque())) {
    const base::TimeTicks timestamp = base::TimeTicks::Now();
    SendFrame(ConvertToYUVFrame(image->isOpaque(), false,
                                static_cast<const uint8_t*>(pixmap.addr(0, 0)),
                                gfx::Size(pixmap.width(), pixmap.height()),
                                pixmap.rowBytes(), pixmap.colorType()),
              timestamp);
    return;
  }

  // Copy the pixels into memory synchronously. This call may block the main
  // render thread.
  if (!image->isTextureBacked()) {
    ReadARGBPixelsSync(image);
    return;
  }

  if (!context_provider) {
    DLOG(ERROR) << "Context lost, skipping frame";
    return;
  }

  // Try async reading SkImage if it is texture backed.
  if (image->isOpaque()) {
    ReadYUVPixelsAsync(image, context_provider);
  } else {
    ReadARGBPixelsAsync(image, context_provider);
  }
}

bool CanvasCaptureHandler::NeedsNewFrame() const {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  return ask_for_new_frame_;
}

void CanvasCaptureHandler::StartVideoCapture(
    const media::VideoCaptureParams& params,
    const media::VideoCapturerSource::VideoCaptureDeliverFrameCB&
        new_frame_callback,
    const media::VideoCapturerSource::RunningCallback& running_callback) {
  DVLOG(3) << __func__ << " requested "
           << media::VideoCaptureFormat::ToString(params.requested_format);
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(params.requested_format.IsValid());
  capture_format_ = params.requested_format;
  delegate_.reset(new CanvasCaptureHandlerDelegate(new_frame_callback));
  DCHECK(delegate_);
  ask_for_new_frame_ = true;
  running_callback.Run(true);
}

void CanvasCaptureHandler::RequestRefreshFrame() {
  DVLOG(3) << __func__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (last_frame_ && delegate_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CanvasCaptureHandler::CanvasCaptureHandlerDelegate::
                           SendNewFrameOnIOThread,
                       delegate_->GetWeakPtrForIOThread(), last_frame_,
                       base::TimeTicks::Now()));
  }
}

void CanvasCaptureHandler::StopVideoCapture() {
  DVLOG(3) << __func__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  ask_for_new_frame_ = false;
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

void CanvasCaptureHandler::ReadARGBPixelsSync(sk_sp<SkImage> image) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::Size image_size(image->width(), image->height());
  scoped_refptr<VideoFrame> temp_argb_frame = frame_pool_.CreateFrame(
      media::PIXEL_FORMAT_ARGB, image_size, gfx::Rect(image_size), image_size,
      base::TimeDelta());
  if (!temp_argb_frame) {
    DLOG(ERROR) << "Couldn't allocate video frame";
    return;
  }
  const bool is_opaque = image->isOpaque();
  SkImageInfo image_info = SkImageInfo::MakeN32(
      image_size.width(), image_size.height(),
      is_opaque ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  if (!image->readPixels(image_info,
                         temp_argb_frame->visible_data(VideoFrame::kARGBPlane),
                         temp_argb_frame->stride(VideoFrame::kARGBPlane),
                         0 /*srcX*/, 0 /*srcY*/)) {
    DLOG(ERROR) << "Couldn't read SkImage using readPixels()";
    return;
  }
  SendFrame(
      ConvertToYUVFrame(
          is_opaque, false /* flip */,
          temp_argb_frame->visible_data(VideoFrame::kARGBPlane), image_size,
          temp_argb_frame->stride(VideoFrame::kARGBPlane), kN32_SkColorType),
      timestamp);
}

void CanvasCaptureHandler::ReadARGBPixelsAsync(
    sk_sp<SkImage> image,
    blink::WebGraphicsContext3DProvider* context_provider) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::Size image_size(image->width(), image->height());
  scoped_refptr<VideoFrame> temp_argb_frame = frame_pool_.CreateFrame(
      media::PIXEL_FORMAT_ARGB, image_size, gfx::Rect(image_size), image_size,
      base::TimeDelta());
  if (!temp_argb_frame) {
    DLOG(ERROR) << "Couldn't allocate video frame";
    return;
  }
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  GrBackendTexture backend_texture =
      image->getBackendTexture(true, &surface_origin);
  DCHECK(backend_texture.isValid());
  GrGLTextureInfo texture_info;
  const bool result = backend_texture.getGLTextureInfo(&texture_info);
  DCHECK(result);
  DCHECK(context_provider->GetGLHelper());
  context_provider->GetGLHelper()->ReadbackTextureAsync(
      texture_info.fID, image_size,
      temp_argb_frame->visible_data(VideoFrame::kARGBPlane), kN32_SkColorType,
      base::BindOnce(&CanvasCaptureHandler::OnARGBPixelsReadAsync,
                     weak_ptr_factory_.GetWeakPtr(), image, temp_argb_frame,
                     timestamp, surface_origin != kTopLeft_GrSurfaceOrigin));
}

void CanvasCaptureHandler::ReadYUVPixelsAsync(
    sk_sp<SkImage> image,
    blink::WebGraphicsContext3DProvider* context_provider) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::Size image_size(image->width(), image->height());
  scoped_refptr<VideoFrame> output_frame = frame_pool_.CreateFrame(
      media::PIXEL_FORMAT_I420, image_size, gfx::Rect(image_size), image_size,
      base::TimeDelta());
  if (!output_frame) {
    DLOG(ERROR) << "Couldn't allocate video frame";
    return;
  }

  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  GrBackendTexture backend_texture =
      image->getBackendTexture(true, &surface_origin);
  DCHECK(backend_texture.isValid());
  GrGLTextureInfo texture_info;
  const bool result = backend_texture.getGLTextureInfo(&texture_info);
  DCHECK(result);
  DCHECK(context_provider->GetGLHelper());
  const gpu::MailboxHolder& mailbox_holder =
      context_provider->GetGLHelper()->ProduceMailboxHolderFromTexture(
          texture_info.fID);
  DCHECK_EQ(static_cast<int>(texture_info.fTarget), GL_TEXTURE_2D);
  viz::ReadbackYUVInterface* const yuv_reader =
      context_provider->GetGLHelper()->GetReadbackPipelineYUV(
          surface_origin != kTopLeft_GrSurfaceOrigin);
  yuv_reader->ReadbackYUV(
      mailbox_holder.mailbox, mailbox_holder.sync_token, image_size,
      gfx::Rect(image_size), output_frame->stride(media::VideoFrame::kYPlane),
      output_frame->visible_data(media::VideoFrame::kYPlane),
      output_frame->stride(media::VideoFrame::kUPlane),
      output_frame->visible_data(media::VideoFrame::kUPlane),
      output_frame->stride(media::VideoFrame::kVPlane),
      output_frame->visible_data(media::VideoFrame::kVPlane), gfx::Point(0, 0),
      base::BindOnce(&CanvasCaptureHandler::OnYUVPixelsReadAsync,
                     weak_ptr_factory_.GetWeakPtr(), image, output_frame,
                     timestamp));
}

void CanvasCaptureHandler::OnARGBPixelsReadAsync(
    sk_sp<SkImage> image,
    scoped_refptr<media::VideoFrame> temp_argb_frame,
    base::TimeTicks this_frame_ticks,
    bool flip,
    bool success) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (!success) {
    DLOG(ERROR) << "Couldn't read SkImage using async callback";
    // Async reading is not supported on some platforms, see
    // http://crbug.com/788386.
    ReadARGBPixelsSync(image);
    return;
  }
  // Let |image| fall out of scope after we are done reading.
  const bool is_opaque = image->isOpaque();
  image = nullptr;

  SendFrame(
      ConvertToYUVFrame(is_opaque, flip,
                        temp_argb_frame->visible_data(VideoFrame::kARGBPlane),
                        temp_argb_frame->visible_rect().size(),
                        temp_argb_frame->stride(VideoFrame::kARGBPlane),
                        kN32_SkColorType),
      this_frame_ticks);
}

void CanvasCaptureHandler::OnYUVPixelsReadAsync(
    sk_sp<SkImage> /* image */,
    scoped_refptr<media::VideoFrame> yuv_frame,
    base::TimeTicks this_frame_ticks,
    bool success) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (!success) {
    DLOG(ERROR) << "Couldn't read SkImage using async callback";
    return;
  }
  SendFrame(yuv_frame, this_frame_ticks);
}

scoped_refptr<media::VideoFrame> CanvasCaptureHandler::ConvertToYUVFrame(
    bool is_opaque,
    bool flip,
    const uint8_t* source_ptr,
    const gfx::Size& image_size,
    int stride,
    SkColorType source_color_type) {
  DVLOG(4) << __func__;
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
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
                                     base::TimeTicks this_frame_ticks) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  // If this function is called asynchronously, |delegate_| might have been
  // released already in StopVideoCapture().
  if (!delegate_ || !video_frame)
    return;

  if (!first_frame_ticks_)
    first_frame_ticks_ = this_frame_ticks;
  video_frame->set_timestamp(this_frame_ticks - *first_frame_ticks_);

  last_frame_ = video_frame;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CanvasCaptureHandler::CanvasCaptureHandlerDelegate::
                         SendNewFrameOnIOThread,
                     delegate_->GetWeakPtrForIOThread(), std::move(video_frame),
                     this_frame_ticks));
}

void CanvasCaptureHandler::AddVideoCapturerSourceToVideoTrack(
    std::unique_ptr<media::VideoCapturerSource> source,
    blink::WebMediaStreamTrack* web_track) {
  std::string str_track_id;
  base::Base64Encode(base::RandBytesAsString(64), &str_track_id);
  const blink::WebString track_id = blink::WebString::FromASCII(str_track_id);
  media::VideoCaptureFormats preferred_formats = source->GetPreferredFormats();
  std::unique_ptr<MediaStreamVideoSource> media_stream_source(
      new MediaStreamVideoCapturerSource(
          MediaStreamSource::SourceStoppedCallback(), std::move(source)));
  blink::WebMediaStreamSource webkit_source;
  webkit_source.Initialize(track_id, blink::WebMediaStreamSource::kTypeVideo,
                           track_id, false);
  webkit_source.SetExtraData(media_stream_source.get());
  webkit_source.SetCapabilities(ComputeCapabilitiesForVideoSource(
      track_id, preferred_formats,
      media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE,
      false /* is_device_capture */));

  web_track->Initialize(webkit_source);
  web_track->SetTrackData(new MediaStreamVideoTrack(
      media_stream_source.release(),
      MediaStreamVideoSource::ConstraintsCallback(), true));
}

}  // namespace content
