// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_RESOURCE_H_
#define PPAPI_SHARED_IMPL_RESOURCE_H_

#include <stddef.h>  // For NULL.

#include <string>

#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/shared_impl/host_resource.h"

// All resource types should be added here. This implements our hand-rolled
// RTTI system since we don't compile with "real" RTTI.
#define FOR_ALL_PPAPI_RESOURCE_APIS(F)  \
  F(PPB_Audio_API)                      \
  F(PPB_AudioBuffer_API)                \
  F(PPB_AudioConfig_API)                \
  F(PPB_AudioInput_API)                 \
  F(PPB_AudioOutput_API)                \
  F(PPB_AudioTrusted_API)               \
  F(PPB_BrowserFont_Singleton_API)      \
  F(PPB_BrowserFont_Trusted_API)        \
  F(PPB_Buffer_API)                     \
  F(PPB_CameraCapabilities_API)         \
  F(PPB_CameraDevice_API)               \
  F(PPB_DeviceRef_API)                  \
  F(PPB_Ext_CrxFileSystem_Private_API)  \
  F(PPB_FileChooser_API)                \
  F(PPB_FileIO_API)                     \
  F(PPB_FileRef_API)                    \
  F(PPB_FileSystem_API)                 \
  F(PPB_Gamepad_API)                    \
  F(PPB_Graphics2D_API)                 \
  F(PPB_Graphics3D_API)                 \
  F(PPB_HostResolver_API)               \
  F(PPB_HostResolver_Private_API)       \
  F(PPB_ImageData_API)                  \
  F(PPB_InputEvent_API)                 \
  F(PPB_IsolatedFileSystem_Private_API) \
  F(PPB_MediaStreamAudioTrack_API)      \
  F(PPB_MediaStreamVideoTrack_API)      \
  F(PPB_MessageLoop_API)                \
  F(PPB_NetAddress_API)                 \
  F(PPB_NetworkList_API)                \
  F(PPB_NetworkMonitor_API)             \
  F(PPB_NetworkProxy_API)               \
  F(PPB_Printing_API)                   \
  F(PPB_Scrollbar_API)                  \
  F(PPB_TCPServerSocket_Private_API)    \
  F(PPB_TCPSocket_API)                  \
  F(PPB_TCPSocket_Private_API)          \
  F(PPB_UDPSocket_API)                  \
  F(PPB_UDPSocket_Private_API)          \
  F(PPB_UMA_Singleton_API)              \
  F(PPB_URLLoader_API)                  \
  F(PPB_URLRequestInfo_API)             \
  F(PPB_URLResponseInfo_API)            \
  F(PPB_VideoCapture_API)               \
  F(PPB_VideoDecoder_API)               \
  F(PPB_VideoDecoder_Dev_API)           \
  F(PPB_VideoEncoder_API)               \
  F(PPB_VideoFrame_API)                 \
  F(PPB_VideoLayer_API)                 \
  F(PPB_View_API)                       \
  F(PPB_VpnProvider_API)                \
  F(PPB_WebSocket_API)                  \
  F(PPB_Widget_API)                     \
  F(PPB_X509Certificate_Private_API)

namespace IPC {
class Message;
}

