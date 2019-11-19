// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/resource_converter.h"

#include <stddef.h>

#include "base/bind.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_file_system_host.h"
#include "content/renderer/pepper/pepper_media_stream_audio_track_host.h"
#include "content/renderer/pepper/pepper_media_stream_video_track_host.h"
#include "ipc/ipc_message.h"
#include "media/media_buildflags.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/platform/web_file_system_type.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/web/web_dom_file_system.h"
#include "third_party/blink/public/web/web_dom_media_stream_track.h"
#include "third_party/blink/public/web/web_local_frame.h"

using ppapi::ResourceVar;

namespace content {
namespace {

void FlushComplete(
    base::OnceCallback<void(bool)> callback,
    const std::vector<scoped_refptr<content::HostResourceVar>>& browser_vars,
    const std::vector<int>& pending_host_ids) {
  CHECK(browser_vars.size() == pending_host_ids.size());
  for (size_t i = 0; i < browser_vars.size(); ++i) {
    browser_vars[i]->set_pending_browser_host_id(pending_host_ids[i]);
  }
  std::move(callback).Run(true);
}

PP_FileSystemType WebFileSystemTypeToPPAPI(blink::WebFileSystemType type) {
  switch (type) {
    case blink::WebFileSystemType::kWebFileSystemTypeTemporary:
      return PP_FILESYSTEMTYPE_LOCALTEMPORARY;
    case blink::WebFileSystemType::kWebFileSystemTypePersistent:
      return PP_FILESYSTEMTYPE_LOCALPERSISTENT;
    case blink::WebFileSystemType::kWebFileSystemTypeIsolated:
      return PP_FILESYSTEMTYPE_ISOLATED;
    case blink::WebFileSystemType::kWebFileSystemTypeExternal:
      return PP_FILESYSTEMTYPE_EXTERNAL;
    default:
      NOTREACHED();
      return PP_FILESYSTEMTYPE_LOCALTEMPORARY;
  }
}

// Converts a storage::FileSystemType to a blink::WebFileSystemType.
// Returns true on success, false if |type| does not correspond to a
// WebFileSystemType.
bool FileApiFileSystemTypeToWebFileSystemType(
    storage::FileSystemType type,
    blink::WebFileSystemType* result_type) {
  switch (type) {
    case storage::kFileSystemTypeTemporary:
      *result_type = blink::kWebFileSystemTypeTemporary;
      return true;
    case storage::kFileSystemTypePersistent:
      *result_type = blink::kWebFileSystemTypePersistent;
      return true;
    case storage::kFileSystemTypeIsolated:
      *result_type = blink::kWebFileSystemTypeIsolated;
      return true;
    case storage::kFileSystemTypeExternal:
      *result_type = blink::kWebFileSystemTypeExternal;
      return true;
    default:
      return false;
  }
}

// Given a V8 value containing a DOMFileSystem, creates a resource host and
// returns the resource information for serialization.
// On error, false.
bool DOMFileSystemToResource(
    PP_Instance instance,
    RendererPpapiHost* host,
    const blink::WebDOMFileSystem& dom_file_system,
    int* pending_renderer_id,
    std::unique_ptr<IPC::Message>* create_message,
    std::unique_ptr<IPC::Message>* browser_host_create_message) {
  DCHECK(!dom_file_system.IsNull());

  PP_FileSystemType file_system_type =
      WebFileSystemTypeToPPAPI(dom_file_system.GetType());
  GURL root_url = dom_file_system.RootURL();

  // Raw external file system access is not allowed, but external file system
  // access through fileapi is allowed. (Without this check, there would be a
  // CHECK failure in FileRefResource.)
  if ((file_system_type == PP_FILESYSTEMTYPE_EXTERNAL) &&
      (!root_url.is_valid())) {
    return false;
  }

  *pending_renderer_id = host->GetPpapiHost()->AddPendingResourceHost(
      std::unique_ptr<ppapi::host::ResourceHost>(new PepperFileSystemHost(
          host, instance, 0, root_url, file_system_type)));
  if (*pending_renderer_id == 0)
    return false;

  create_message->reset(
      new PpapiPluginMsg_FileSystem_CreateFromPendingHost(file_system_type));

  browser_host_create_message->reset(
      new PpapiHostMsg_FileSystem_CreateFromRenderer(root_url.spec(),
                                                     file_system_type));
  return true;
}

bool ResourceHostToDOMFileSystem(
    content::PepperFileSystemHost* file_system_host,
    v8::Local<v8::Context> context,
    v8::Local<v8::Value>* dom_file_system) {
  GURL root_url = file_system_host->GetRootUrl();
  GURL origin;
  storage::FileSystemType type;
  base::FilePath virtual_path;
  storage::ParseFileSystemSchemeURL(root_url, &origin, &type, &virtual_path);

  std::string name = storage::GetFileSystemName(origin, type);
  blink::WebFileSystemType blink_type;
  if (!FileApiFileSystemTypeToWebFileSystemType(type, &blink_type))
    return false;
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForContext(context);
  blink::WebDOMFileSystem web_dom_file_system = blink::WebDOMFileSystem::Create(
      frame, blink_type, blink::WebString::FromUTF8(name), root_url,
      blink::WebDOMFileSystem::kSerializableTypeSerializable);
  *dom_file_system =
      web_dom_file_system.ToV8Value(context->Global(), context->GetIsolate());
  return true;
}

bool ResourceHostToDOMMediaStreamVideoTrack(
    content::PepperMediaStreamVideoTrackHost* host,
    v8::Local<v8::Context> context,
    v8::Local<v8::Value>* dom_video_track) {
  // TODO(ronghuawu): Implement this once crbug/352219 is resolved.
  // blink::WebMediaStreamTrack track = host->track();
  // *dom_video_track = track.toV8Value();
  return false;
}

bool DOMMediaStreamTrackToResource(
    PP_Instance instance,
    RendererPpapiHost* host,
    const blink::WebDOMMediaStreamTrack& dom_media_stream_track,
    int* pending_renderer_id,
    std::unique_ptr<IPC::Message>* create_message) {
  DCHECK(!dom_media_stream_track.IsNull());
  *pending_renderer_id = 0;
  const blink::WebMediaStreamTrack track = dom_media_stream_track.Component();
  const std::string id = track.Source().Id().Utf8();

  if (track.Source().GetType() == blink::WebMediaStreamSource::kTypeVideo) {
    *pending_renderer_id = host->GetPpapiHost()->AddPendingResourceHost(
        std::unique_ptr<ppapi::host::ResourceHost>(
            new PepperMediaStreamVideoTrackHost(host, instance, 0, track)));
    if (*pending_renderer_id == 0)
      return false;

    create_message->reset(
        new PpapiPluginMsg_MediaStreamVideoTrack_CreateFromPendingHost(id));
    return true;
  } else if (track.Source().GetType() ==
             blink::WebMediaStreamSource::kTypeAudio) {
    *pending_renderer_id = host->GetPpapiHost()->AddPendingResourceHost(
        std::unique_ptr<ppapi::host::ResourceHost>(
            new PepperMediaStreamAudioTrackHost(host, instance, 0, track)));
    if (*pending_renderer_id == 0)
      return false;

    create_message->reset(
        new PpapiPluginMsg_MediaStreamAudioTrack_CreateFromPendingHost(id));
    return true;
  }
  return false;
}

}  // namespace

ResourceConverter::~ResourceConverter() {}

ResourceConverterImpl::ResourceConverterImpl(PP_Instance instance)
    : instance_(instance) {}

ResourceConverterImpl::~ResourceConverterImpl() {
  // Verify Flush() was called.
  DCHECK(browser_host_create_messages_.empty());
  DCHECK(browser_vars_.empty());
}

bool ResourceConverterImpl::FromV8Value(v8::Local<v8::Object> val,
                                        v8::Local<v8::Context> context,
                                        PP_Var* result,
                                        bool* was_resource) {
  v8::Context::Scope context_scope(context);
  v8::HandleScope handle_scope(context->GetIsolate());
  RendererPpapiHost* host = RendererPpapiHost::GetForPPInstance(instance_);

  *was_resource = false;

  blink::WebDOMFileSystem dom_file_system =
      blink::WebDOMFileSystem::FromV8Value(val);
  if (!dom_file_system.IsNull()) {
    int pending_renderer_id;
    std::unique_ptr<IPC::Message> create_message;
    std::unique_ptr<IPC::Message> browser_host_create_message;
    if (!DOMFileSystemToResource(instance_,
                                 host,
                                 dom_file_system,
                                 &pending_renderer_id,
                                 &create_message,
                                 &browser_host_create_message)) {
      return false;
    }
    DCHECK(create_message);
    DCHECK(browser_host_create_message);
    scoped_refptr<HostResourceVar> result_var =
        CreateResourceVarWithBrowserHost(
            pending_renderer_id, *create_message, *browser_host_create_message);
    *result = result_var->GetPPVar();
    *was_resource = true;
    return true;
  }

  blink::WebDOMMediaStreamTrack dom_media_stream_track =
      blink::WebDOMMediaStreamTrack::FromV8Value(val);
  if (!dom_media_stream_track.IsNull()) {
    int pending_renderer_id;
    std::unique_ptr<IPC::Message> create_message;
    if (!DOMMediaStreamTrackToResource(instance_,
                                       host,
                                       dom_media_stream_track,
                                       &pending_renderer_id,
                                       &create_message)) {
      return false;
    }
    DCHECK(create_message);
    scoped_refptr<HostResourceVar> result_var =
        CreateResourceVar(pending_renderer_id, *create_message);
    *result = result_var->GetPPVar();
    *was_resource = true;
    return true;
  }

  // The value was not convertible to a resource. Return true with
  // |was_resource| set to false. As per the interface of FromV8Value, |result|
  // may be left unmodified in this case.
  return true;
}

void ResourceConverterImpl::Reset() {
  browser_host_create_messages_.clear();
  browser_vars_.clear();
}

bool ResourceConverterImpl::NeedsFlush() {
  return !browser_host_create_messages_.empty();
}

void ResourceConverterImpl::Flush(base::OnceCallback<void(bool)> callback) {
  RendererPpapiHost::GetForPPInstance(instance_)->CreateBrowserResourceHosts(
      instance_, browser_host_create_messages_,
      base::BindOnce(&FlushComplete, std::move(callback), browser_vars_));
  browser_host_create_messages_.clear();
  browser_vars_.clear();
}

bool ResourceConverterImpl::ToV8Value(const PP_Var& var,
                                      v8::Local<v8::Context> context,
                                      v8::Local<v8::Value>* result) {
  DCHECK(var.type == PP_VARTYPE_RESOURCE);

  ResourceVar* resource = ResourceVar::FromPPVar(var);
  if (!resource) {
    NOTREACHED();
    return false;
  }
  PP_Resource resource_id = resource->GetPPResource();

  // Get the renderer-side resource host for this resource.
  content::RendererPpapiHost* renderer_ppapi_host =
      content::RendererPpapiHost::GetForPPInstance(instance_);
  if (!renderer_ppapi_host) {
    // This should never happen: the RendererPpapiHost is owned by the module
    // and should outlive instances associated with it. However, if it doesn't
    // for some reason, we do not want to crash.
    NOTREACHED();
    return false;
  }
  ::ppapi::host::PpapiHost* ppapi_host = renderer_ppapi_host->GetPpapiHost();
  ::ppapi::host::ResourceHost* resource_host =
      ppapi_host->GetResourceHost(resource_id);
  if (resource_host == nullptr) {
    LOG(ERROR) << "No resource host for resource #" << resource_id;
    return false;
  }

  // Convert to the appropriate type of resource host.
  if (resource_host->IsFileSystemHost()) {
    return ResourceHostToDOMFileSystem(
        static_cast<content::PepperFileSystemHost*>(resource_host),
        context,
        result);
  } else if (resource_host->IsMediaStreamVideoTrackHost()) {
    return ResourceHostToDOMMediaStreamVideoTrack(
        static_cast<content::PepperMediaStreamVideoTrackHost*>(resource_host),
        context,
        result);
  } else {
    LOG(ERROR) << "The type of resource #" << resource_id
               << " cannot be converted to a JavaScript object.";
    return false;
  }
}

scoped_refptr<HostResourceVar> ResourceConverterImpl::CreateResourceVar(
    int pending_renderer_id,
    const IPC::Message& create_message) {
  return new HostResourceVar(pending_renderer_id, create_message);
}

scoped_refptr<HostResourceVar>
ResourceConverterImpl::CreateResourceVarWithBrowserHost(
    int pending_renderer_id,
    const IPC::Message& create_message,
    const IPC::Message& browser_host_create_message) {
  scoped_refptr<HostResourceVar> result =
      CreateResourceVar(pending_renderer_id, create_message);
  browser_host_create_messages_.push_back(browser_host_create_message);
  browser_vars_.push_back(result);
  return result;
}

}  // namespace content
