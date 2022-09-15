// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_VIDEO_DECODER_PROXY_H_
#define PPAPI_PROXY_PPP_VIDEO_DECODER_PROXY_H_

#include <stdint.h>

#include "ppapi/c/dev/ppp_video_decoder_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

struct PP_Picture_Dev;
struct PP_Size;

namespace ppapi {
namespace proxy {

class PPP_VideoDecoder_Proxy : public InterfaceProxy {
 public:
  explicit PPP_VideoDecoder_Proxy(Dispatcher* dispatcher);

  PPP_VideoDecoder_Proxy(const PPP_VideoDecoder_Proxy&) = delete;
  PPP_VideoDecoder_Proxy& operator=(const PPP_VideoDecoder_Proxy&) = delete;

  ~PPP_VideoDecoder_Proxy() override;

  static const PPP_VideoDecoder_Dev* GetProxyInterface();

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnMsgProvidePictureBuffers(const ppapi::HostResource& decoder,
                                  uint32_t req_num_of_buffers,
                                  const PP_Size& dimensions,
                                  uint32_t texture_target);
  void OnMsgDismissPictureBuffer(const ppapi::HostResource& decoder,
                                 int32_t picture_id);
  void OnMsgPictureReady(const ppapi::HostResource& decoder,
                         const PP_Picture_Dev& picture_buffer);
  void OnMsgNotifyError(const ppapi::HostResource& decoder,
                        PP_VideoDecodeError_Dev error);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_VideoDecoder_Dev* ppp_video_decoder_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_VIDEO_DECODER_PROXY_H_
