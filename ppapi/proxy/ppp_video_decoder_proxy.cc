// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_video_decoder_proxy.h"

#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_video_decoder_proxy.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_video_decoder_dev_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

void ProvidePictureBuffers(PP_Instance instance, PP_Resource decoder,
                           uint32_t req_num_of_bufs,
                           const PP_Size* dimensions,
                           uint32_t texture_target) {
  HostResource decoder_resource;
  decoder_resource.SetHostResource(instance, decoder);

  // This is called by the graphics system in response to a message from the
  // GPU process. These messages will not be synchronized with the lifetime
  // of the plugin so we need to null-check here.
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (dispatcher) {
    dispatcher->Send(new PpapiMsg_PPPVideoDecoder_ProvidePictureBuffers(
        API_ID_PPP_VIDEO_DECODER_DEV,
        decoder_resource, req_num_of_bufs, *dimensions, texture_target));
  }
}

void DismissPictureBuffer(PP_Instance instance, PP_Resource decoder,
                          int32_t picture_buffer_id) {
  HostResource decoder_resource;
  decoder_resource.SetHostResource(instance, decoder);

  // Null check as in ProvidePictureBuffers above.
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (dispatcher) {
    dispatcher->Send(new PpapiMsg_PPPVideoDecoder_DismissPictureBuffer(
        API_ID_PPP_VIDEO_DECODER_DEV,
        decoder_resource, picture_buffer_id));
  }
}

void PictureReady(PP_Instance instance, PP_Resource decoder,
                  const PP_Picture_Dev* picture) {
  HostResource decoder_resource;
  decoder_resource.SetHostResource(instance, decoder);

  // Null check as in ProvidePictureBuffers above.
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (dispatcher) {
    dispatcher->Send(new PpapiMsg_PPPVideoDecoder_PictureReady(
        API_ID_PPP_VIDEO_DECODER_DEV, decoder_resource, *picture));
  }
}

void NotifyError(PP_Instance instance, PP_Resource decoder,
                 PP_VideoDecodeError_Dev error) {
  HostResource decoder_resource;
  decoder_resource.SetHostResource(instance, decoder);

  // It's possible that the error we're being notified about is happening
  // because the instance is shutting down. In this case, our instance may
  // already have been removed from the HostDispatcher map.
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (dispatcher) {
    dispatcher->Send(
      new PpapiMsg_PPPVideoDecoder_NotifyError(
          API_ID_PPP_VIDEO_DECODER_DEV, decoder_resource, error));
  }
}

static const PPP_VideoDecoder_Dev video_decoder_interface = {
  &ProvidePictureBuffers,
  &DismissPictureBuffer,
  &PictureReady,
  &NotifyError
};

}  // namespace

PPP_VideoDecoder_Proxy::PPP_VideoDecoder_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_video_decoder_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_video_decoder_impl_ = static_cast<const PPP_VideoDecoder_Dev*>(
        dispatcher->local_get_interface()(PPP_VIDEODECODER_DEV_INTERFACE));
  }
}

PPP_VideoDecoder_Proxy::~PPP_VideoDecoder_Proxy() {
}

// static
const PPP_VideoDecoder_Dev* PPP_VideoDecoder_Proxy::GetProxyInterface() {
  return &video_decoder_interface;
}

bool PPP_VideoDecoder_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_VideoDecoder_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoDecoder_ProvidePictureBuffers,
                        OnMsgProvidePictureBuffers)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoDecoder_DismissPictureBuffer,
                        OnMsgDismissPictureBuffer)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoDecoder_PictureReady,
                        OnMsgPictureReady)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPVideoDecoder_NotifyError,
                        OnMsgNotifyError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void PPP_VideoDecoder_Proxy::OnMsgProvidePictureBuffers(
    const HostResource& decoder,
    uint32_t req_num_of_bufs,
    const PP_Size& dimensions,
    uint32_t texture_target) {
  PP_Resource plugin_decoder = PluginGlobals::Get()->plugin_resource_tracker()->
      PluginResourceForHostResource(decoder);
  if (!plugin_decoder)
    return;
  CallWhileUnlocked(ppp_video_decoder_impl_->ProvidePictureBuffers,
                    decoder.instance(),
                    plugin_decoder,
                    req_num_of_bufs,
                    &dimensions,
                    texture_target);
}

void PPP_VideoDecoder_Proxy::OnMsgDismissPictureBuffer(
    const HostResource& decoder, int32_t picture_id) {
  PP_Resource plugin_decoder = PluginGlobals::Get()->plugin_resource_tracker()->
      PluginResourceForHostResource(decoder);
  if (!plugin_decoder)
    return;
  CallWhileUnlocked(ppp_video_decoder_impl_->DismissPictureBuffer,
                    decoder.instance(),
                    plugin_decoder,
                    picture_id);
}

void PPP_VideoDecoder_Proxy::OnMsgPictureReady(
    const HostResource& decoder, const PP_Picture_Dev& picture) {
  PP_Resource plugin_decoder = PluginGlobals::Get()->plugin_resource_tracker()->
      PluginResourceForHostResource(decoder);
  if (!plugin_decoder)
    return;
  CallWhileUnlocked(ppp_video_decoder_impl_->PictureReady,
                    decoder.instance(),
                    plugin_decoder,
                    &picture);
}

void PPP_VideoDecoder_Proxy::OnMsgNotifyError(
    const HostResource& decoder, PP_VideoDecodeError_Dev error) {
  PP_Resource plugin_decoder = PluginGlobals::Get()->plugin_resource_tracker()->
      PluginResourceForHostResource(decoder);
  if (!plugin_decoder)
    return;
  CallWhileUnlocked(ppp_video_decoder_impl_->NotifyError,
                    decoder.instance(),
                    plugin_decoder,
                    error);
}

}  // namespace proxy
}  // namespace ppapi
