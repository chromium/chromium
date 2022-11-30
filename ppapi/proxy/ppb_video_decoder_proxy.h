// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_VIDEO_DECODER_PROXY_H_
#define PPAPI_PROXY_PPB_VIDEO_DECODER_PROXY_H_

#include <stdint.h>

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_completion_callback_factory.h"
#include "ppapi/shared_impl/ppb_video_decoder_shared.h"
#include "ppapi/thunk/ppb_video_decoder_dev_api.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace ppapi {
namespace proxy {

class PPB_VideoDecoder_Proxy : public InterfaceProxy {
 public:
  explicit PPB_VideoDecoder_Proxy(Dispatcher* dispatcher);

  PPB_VideoDecoder_Proxy(const PPB_VideoDecoder_Proxy&) = delete;
  PPB_VideoDecoder_Proxy& operator=(const PPB_VideoDecoder_Proxy&) = delete;

  ~PPB_VideoDecoder_Proxy() override;

  // Creates a VideoDecoder object in the plugin process.
  static PP_Resource CreateProxyResource(
      PP_Instance instance,
      PP_Resource graphics_context,
      PP_VideoDecoder_Profile profile);

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  static const ApiID kApiID = API_ID_PPB_VIDEO_DECODER_DEV;

 private:
  // Message handlers in the renderer process to receive messages from the
  // plugin process.
  void OnMsgCreate(PP_Instance instance,
                   const ppapi::HostResource& graphics_context,
                   PP_VideoDecoder_Profile profile,
                   ppapi::HostResource* result);
  void OnMsgDecode(const ppapi::HostResource& decoder,
                   const ppapi::HostResource& buffer,
                   int32_t id,
                   uint32_t size);
  void OnMsgAssignPictureBuffers(
      const ppapi::HostResource& decoder,
      const std::vector<PP_PictureBuffer_Dev>& buffers);
  void OnMsgReusePictureBuffer(const ppapi::HostResource& decoder,
                               int32_t picture_buffer_id);
  void OnMsgFlush(const ppapi::HostResource& decoder);
  void OnMsgReset(const ppapi::HostResource& decoder);
  void OnMsgDestroy(const ppapi::HostResource& decoder);

  // Send a message from the renderer process to the plugin process to tell it
  // to run its callback.
  void SendMsgEndOfBitstreamACKToPlugin(int32_t result,
                                        const ppapi::HostResource& decoder,
                                        int32_t id);
  void SendMsgFlushACKToPlugin(
      int32_t result, const ppapi::HostResource& decoder);
  void SendMsgResetACKToPlugin(
      int32_t result, const ppapi::HostResource& decoder);

  // Message handlers in the plugin process to receive messages from the
  // renderer process.
  void OnMsgEndOfBitstreamACK(const ppapi::HostResource& decoder,
                              int32_t id, int32_t result);
  void OnMsgFlushACK(const ppapi::HostResource& decoder, int32_t result);
  void OnMsgResetACK(const ppapi::HostResource& decoder, int32_t result);

  ProxyCompletionCallbackFactory<PPB_VideoDecoder_Proxy> callback_factory_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_VIDEO_DECODER_PROXY_H_
