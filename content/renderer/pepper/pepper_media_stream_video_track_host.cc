// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/renderer/pepper/pepper_media_stream_video_track_host.h"

#include <stddef.h>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/video_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_media_stream_video_track.h"
#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_utils.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/gpu_memory_buffer.h"

using media::VideoFrame;
using ppapi::host::HostMessageContext;
using ppapi::MediaStreamVideoTrackShared;

namespace {

const int32_t kDefaultNumberOfVideoBuffers = 4;
const int32_t kMaxNumberOfVideoBuffers = 8;
// Filter mode for scaling frames.
const libyuv::FilterMode kFilterMode = libyuv::kFilterBox;

const char kPepperVideoSourceName[] = "PepperVideoSourceName";

// Default config for output mode.
const int kDefaultOutputFrameRate = 30;

media::VideoPixelFormat ToPixelFormat(PP_VideoFrame_Format format) {
  switch (format) {
    case PP_VIDEOFRAME_FORMAT_YV12:
      return media::PIXEL_FORMAT_YV12;
    case PP_VIDEOFRAME_FORMAT_I420:
      return media::PIXEL_FORMAT_I420;
    default:
      DVLOG(1) << "Unsupported pixel format " << format;
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

PP_VideoFrame_Format ToPpapiFormat(media::VideoPixelFormat format) {
  switch (format) {
    case media::PIXEL_FORMAT_YV12:
      return PP_VIDEOFRAME_FORMAT_YV12;
    case media::PIXEL_FORMAT_I420:
      return PP_VIDEOFRAME_FORMAT_I420;
    default:
      DVLOG(1) << "Unsupported pixel format " << format;
      return PP_VIDEOFRAME_FORMAT_UNKNOWN;
  }
}

media::VideoPixelFormat FromPpapiFormat(PP_VideoFrame_Format format) {
  switch (format) {
    case PP_VIDEOFRAME_FORMAT_YV12:
      return media::PIXEL_FORMAT_YV12;
    case PP_VIDEOFRAME_FORMAT_I420:
      return media::PIXEL_FORMAT_I420;
    default:
      DVLOG(1) << "Unsupported pixel format " << format;
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

// Compute size base on the size of frame received from MediaStreamVideoSink
// and size specified by plugin.
gfx::Size GetTargetSize(const gfx::Size& source, const gfx::Size& plugin) {
  return gfx::Size(plugin.width() ? plugin.width() : source.width(),
                   plugin.height() ? plugin.height() : source.height());
}

// Compute format base on the format of frame received from MediaStreamVideoSink
// and format specified by plugin.
PP_VideoFrame_Format GetTargetFormat(PP_VideoFrame_Format source,
                                     PP_VideoFrame_Format plugin) {
  return plugin != PP_VIDEOFRAME_FORMAT_UNKNOWN ? plugin : source;
}

void ConvertFromMediaVideoFrame(const media::VideoFrame& src,
                                PP_VideoFrame_Format dst_format,
                                const gfx::Size& dst_size,
                                uint8_t* dst) {
  CHECK(src.format() == media::PIXEL_FORMAT_YV12 ||
        src.format() == media::PIXEL_FORMAT_I420);
  if (dst_format == PP_VIDEOFRAME_FORMAT_BGRA) {
    if (src.visible_rect().size() == dst_size) {
      libyuv::I420ToARGB(src.visible_data(VideoFrame::Plane::kY),
                         src.stride(VideoFrame::Plane::kY),
                         src.visible_data(VideoFrame::Plane::kU),
                         src.stride(VideoFrame::Plane::kU),
                         src.visible_data(VideoFrame::Plane::kV),
                         src.stride(VideoFrame::Plane::kV), dst,
                         dst_size.width() * 4, dst_size.width(),
                         dst_size.height());
    } else {
      libyuv::YUVToARGBScaleClip(
          src.visible_data(VideoFrame::Plane::kY),
          src.stride(VideoFrame::Plane::kY),
          src.visible_data(VideoFrame::Plane::kU),
          src.stride(VideoFrame::Plane::kU),
          src.visible_data(VideoFrame::Plane::kV),
          src.stride(VideoFrame::Plane::kV), libyuv::FOURCC_YV12,
          src.visible_rect().width(), src.visible_rect().height(), dst,
          dst_size.width() * 4, libyuv::FOURCC_ARGB, dst_size.width(),
          dst_size.height(), 0, 0, dst_size.width(), dst_size.height(),
          kFilterMode);
    }
  } else if (dst_format == PP_VIDEOFRAME_FORMAT_YV12 ||
             dst_format == PP_VIDEOFRAME_FORMAT_I420) {
    static const size_t kPlanesOrder[][3] = {
        {VideoFrame::Plane::kY, VideoFrame::Plane::kV,
         VideoFrame::Plane::kU},  // YV12
        {VideoFrame::Plane::kY, VideoFrame::Plane::kU,
         VideoFrame::Plane::kV},  // I420
    };
    const int plane_order = (dst_format == PP_VIDEOFRAME_FORMAT_YV12) ? 0 : 1;
    int dst_width = dst_size.width();
    int dst_height = dst_size.height();
    libyuv::ScalePlane(src.visible_data(kPlanesOrder[plane_order][0]),
                       src.stride(kPlanesOrder[plane_order][0]),
                       src.visible_rect().width(), src.visible_rect().height(),
                       dst, dst_width, dst_width, dst_height, kFilterMode);
    dst += dst_width * dst_height;
    const int src_halfwidth = (src.visible_rect().width() + 1) >> 1;
    const int src_halfheight = (src.visible_rect().height() + 1) >> 1;
    const int dst_halfwidth = (dst_width + 1) >> 1;
    const int dst_halfheight = (dst_height + 1) >> 1;
    libyuv::ScalePlane(src.visible_data(kPlanesOrder[plane_order][1]),
                       src.stride(kPlanesOrder[plane_order][1]), src_halfwidth,
                       src_halfheight, dst, dst_halfwidth, dst_halfwidth,
                       dst_halfheight, kFilterMode);
    dst += dst_halfwidth * dst_halfheight;
    libyuv::ScalePlane(src.visible_data(kPlanesOrder[plane_order][2]),
                       src.stride(kPlanesOrder[plane_order][2]), src_halfwidth,
                       src_halfheight, dst, dst_halfwidth, dst_halfwidth,
                       dst_halfheight, kFilterMode);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace

namespace content {

// Internal class used for delivering video frames on the IO-thread to
// the blink::MediaStreamVideoSource implementation.
class PepperMediaStreamVideoTrackHost::FrameDeliverer
    : public base::RefCountedThreadSafe<FrameDeliverer> {
 public:
  FrameDeliverer(scoped_refptr<base::SequencedTaskRunner> video_task_runner,
                 const blink::VideoCaptureDeliverFrameCB& new_frame_callback);

  FrameDeliverer(const FrameDeliverer&) = delete;
  FrameDeliverer& operator=(const FrameDeliverer&) = delete;

  void DeliverVideoFrame(scoped_refptr<media::VideoFrame> frame);

 private:
  friend class base::RefCountedThreadSafe<FrameDeliverer>;
  virtual ~FrameDeliverer();

  void DeliverFrameOnIO(scoped_refptr<media::VideoFrame> frame);

  scoped_refptr<base::SequencedTaskRunner> video_task_runner_;
  blink::VideoCaptureDeliverFrameCB new_frame_callback_;
};

PepperMediaStreamVideoTrackHost::FrameDeliverer::FrameDeliverer(
    scoped_refptr<base::SequencedTaskRunner> video_task_runner,
    const blink::VideoCaptureDeliverFrameCB& new_frame_callback)
    : video_task_runner_(video_task_runner),
      new_frame_callback_(new_frame_callback) {}

PepperMediaStreamVideoTrackHost::FrameDeliverer::~FrameDeliverer() {
}

void PepperMediaStreamVideoTrackHost::FrameDeliverer::DeliverVideoFrame(
    scoped_refptr<media::VideoFrame> frame) {
  video_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FrameDeliverer::DeliverFrameOnIO, this,
                                std::move(frame)));
}

void PepperMediaStreamVideoTrackHost::FrameDeliverer::DeliverFrameOnIO(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(video_task_runner_->RunsTasksInCurrentSequence());
  // The time when this frame is generated is unknown so give a null value to
  // |estimated_capture_time|.
  new_frame_callback_.Run(std::move(frame), base::TimeTicks());
}

PepperMediaStreamVideoTrackHost::PepperMediaStreamVideoTrackHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const blink::WebMediaStreamTrack& track)
    : PepperMediaStreamTrackHostBase(host, instance, resource),
      track_(track),
      number_of_buffers_(kDefaultNumberOfVideoBuffers),
      source_frame_format_(PP_VIDEOFRAME_FORMAT_UNKNOWN),
      plugin_frame_format_(PP_VIDEOFRAME_FORMAT_UNKNOWN),
      frame_data_size_(0),
      type_(kRead) {
  DCHECK(!track_.IsNull());
}

PepperMediaStreamVideoTrackHost::PepperMediaStreamVideoTrackHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : PepperMediaStreamTrackHostBase(host, instance, resource),
      number_of_buffers_(kDefaultNumberOfVideoBuffers),
      source_frame_format_(PP_VIDEOFRAME_FORMAT_UNKNOWN),
      plugin_frame_format_(PP_VIDEOFRAME_FORMAT_UNKNOWN),
      frame_data_size_(0),
      type_(kWrite) {
  InitBlinkTrack();
  DCHECK(!track_.IsNull());
}

PepperMediaStreamVideoTrackHost::~PepperMediaStreamVideoTrackHost() {
  OnClose();
}

bool PepperMediaStreamVideoTrackHost::IsMediaStreamVideoTrackHost() {
  return true;
}

void PepperMediaStreamVideoTrackHost::InitBuffers() {
  gfx::Size size = GetTargetSize(source_frame_size_, plugin_frame_size_);
  DCHECK(!size.IsEmpty());

  PP_VideoFrame_Format format =
      GetTargetFormat(source_frame_format_, plugin_frame_format_);
  DCHECK_NE(format, PP_VIDEOFRAME_FORMAT_UNKNOWN);

  if (format == PP_VIDEOFRAME_FORMAT_BGRA) {
    frame_data_size_ = size.width() * size.height() * 4;
  } else {
    frame_data_size_ =
        VideoFrame::AllocationSize(FromPpapiFormat(format), size);
  }

  DCHECK_GT(frame_data_size_, 0U);
  int32_t buffer_size =
      sizeof(ppapi::MediaStreamBuffer::Video) + frame_data_size_;
  bool result = PepperMediaStreamTrackHostBase::InitBuffers(number_of_buffers_,
                                                            buffer_size,
                                                            type_);
  CHECK(result);

  if (type_ == kWrite) {
    for (int32_t i = 0; i < buffer_manager()->number_of_buffers(); ++i) {
      ppapi::MediaStreamBuffer::Video* buffer =
          &(buffer_manager()->GetBufferPointer(i)->video);
      buffer->header.size = buffer_manager()->buffer_size();
      buffer->header.type = ppapi::MediaStreamBuffer::TYPE_VIDEO;
      buffer->format = format;
      buffer->size.width = size.width();
      buffer->size.height = size.height();
      buffer->data_size = frame_data_size_;
    }

    // Make all the frames avaiable to the plugin.
    std::vector<int32_t> indices = buffer_manager()->DequeueBuffers();
    SendEnqueueBuffersMessageToPlugin(indices);
  }
}

void PepperMediaStreamVideoTrackHost::OnClose() {
  blink::MediaStreamVideoSink::DisconnectFromTrack();
  weak_factory_.InvalidateWeakPtrs();
}

int32_t PepperMediaStreamVideoTrackHost::OnHostMsgEnqueueBuffer(
    ppapi::host::HostMessageContext* context, int32_t index) {
  if (type_ == kRead) {
    return PepperMediaStreamTrackHostBase::OnHostMsgEnqueueBuffer(context,
                                                                  index);
  } else {
    return SendFrameToTrack(index);
  }
}

int32_t PepperMediaStreamVideoTrackHost::SendFrameToTrack(int32_t index) {
  DCHECK_EQ(type_, kWrite);

  if (frame_deliverer_) {
    // Sends the frame to blink video track.
    ppapi::MediaStreamBuffer::Video* pp_frame =
        &(buffer_manager()->GetBufferPointer(index)->video);

    int32_t y_stride = plugin_frame_size_.width();
    int32_t uv_stride = (plugin_frame_size_.width() + 1) / 2;
    uint8_t* y_data = static_cast<uint8_t*>(pp_frame->data);
    // Default to I420
    uint8_t* u_data = y_data + plugin_frame_size_.GetArea();
    uint8_t* v_data = y_data + (plugin_frame_size_.GetArea() * 5 / 4);
    if (plugin_frame_format_ == PP_VIDEOFRAME_FORMAT_YV12) {
      // Swap u and v for YV12.
      uint8_t* tmp = u_data;
      u_data = v_data;
      v_data = tmp;
    }

    int64_t ts_ms = static_cast<int64_t>(pp_frame->timestamp *
                                         base::Time::kMillisecondsPerSecond);
    scoped_refptr<VideoFrame> frame = media::VideoFrame::WrapExternalYuvData(
        FromPpapiFormat(plugin_frame_format_), plugin_frame_size_,
        gfx::Rect(plugin_frame_size_), plugin_frame_size_, y_stride, uv_stride,
        uv_stride, y_data, u_data, v_data, base::Milliseconds(ts_ms));
    if (!frame)
      return PP_ERROR_FAILED;

    frame_deliverer_->DeliverVideoFrame(frame);
  }

  // Makes the frame available again for plugin.
  SendEnqueueBufferMessageToPlugin(index);
  return PP_OK;
}

void PepperMediaStreamVideoTrackHost::OnVideoFrame(
    scoped_refptr<VideoFrame> video_frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK(video_frame);
  // TODO(penghuang): Check |frame->end_of_stream()| and close the track.
  // Scaled video frames are currently ignored.
  scoped_refptr<media::VideoFrame> frame = video_frame;
  // Drop alpha channel since we do not support it yet.
  if (frame->format() == media::PIXEL_FORMAT_I420A)
    frame = media::WrapAsI420VideoFrame(video_frame);
  PP_VideoFrame_Format ppformat = ToPpapiFormat(frame->format());
  if (frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    // NV12 is the only supported GMB pixel format at the moment, and there is
    // no corresponding PP_VideoFrame_Format. Convert the video frame to I420.
    DCHECK_EQ(frame->format(), media::PIXEL_FORMAT_NV12);
    ppformat = PP_VIDEOFRAME_FORMAT_I420;
    auto scoped_mapping = video_frame->MapGMBOrSharedImage();
    if (!scoped_mapping) {
      DLOG(WARNING) << "Failed to get a scoped_mapping object.";
      return;
    }
    frame = media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_I420, video_frame->coded_size(),
        video_frame->visible_rect(), video_frame->natural_size(),
        video_frame->timestamp());
    int ret = libyuv::NV12ToI420(scoped_mapping->Memory(0),
        scoped_mapping->Stride(0),
        scoped_mapping->Memory(1),
        scoped_mapping->Stride(1),
        frame->writable_data(media::VideoFrame::Plane::kY),
        frame->stride(media::VideoFrame::Plane::kY),
        frame->writable_data(media::VideoFrame::Plane::kU),
        frame->stride(media::VideoFrame::Plane::kU),
        frame->writable_data(media::VideoFrame::Plane::kV),
        frame->stride(media::VideoFrame::Plane::kV),
        video_frame->coded_size().width(), video_frame->coded_size().height());
    if (ret != 0) {
      DLOG(WARNING) << "Failed to convert NV12 to I420";
      return;
    }
  }
  if (ppformat == PP_VIDEOFRAME_FORMAT_UNKNOWN)
    return;

  if (source_frame_size_.IsEmpty()) {
    source_frame_size_ = frame->visible_rect().size();
    source_frame_format_ = ppformat;
    InitBuffers();
  }

  int32_t index = buffer_manager()->DequeueBuffer();
  // Drop frames if the underlying buffer is full.
  if (index < 0) {
    DVLOG(1) << "A frame is dropped.";
    return;
  }

  CHECK_EQ(ppformat, source_frame_format_) << "Frame format is changed.";

  gfx::Size size = GetTargetSize(source_frame_size_, plugin_frame_size_);
  ppformat =
      GetTargetFormat(source_frame_format_, plugin_frame_format_);
  ppapi::MediaStreamBuffer::Video* buffer =
      &(buffer_manager()->GetBufferPointer(index)->video);
  buffer->header.size = buffer_manager()->buffer_size();
  buffer->header.type = ppapi::MediaStreamBuffer::TYPE_VIDEO;
  buffer->timestamp = frame->timestamp().InSecondsF();
  buffer->format = ppformat;
  buffer->size.width = size.width();
  buffer->size.height = size.height();
  buffer->data_size = frame_data_size_;
  ConvertFromMediaVideoFrame(*frame, ppformat, size, buffer->data);

  SendEnqueueBufferMessageToPlugin(index);
}

class PepperMediaStreamVideoTrackHost::VideoSource final
    : public blink::MediaStreamVideoSource {
 public:
  explicit VideoSource(base::WeakPtr<PepperMediaStreamVideoTrackHost> host)
      : blink::MediaStreamVideoSource(
            base::SingleThreadTaskRunner::GetCurrentDefault()),
        host_(std::move(host)) {}

  VideoSource(const VideoSource&) = delete;
  VideoSource& operator=(const VideoSource&) = delete;

  ~VideoSource() final { StopSourceImpl(); }

  void StartSourceImpl(
      blink::VideoCaptureDeliverFrameCB frame_callback,
      blink::EncodedVideoFrameCB encoded_frame_callback,
      blink::VideoCaptureSubCaptureTargetVersionCB
          sub_capture_target_version_callback,
      blink::VideoCaptureNotifyFrameDroppedCB frame_dropped_callback) final {
    if (host_) {
      host_->frame_deliverer_ =
          new FrameDeliverer(video_task_runner(), std::move(frame_callback));
    }
  }

  void StopSourceImpl() final {
    if (host_)
      host_->frame_deliverer_ = nullptr;
  }

  base::WeakPtr<MediaStreamVideoSource> GetWeakPtr() final {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::optional<media::VideoCaptureFormat> GetCurrentFormat() const override {
    if (host_) {
      return std::optional<media::VideoCaptureFormat>(media::VideoCaptureFormat(
          host_->plugin_frame_size_, kDefaultOutputFrameRate,
          ToPixelFormat(host_->plugin_frame_format_)));
    }
    return std::optional<media::VideoCaptureFormat>();
  }

  const base::WeakPtr<PepperMediaStreamVideoTrackHost> host_;
  base::WeakPtrFactory<MediaStreamVideoSource> weak_factory_{this};
};

void PepperMediaStreamVideoTrackHost::DidConnectPendingHostToResource() {
  if (!blink::MediaStreamVideoSink::connected_track().IsNull())
    return;
  blink::MediaStreamVideoSink::ConnectToTrack(
      track_,
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&PepperMediaStreamVideoTrackHost::OnVideoFrame,
                              weak_factory_.GetWeakPtr())),
      MediaStreamVideoSink::IsSecure::kNo,
      MediaStreamVideoSink::UsesAlpha::kDefault);
}

int32_t PepperMediaStreamVideoTrackHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperMediaStreamVideoTrackHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_MediaStreamVideoTrack_Configure, OnHostMsgConfigure)
  PPAPI_END_MESSAGE_MAP()
  return PepperMediaStreamTrackHostBase::OnResourceMessageReceived(msg,
                                                                   context);
}

int32_t PepperMediaStreamVideoTrackHost::OnHostMsgConfigure(
    HostMessageContext* context,
    const MediaStreamVideoTrackShared::Attributes& attributes) {
  CHECK(MediaStreamVideoTrackShared::VerifyAttributes(attributes));

  bool changed = false;
  gfx::Size new_size(attributes.width, attributes.height);
  if (GetTargetSize(source_frame_size_, plugin_frame_size_) !=
      GetTargetSize(source_frame_size_, new_size)) {
    changed = true;
  }
  plugin_frame_size_ = new_size;

  int32_t buffers = attributes.buffers
                        ? std::min(kMaxNumberOfVideoBuffers, attributes.buffers)
                        : kDefaultNumberOfVideoBuffers;
  if (buffers != number_of_buffers_)
    changed = true;
  number_of_buffers_ = buffers;

  if (GetTargetFormat(source_frame_format_, plugin_frame_format_) !=
      GetTargetFormat(source_frame_format_, attributes.format)) {
    changed = true;
  }
  plugin_frame_format_ = attributes.format;

  // If the first frame has been received, we will re-initialize buffers with
  // new settings. Otherwise, we will initialize buffer when we receive
  // the first frame, because plugin can only provide part of attributes
  // which are not enough to initialize buffers.
  if (changed && (type_ == kWrite || !source_frame_size_.IsEmpty()))
    InitBuffers();

  // TODO(ronghuawu): Ask the owner of DOMMediaStreamTrackToResource why
  // source id instead of track id is used there.
  const std::string id = track_.Source().Id().Utf8();
  context->reply_msg = PpapiPluginMsg_MediaStreamVideoTrack_ConfigureReply(id);
  return PP_OK;
}

void PepperMediaStreamVideoTrackHost::InitBlinkTrack() {
  std::string source_id = base::Base64Encode(base::RandBytesAsVector(64));
  blink::WebMediaStreamSource webkit_source;
  auto source = std::make_unique<VideoSource>(weak_factory_.GetWeakPtr());
  blink::MediaStreamVideoSource* const source_ptr = source.get();
  webkit_source.Initialize(blink::WebString::FromASCII(source_id),
                           blink::WebMediaStreamSource::kTypeVideo,
                           blink::WebString::FromASCII(kPepperVideoSourceName),
                           false /* remote */,
                           std::move(source));  // Takes ownership of |source|.

  const bool enabled = true;
  track_ = blink::CreateWebMediaStreamVideoTrack(
      source_ptr,
      base::BindOnce(&PepperMediaStreamVideoTrackHost::OnTrackStarted,
                     base::Unretained(this)),
      enabled);
  // Note: The call to CreateVideoTrack() returned a track that holds a
  // ref-counted reference to |webkit_source| (and, implicitly, |source|).
}

void PepperMediaStreamVideoTrackHost::OnTrackStarted(
    blink::WebPlatformMediaStreamSource* source,
    blink::mojom::MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DVLOG(3) << "OnTrackStarted result: " << result;
}

}  // namespace content