namespace ppapi {

// Normally we shouldn't reply on proxy here, but this is to support
// OnReplyReceived. See that comment.
namespace proxy {
class ResourceMessageReplyParams;
}

// Forward declare all the resource APIs.
namespace thunk {
#define DECLARE_RESOURCE_CLASS(RESOURCE) class RESOURCE;
FOR_ALL_PPAPI_RESOURCE_APIS(DECLARE_RESOURCE_CLASS)
#undef DECLARE_RESOURCE_CLASS
}  // namespace thunk

// Resources have slightly different registration behaviors when the're an
// in-process ("impl") resource in the host (renderer) process, or when they're
// a proxied resource in the plugin process. This enum differentiates those
// cases.
enum ResourceObjectType { OBJECT_IS_IMPL, OBJECT_IS_PROXY };

class PPAPI_SHARED_EXPORT Resource
    : public base::RefCountedThreadSafe<Resource> {
 public:
  Resource() = delete;

  // Constructor for impl and non-proxied, instance-only objects.
  //
  // For constructing "impl" (non-proxied) objects, this just takes the
  // associated instance, and generates a new resource ID. The host resource
  // will be the same as the newly-generated resource ID. For all objects in
  // the renderer (host) process, you'll use this constructor and call it with
  // OBJECT_IS_IMPL.
  //
  // For proxied objects, this will create an "instance-only" object which
  // lives only in the plugin and doesn't have a corresponding object in the
  // host. If you have a host resource ID, use the constructor below which
  // takes that HostResource value.
  Resource(ResourceObjectType type, PP_Instance instance);

  // For constructing given a host resource.
  //
  // For OBJECT_IS_PROXY objects, this takes the resource generated in the host
  // side, stores it, and allocates a "local" resource ID for use in the
  // current process.
  //
  // For OBJECT_IS_IMPL, the host resource ID must be 0, since there should be
  // no host resource generated (impl objects should generate their own). The
  // reason for supporting this constructor at all for the IMPL case is that
  // some shared objects use a host resource for both modes to keep things the
  // same.
  Resource(ResourceObjectType type, const HostResource& host_resource);

  // Constructor for untracked objects. These have no associated instance. Use
  // this with care, as the object is likely to persist for the lifetime of the
  // plugin module. This is appropriate in some rare cases, like the
  // PPB_MessageLoop resource for the main thread.
  struct Untracked {};
  explicit Resource(Untracked);

  Resource(const Resource&) = delete;
  Resource& operator=(const Resource&) = delete;

  virtual ~Resource();

  PP_Instance pp_instance() const { return host_resource_.instance(); }

  // Returns the resource ID for this object in the current process without
  // adjusting the refcount. See also GetReference().
  PP_Resource pp_resource() const { return pp_resource_; }

  // Returns the host resource which identifies the resource in the host side
  // of the process in the case of proxied objects. For in-process objects,
  // this just identifies the in-process resource ID & instance.
  const HostResource& host_resource() { return host_resource_; }

  // Adds a ref on behalf of the plugin and returns the resource ID. This is
  // normally used when returning a resource to the plugin, where it's
  // expecting the returned resource to have ownership of a ref passed.
  // See also pp_resource() to avoid the AddRef.
  PP_Resource GetReference();

  // Called by the resource tracker when the last reference from the plugin
  // was released. For a few types of resources, the resource could still
  // stay alive if there are other references held by the PPAPI implementation
  // (possibly for callbacks and things).
  //
  // Note that subclasses except PluginResource should override
  // LastPluginRefWasDeleted() to be notified.
  virtual void NotifyLastPluginRefWasDeleted();

  // Called by the resource tracker when the instance is going away but the
  // object is still alive (this is not the common case, since it requires
  // something in the implementation to be keeping a ref that keeps the
  // resource alive.
  //
  // You will want to override this if your resource does some kind of
  // background processing (like maybe network loads) on behalf of the plugin
  // and you want to stop that when the plugin is deleted.
  //
  // Note that subclasses except PluginResource should override
  // InstanceWasDeleted() to be notified.
  virtual void NotifyInstanceWasDeleted();

// Dynamic casting for this object. Returns the pointer to the given type if
// it's supported. Derived classes override the functions they support to
// return the interface.
#define DEFINE_TYPE_GETTER(RESOURCE) virtual thunk::RESOURCE* As##RESOURCE();
  FOR_ALL_PPAPI_RESOURCE_APIS(DEFINE_TYPE_GETTER)
#undef DEFINE_TYPE_GETTER

  // Template-based dynamic casting. See specializations below. This is
  // unimplemented for the default case. This way, for anything that's not a
  // resource (or if a developer forgets to add the resource to the list in
  // this file), the result is a linker error.
  template <typename T>
  T* GetAs();

  // Called when a PpapiPluginMsg_ResourceReply reply is received for a
  // previous CallRenderer. The message is the nested reply message, which may
  // be an empty message (depending on what the host sends).
  //
  // The default implementation will assert (if you send a request, you should
  // override this function).
  //
  // (This function would make more conceptual sense on PluginResource but we
  // need to call this function from general code that doesn't know how to
  // distinguish the classes.)
  virtual void OnReplyReceived(const proxy::ResourceMessageReplyParams& params,
                               const IPC::Message& msg);

 protected:
  // Logs a message to the console from this resource.
  void Log(PP_LogLevel level, const std::string& message);

  // Removes the resource from the ResourceTracker's tables. This normally
  // happens as part of Resource destruction, but if a subclass destructor
  // has a risk of re-entering destruction via the ResourceTracker, it can
  // call this explicitly to get rid of the table entry before continuing
  // with the destruction. If the resource is not in the ResourceTracker's
  // tables, silently does nothing. See http://crbug.com/159429.
  void RemoveFromResourceTracker();

  // Notifications for subclasses.
  virtual void LastPluginRefWasDeleted() {}
  virtual void InstanceWasDeleted() {}

 private:
  // See the getters above.
  PP_Resource pp_resource_;
  HostResource host_resource_;
};

// Template-based dynamic casting. These specializations forward to the
// AsXXX virtual functions to return whether the given type is supported.
#define DEFINE_RESOURCE_CAST(RESOURCE)        \
  template <>                                 \
  inline thunk::RESOURCE* Resource::GetAs() { \
    return As##RESOURCE();                    \
  }
FOR_ALL_PPAPI_RESOURCE_APIS(DEFINE_RESOURCE_CAST)
#undef DEFINE_RESOURCE_CAST

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_RESOURCE_H_
