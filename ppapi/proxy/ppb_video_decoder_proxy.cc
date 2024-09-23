// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/proxy/ppb_video_decoder_proxy.h"

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/ppb_graphics_3d_proxy.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::PPB_VideoDecoder_Dev_API;

namespace ppapi {
namespace proxy {

class VideoDecoder : public PPB_VideoDecoder_Shared {
 public:
  // You must call Init() before using this class.
  explicit VideoDecoder(const HostResource& resource);

  VideoDecoder(const VideoDecoder&) = delete;
  VideoDecoder& operator=(const VideoDecoder&) = delete;

  ~VideoDecoder() override;

  static VideoDecoder* Create(const HostResource& resource,
                              PP_Resource graphics_context,
                              PP_VideoDecoder_Profile profile);

  // PPB_VideoDecoder_Dev_API implementation.
  int32_t Decode(const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
                 scoped_refptr<TrackedCallback> callback) override;
  void AssignPictureBuffers(uint32_t no_of_buffers,
                            const PP_PictureBuffer_Dev* buffers) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  int32_t Flush(scoped_refptr<TrackedCallback> callback) override;
  int32_t Reset(scoped_refptr<TrackedCallback> callback) override;
  void Destroy() override;

 private:
  friend class PPB_VideoDecoder_Proxy;

  PluginDispatcher* GetDispatcher() const;

  // Run the callbacks that were passed into the plugin interface.
  void FlushACK(int32_t result);
  void ResetACK(int32_t result);
  void EndOfBitstreamACK(int32_t buffer_id, int32_t result);
};

VideoDecoder::VideoDecoder(const HostResource& decoder)
    : PPB_VideoDecoder_Shared(decoder) {
}

VideoDecoder::~VideoDecoder() {
  FlushCommandBuffer();
  PPB_VideoDecoder_Shared::Destroy();
}

int32_t VideoDecoder::Decode(
    const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
    scoped_refptr<TrackedCallback> callback) {
  EnterResourceNoLock<PPB_Buffer_API>
      enter_buffer(bitstream_buffer->data, true);
  if (enter_buffer.failed())
    return PP_ERROR_BADRESOURCE;

  if (!SetBitstreamBufferCallback(bitstream_buffer->id, callback))
    return PP_ERROR_BADARGUMENT;

  Buffer* ppb_buffer =
      static_cast<Buffer*>(enter_buffer.object());
  HostResource host_buffer = ppb_buffer->host_resource();

  FlushCommandBuffer();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoDecoder_Decode(
      API_ID_PPB_VIDEO_DECODER_DEV, host_resource(),
      host_buffer, bitstream_buffer->id,
      bitstream_buffer->size));
  return PP_OK_COMPLETIONPENDING;
}

void VideoDecoder::AssignPictureBuffers(uint32_t no_of_buffers,
                                        const PP_PictureBuffer_Dev* buffers) {
  std::vector<PP_PictureBuffer_Dev> buffer_list(
      buffers, buffers + no_of_buffers);
  FlushCommandBuffer();
  GetDispatcher()->Send(
      new PpapiHostMsg_PPBVideoDecoder_AssignPictureBuffers(
          API_ID_PPB_VIDEO_DECODER_DEV, host_resource(), buffer_list));
}

void VideoDecoder::ReusePictureBuffer(int32_t picture_buffer_id) {
  FlushCommandBuffer();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoDecoder_ReusePictureBuffer(
      API_ID_PPB_VIDEO_DECODER_DEV, host_resource(), picture_buffer_id));
}

