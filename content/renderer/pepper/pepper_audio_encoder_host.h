// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_ENCODER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_ENCODER_HOST_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_math.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/media_stream_buffer_manager.h"

namespace content {

class RendererPpapiHost;

class CONTENT_EXPORT PepperAudioEncoderHost
    : public ppapi::host::ResourceHost,
      public ppapi::MediaStreamBufferManager::Delegate {
 public:
  PepperAudioEncoderHost(RendererPpapiHost* host,
                         PP_Instance instance,
                         PP_Resource resource);
  ~PepperAudioEncoderHost() override;

 private:
  class AudioEncoderImpl;

  // ResourceHost implementation.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  int32_t OnHostMsgGetSupportedProfiles(
      ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgInitialize(
      ppapi::host::HostMessageContext* context,
      const ppapi::proxy::PPB_AudioEncodeParameters& parameters);
  int32_t OnHostMsgGetAudioBuffers(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgEncode(ppapi::host::HostMessageContext* context,
                          int32_t buffer_id);
  int32_t OnHostMsgRecycleBitstreamBuffer(
      ppapi::host::HostMessageContext* context,
      int32_t buffer_id);
  int32_t OnHostMsgRequestBitrateChange(
      ppapi::host::HostMessageContext* context,
      uint32_t bitrate);
  int32_t OnHostMsgClose(ppapi::host::HostMessageContext* context);

  void GetSupportedProfiles(std::vector<PP_AudioProfileDescription>* profiles);
  bool IsInitializationValid(
      const ppapi::proxy::PPB_AudioEncodeParameters& parameters);
  bool AllocateBuffers(
      const ppapi::proxy::PPB_AudioEncodeParameters& parameters,
      int32_t samples_per_frame);
  void DoEncode();
  void BitstreamBufferReady(int32_t audio_buffer_id,
                            int32_t bitstream_buffer_id,
                            int32_t size);
  void NotifyPepperError(int32_t error);
  void Close();

  static void StopAudioEncoder(
      std::unique_ptr<AudioEncoderImpl> encoder,
      std::unique_ptr<ppapi::MediaStreamBufferManager> audio_buffer_manager,
      std::unique_ptr<ppapi::MediaStreamBufferManager>
          bitstream_buffer_manager);

  // Non-owning pointer.
  RendererPpapiHost* renderer_ppapi_host_;

  // Whether the encoder has been successfully initialized.
  bool initialized_;

  // Last error reported by the encoder.
  // This represents the current error state of the encoder, i.e. PP_OK
  // normally, or a Pepper error code if the encoder is uninitialized,
  // has been notified of an encoder error, has encountered some
  // other unrecoverable error, or has been closed by the plugin.
  // This field is checked in most message handlers to decide whether
  // operations should proceed or fail.
  int32_t encoder_last_error_;

  // Manages buffers containing audio samples from the plugin.
  std::unique_ptr<ppapi::MediaStreamBufferManager> audio_buffer_manager_;

  // Manages buffers containing encoded audio from the browser.
  std::unique_ptr<ppapi::MediaStreamBufferManager> bitstream_buffer_manager_;

  // Media task runner used to run the encoder.
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // The encoder actually doing the work.
  std::unique_ptr<AudioEncoderImpl> encoder_;

  base::WeakPtrFactory<PepperAudioEncoderHost> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PepperAudioEncoderHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_AUDIO_ENCODER_HOST_H_
