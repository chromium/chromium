// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/content_renderer_pepper_host_factory.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/pepper/pepper_audio_input_host.h"
#include "content/renderer/pepper/pepper_audio_output_host.h"
#include "content/renderer/pepper/pepper_camera_device_host.h"
#include "content/renderer/pepper/pepper_file_chooser_host.h"
#include "content/renderer/pepper/pepper_file_ref_renderer_host.h"
#include "content/renderer/pepper/pepper_file_system_host.h"
#include "content/renderer/pepper/pepper_graphics_2d_host.h"
#include "content/renderer/pepper/pepper_media_stream_video_track_host.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_url_loader_host.h"
#include "content/renderer/pepper/pepper_video_capture_host.h"
#include "content/renderer/pepper/pepper_video_decoder_host.h"
#include "content/renderer/pepper/pepper_video_encoder_host.h"
#include "content/renderer/pepper/pepper_websocket_host.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "media/media_buildflags.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/ppb_image_data_shared.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_plugin_container.h"

using ppapi::host::ResourceHost;
using ppapi::UnpackMessage;

namespace content {

namespace {

static bool CanUseCameraDeviceAPI(RendererPpapiHost* host,
                                  PP_Instance instance) {
  blink::WebPluginContainer* container =
      host->GetContainerForInstance(instance);
  if (!container)
    return false;

  GURL document_url = container->GetDocument().Url();
  ContentRendererClient* content_renderer_client =
      GetContentClient()->renderer();
  return content_renderer_client->IsPluginAllowedToUseCameraDeviceAPI(
      document_url);
}

}  // namespace

ContentRendererPepperHostFactory::ContentRendererPepperHostFactory(
    RendererPpapiHostImpl* host)
    : host_(host) {}

ContentRendererPepperHostFactory::~ContentRendererPepperHostFactory() {}

std::unique_ptr<ResourceHost>
ContentRendererPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    PP_Resource resource,
    PP_Instance instance,
    const IPC::Message& message) {
  DCHECK(host == host_->GetPpapiHost());

  // Make sure the plugin is giving us a valid instance for this resource.
  if (!host_->IsValidInstance(instance))
    return nullptr;

  PepperPluginInstanceImpl* instance_impl =
      host_->GetPluginInstanceImpl(instance);
  if (!instance_impl->render_frame())
    return nullptr;

  // Public interfaces.
  switch (message.type()) {
    case PpapiHostMsg_FileRef_CreateForFileAPI::ID: {
      PP_Resource file_system;
      std::string internal_path;
      if (!UnpackMessage<PpapiHostMsg_FileRef_CreateForFileAPI>(
              message, &file_system, &internal_path)) {
        NOTREACHED_IN_MIGRATION();
        return nullptr;
      }
      return std::make_unique<PepperFileRefRendererHost>(
          host_, instance, resource, file_system, internal_path);
    }
    case PpapiHostMsg_FileSystem_Create::ID: {
      PP_FileSystemType file_system_type;
      if (!UnpackMessage<PpapiHostMsg_FileSystem_Create>(message,
                                                         &file_system_type)) {
        NOTREACHED_IN_MIGRATION();
        return nullptr;
      }
      return std::make_unique<PepperFileSystemHost>(host_, instance, resource,
                                                    file_system_type);
    }
    case PpapiHostMsg_Graphics2D_Create::ID: {
      PP_Size size;
      PP_Bool is_always_opaque;
      if (!UnpackMessage<PpapiHostMsg_Graphics2D_Create>(
              message, &size, &is_always_opaque)) {
        NOTREACHED_IN_MIGRATION();
        return nullptr;
      }
      ppapi::PPB_ImageData_Shared::ImageDataType image_type =
          ppapi::PPB_ImageData_Shared::PLATFORM;
#if BUILDFLAG(IS_WIN)
      // We use the SIMPLE image data type as the PLATFORM image data type
      // calls GDI functions to create DIB sections etc which fail in Win32K
      // lockdown mode.
      image_type = ppapi::PPB_ImageData_Shared::SIMPLE;
#endif
      scoped_refptr<PPB_ImageData_Impl> image_data(new PPB_ImageData_Impl(
          instance, image_type));
      return base::WrapUnique(PepperGraphics2DHost::Create(
          host_, instance, resource, size, is_always_opaque, image_data));
    }
    case PpapiHostMsg_URLLoader_Create::ID:
      return std::make_unique<PepperURLLoaderHost>(host_, false, instance,
                                                   resource);
    case PpapiHostMsg_VideoDecoder_Create::ID:
      return std::make_unique<PepperVideoDecoderHost>(host_, instance,
                                                      resource);
    case PpapiHostMsg_VideoEncoder_Create::ID:
      return std::make_unique<PepperVideoEncoderHost>(host_, instance,
                                                      resource);
    case PpapiHostMsg_WebSocket_Create::ID:
      return std::make_unique<PepperWebSocketHost>(host_, instance, resource);
    case PpapiHostMsg_MediaStreamVideoTrack_Create::ID:
      return std::make_unique<PepperMediaStreamVideoTrackHost>(host_, instance,
                                                               resource);
  }

  // Dev interfaces.
  if (GetPermissions().HasPermission(ppapi::PERMISSION_DEV)) {
    switch (message.type()) {
      case PpapiHostMsg_AudioInput_Create::ID:
        return std::make_unique<PepperAudioInputHost>(host_, instance,
                                                      resource);
      case PpapiHostMsg_AudioOutput_Create::ID:
        return std::make_unique<PepperAudioOutputHost>(host_, instance,
                                                       resource);
      case PpapiHostMsg_FileChooser_Create::ID:
        return std::make_unique<PepperFileChooserHost>(host_, instance,
                                                       resource);
      case PpapiHostMsg_VideoCapture_Create::ID: {
        std::unique_ptr<PepperVideoCaptureHost> video_host(
            new PepperVideoCaptureHost(host_, instance, resource));
        return video_host->Init() ? std::move(video_host) : nullptr;
      }
    }
  }

  // Permissions of the following interfaces are available for whitelisted apps
  // which may not have access to the other private interfaces.
  if (message.type() == PpapiHostMsg_CameraDevice_Create::ID) {
    if (!GetPermissions().HasPermission(ppapi::PERMISSION_PRIVATE) &&
        !CanUseCameraDeviceAPI(host_, instance))
      return nullptr;
    std::unique_ptr<PepperCameraDeviceHost> camera_host(
        new PepperCameraDeviceHost(host_, instance, resource));
    return camera_host->Init() ? std::move(camera_host) : nullptr;
  }

  return nullptr;
}

const ppapi::PpapiPermissions&
ContentRendererPepperHostFactory::GetPermissions() const {
  return host_->GetPpapiHost()->permissions();
}

}  // namespace content