int32_t VideoDecoder::Flush(scoped_refptr<TrackedCallback> callback) {
  if (!SetFlushCallback(callback))
    return PP_ERROR_INPROGRESS;

  FlushCommandBuffer();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoDecoder_Flush(
      API_ID_PPB_VIDEO_DECODER_DEV, host_resource()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoDecoder::Reset(scoped_refptr<TrackedCallback> callback) {
  if (!SetResetCallback(callback))
    return PP_ERROR_INPROGRESS;

  FlushCommandBuffer();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoDecoder_Reset(
      API_ID_PPB_VIDEO_DECODER_DEV, host_resource()));
  return PP_OK_COMPLETIONPENDING;
}

void VideoDecoder::Destroy() {
  FlushCommandBuffer();
  GetDispatcher()->Send(new PpapiHostMsg_PPBVideoDecoder_Destroy(
      API_ID_PPB_VIDEO_DECODER_DEV, host_resource()));
  PPB_VideoDecoder_Shared::Destroy();
}

PluginDispatcher* VideoDecoder::GetDispatcher() const {
  return PluginDispatcher::GetForResource(this);
}

void VideoDecoder::ResetACK(int32_t result) {
  RunResetCallback(result);
}

void VideoDecoder::FlushACK(int32_t result) {
  RunFlushCallback(result);
}

void VideoDecoder::EndOfBitstreamACK(
    int32_t bitstream_buffer_id, int32_t result) {
  RunBitstreamBufferCallback(bitstream_buffer_id, result);
}

PPB_VideoDecoder_Proxy::PPB_VideoDecoder_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(this) {
}

PPB_VideoDecoder_Proxy::~PPB_VideoDecoder_Proxy() {
}

bool PPB_VideoDecoder_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->permissions().HasPermission(PERMISSION_DEV))
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_VideoDecoder_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_Decode, OnMsgDecode)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_AssignPictureBuffers,
                        OnMsgAssignPictureBuffers)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_ReusePictureBuffer,
                        OnMsgReusePictureBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_Flush, OnMsgFlush)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_Reset, OnMsgReset)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVideoDecoder_Destroy, OnMsgDestroy)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBVideoDecoder_ResetACK, OnMsgResetACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBVideoDecoder_EndOfBitstreamACK,
                        OnMsgEndOfBitstreamACK)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBVideoDecoder_FlushACK, OnMsgFlushACK)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

PP_Resource PPB_VideoDecoder_Proxy::CreateProxyResource(
    PP_Instance instance,
    PP_Resource graphics_context,
    PP_VideoDecoder_Profile profile) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  // Dispatcher is null if it cannot find the instance passed to it (i.e. if the
  // client passes in an invalid instance).
  if (!dispatcher)
    return 0;

  if (!dispatcher->preferences().is_accelerated_video_decode_enabled)
    return 0;

  EnterResourceNoLock<PPB_Graphics3D_API> enter_context(graphics_context,
                                                        true);
  if (enter_context.failed())
    return 0;

  Graphics3D* context = static_cast<Graphics3D*>(enter_context.object());

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBVideoDecoder_Create(
      API_ID_PPB_VIDEO_DECODER_DEV, instance,
      context->host_resource(), profile, &result));
  if (result.is_null())
    return 0;

  // Need a scoped_refptr to keep the object alive during the Init call.
  scoped_refptr<VideoDecoder> decoder(new VideoDecoder(result));
  decoder->InitCommon(graphics_context, context->gles2_impl());
  return decoder->GetReference();
}

void PPB_VideoDecoder_Proxy::OnMsgCreate(
    PP_Instance instance, const HostResource& graphics_context,
    PP_VideoDecoder_Profile profile,
    HostResource* result) {
  thunk::EnterResourceCreation resource_creation(instance);
  if (resource_creation.failed())
    return;

  // Make the resource and get the API pointer to its interface.
  result->SetHostResource(
      instance, resource_creation.functions()->CreateVideoDecoderDev(
          instance, graphics_context.host_resource(), profile));
}

void PPB_VideoDecoder_Proxy::OnMsgDecode(const HostResource& decoder,
                                         const HostResource& buffer,
                                         int32_t id,
                                         uint32_t size) {
  EnterHostFromHostResourceForceCallback<PPB_VideoDecoder_Dev_API> enter(
      decoder, callback_factory_,
      &PPB_VideoDecoder_Proxy::SendMsgEndOfBitstreamACKToPlugin, decoder, id);
  if (enter.failed())
    return;
  PP_VideoBitstreamBuffer_Dev bitstream = { id, buffer.host_resource(), size };
  enter.SetResult(enter.object()->Decode(&bitstream, enter.callback()));
}

void PPB_VideoDecoder_Proxy::OnMsgAssignPictureBuffers(
    const HostResource& decoder,
    const std::vector<PP_PictureBuffer_Dev>& buffers) {
  EnterHostFromHostResource<PPB_VideoDecoder_Dev_API> enter(decoder);
  if (enter.succeeded() && !buffers.empty()) {
    const PP_PictureBuffer_Dev* buffer_array = &buffers.front();
    enter.object()->AssignPictureBuffers(
        base::checked_cast<uint32_t>(buffers.size()), buffer_array);
  }
}

void PPB_VideoDecoder_Proxy::OnMsgReusePictureBuffer(
    const HostResource& decoder,
    int32_t picture_buffer_id) {
  EnterHostFromHostResource<PPB_VideoDecoder_Dev_API> enter(decoder);
  if (enter.succeeded())
    enter.object()->ReusePictureBuffer(picture_buffer_id);
}

void PPB_VideoDecoder_Proxy::OnMsgFlush(const HostResource& decoder) {
  EnterHostFromHostResourceForceCallback<PPB_VideoDecoder_Dev_API> enter(
      decoder, callback_factory_,
      &PPB_VideoDecoder_Proxy::SendMsgFlushACKToPlugin, decoder);
  if (enter.succeeded())
    enter.SetResult(enter.object()->Flush(enter.callback()));
}

void PPB_VideoDecoder_Proxy::OnMsgReset(const HostResource& decoder) {
  EnterHostFromHostResourceForceCallback<PPB_VideoDecoder_Dev_API> enter(
      decoder, callback_factory_,
      &PPB_VideoDecoder_Proxy::SendMsgResetACKToPlugin, decoder);
  if (enter.succeeded())
    enter.SetResult(enter.object()->Reset(enter.callback()));
}

void PPB_VideoDecoder_Proxy::OnMsgDestroy(const HostResource& decoder) {
  EnterHostFromHostResource<PPB_VideoDecoder_Dev_API> enter(decoder);
  if (enter.succeeded())
    enter.object()->Destroy();
}

void PPB_VideoDecoder_Proxy::SendMsgEndOfBitstreamACKToPlugin(
    int32_t result,
    const HostResource& decoder,
    int32_t id) {
  dispatcher()->Send(new PpapiMsg_PPBVideoDecoder_EndOfBitstreamACK(
      API_ID_PPB_VIDEO_DECODER_DEV, decoder, id, result));
}

void PPB_VideoDecoder_Proxy::SendMsgFlushACKToPlugin(
    int32_t result, const HostResource& decoder) {
  dispatcher()->Send(new PpapiMsg_PPBVideoDecoder_FlushACK(
      API_ID_PPB_VIDEO_DECODER_DEV, decoder, result));
}

void PPB_VideoDecoder_Proxy::SendMsgResetACKToPlugin(
    int32_t result, const HostResource& decoder) {
  dispatcher()->Send(new PpapiMsg_PPBVideoDecoder_ResetACK(
      API_ID_PPB_VIDEO_DECODER_DEV, decoder, result));
}

void PPB_VideoDecoder_Proxy::OnMsgEndOfBitstreamACK(
    const HostResource& decoder, int32_t id, int32_t result) {
  EnterPluginFromHostResource<PPB_VideoDecoder_Dev_API> enter(decoder);
  if (enter.succeeded())
    static_cast<VideoDecoder*>(enter.object())->EndOfBitstreamACK(id, result);
}

void PPB_VideoDecoder_Proxy::OnMsgFlushACK(
    const HostResource& decoder, int32_t result) {
  EnterPluginFromHostResource<PPB_VideoDecoder_Dev_API> enter(decoder);
  if (enter.succeeded())
    static_cast<VideoDecoder*>(enter.object())->FlushACK(result);
}

void PPB_VideoDecoder_Proxy::OnMsgResetACK(
    const HostResource& decoder, int32_t result) {
  EnterPluginFromHostResource<PPB_VideoDecoder_Dev_API> enter(decoder);
  if (enter.succeeded())
    static_cast<VideoDecoder*>(enter.object())->ResetACK(result);
}

}  // namespace proxy
}  // namespace ppapi
