// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_RESOURCE_H_
#define PPAPI_PROXY_PLUGIN_RESOURCE_H_

#include <stdint.h>

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource_callback.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/resource_reply_thread_registrar.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
namespace ppapi {
namespace proxy {

// A "safe" way to run callbacks, doing nothing if they are not
// pending (active).
void SafeRunCallback(scoped_refptr<TrackedCallback>* callback, int32_t error);

class PPAPI_PROXY_EXPORT PluginResource : public Resource {
 public:
  enum Destination {
    RENDERER = 0,
    BROWSER = 1
  };

  PluginResource(Connection connection, PP_Instance instance);
  ~PluginResource() override;

  // Returns true if we've previously sent a create message to the browser
  // or renderer. Generally resources will use these to tell if they should
  // lazily send create messages.
  bool sent_create_to_browser() const { return sent_create_to_browser_; }
  bool sent_create_to_renderer() const { return sent_create_to_renderer_; }

  // This handles a reply to a resource call. It works by looking up the
  // callback that was registered when CallBrowser/CallRenderer was called
  // and calling it with |params| and |msg|.
  void OnReplyReceived(const proxy::ResourceMessageReplyParams& params,
                       const IPC::Message& msg) override;

  // Resource overrides.
  // Note: Subclasses shouldn't override these methods directly. Instead, they
  // should implement LastPluginRefWasDeleted() or InstanceWasDeleted() to get
  // notified.
  void NotifyLastPluginRefWasDeleted() override;
  void NotifyInstanceWasDeleted() override;

  // Sends a create message to the browser or renderer for the current resource.
  void SendCreate(Destination dest, const IPC::Message& msg);

  // When the host returnes a resource to the plugin, it will create a pending
  // ResourceHost and send an ID back to the plugin that identifies the pending
  // object. The plugin uses this function to connect the plugin resource with
  // the pending host resource. See also PpapiHostMsg_AttachToPendingHost. This
  // is in lieu of sending a create message.
  void AttachToPendingHost(Destination dest, int pending_host_id);

  // Sends the given IPC message as a resource request to the host
  // corresponding to this resource object and does not expect a reply.
  void Post(Destination dest, const IPC::Message& msg);

  // Like Post() but expects a response. |callback| is a |base::Callback| that
  // will be run when a reply message with a sequence number matching that of
  // the call is received. |ReplyMsgClass| is the type of the reply message that
  // is expected. An example of usage:
  //
  // Call<PpapiPluginMsg_MyResourceType_MyReplyMessage>(
  //     BROWSER,
  //     PpapiHostMsg_MyResourceType_MyRequestMessage(),
  //     base::Bind(&MyPluginResource::ReplyHandler, base::Unretained(this)));
  //
  // If a reply message to this call is received whose type does not match
  // |ReplyMsgClass| (for example, in the case of an error), the callback will
  // still be invoked but with the default values of the message parameters.
  //
  // Returns the new request's sequence number which can be used to identify
  // the callback. This value will never be 0, which you can use to identify
  // an invalid callback.
  //
  // Note: 1) When all plugin references to this resource are gone or the
  //          corresponding plugin instance is deleted, all pending callbacks
  //          are abandoned.
  //       2) It is *not* recommended to let |callback| hold any reference to
  //          |this|, in which it will be stored. Otherwise, this object will
  //          live forever if we fail to clean up the callback. It is safe to
  //          use base::Unretained(this) or a weak pointer, because this object
  //          will outlive the callback.
  template<typename ReplyMsgClass, typename CallbackType>
  int32_t Call(Destination dest,
               const IPC::Message& msg,
               const CallbackType& callback);

  // Comparing with the previous Call() method, this method takes
  // |reply_thread_hint| as a hint to determine which thread to handle the reply
  // message.
  //
  // If |reply_thread_hint| is non-blocking, the reply message will be handled
  // on the target thread of the callback; otherwise, it will be handled on the
  // main thread.
  //
  // If handling a reply message will cause a TrackedCallback to be run, it is
  // recommended to use this version of Call(). It eliminates unnecessary
  // thread switching and therefore has better performance.
  template<typename ReplyMsgClass, typename CallbackType>
  int32_t Call(Destination dest,
               const IPC::Message& msg,
               const CallbackType& callback,
               scoped_refptr<TrackedCallback> reply_thread_hint);

  // Calls the browser/renderer with sync messages. Returns the pepper error
  // code from the call.
  // |ReplyMsgClass| is the type of the reply message that is expected. If it
  // carries x parameters, then the method with x out parameters should be used.
  // An example of usage:
  //
  // // Assuming the reply message carries a string and an integer.
  // std::string param_1;
  // int param_2 = 0;
  // int32_t result = SyncCall<PpapiPluginMsg_MyResourceType_MyReplyMessage>(
  //     RENDERER, PpapiHostMsg_MyResourceType_MyRequestMessage(),
  //     &param_1, &param_2);
  template <class ReplyMsgClass>
  int32_t SyncCall(Destination dest, const IPC::Message& msg);
  template <class ReplyMsgClass, class A>
  int32_t SyncCall(Destination dest, const IPC::Message& msg, A* a);
  template <class ReplyMsgClass, class A, class B>
  int32_t SyncCall(Destination dest, const IPC::Message& msg, A* a, B* b);
  template <class ReplyMsgClass, class A, class B, class C>
  int32_t SyncCall(Destination dest, const IPC::Message& msg, A* a, B* b, C* c);
  template <class ReplyMsgClass, class A, class B, class C, class D>
  int32_t SyncCall(
      Destination dest, const IPC::Message& msg, A* a, B* b, C* c, D* d);
  template <class ReplyMsgClass, class A, class B, class C, class D, class E>
  int32_t SyncCall(
      Destination dest, const IPC::Message& msg, A* a, B* b, C* c, D* d, E* e);

  int32_t GenericSyncCall(Destination dest,
                          const IPC::Message& msg,
                          IPC::Message* reply_msg,
                          ResourceMessageReplyParams* reply_params);

  const Connection& connection() { return connection_; }

 private:
  IPC::Sender* GetSender(Destination dest) {
    return dest == RENDERER ? connection_.GetRendererSender()
                            : connection_.browser_sender();
  }

  // Helper function to send a |PpapiHostMsg_ResourceCall| to the given
  // destination with |nested_msg| and |call_params|.
  bool SendResourceCall(Destination dest,
                        const ResourceMessageCallParams& call_params,
                        const IPC::Message& nested_msg);

  int32_t GetNextSequence();

  Connection connection_;

  // Use GetNextSequence to retrieve the next value.
  int32_t next_sequence_number_;

  bool sent_create_to_browser_;
  bool sent_create_to_renderer_;

  typedef std::map<int32_t, scoped_refptr<PluginResourceCallbackBase> >
      CallbackMap;
  CallbackMap callbacks_;

  scoped_refptr<ResourceReplyThreadRegistrar> resource_reply_thread_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PluginResource);
};

template<typename ReplyMsgClass, typename CallbackType>
int32_t PluginResource::Call(Destination dest,
                             const IPC::Message& msg,
                             const CallbackType& callback) {
  return Call<ReplyMsgClass>(dest, msg, callback, nullptr);
}

template<typename ReplyMsgClass, typename CallbackType>
int32_t PluginResource::Call(
    Destination dest,
    const IPC::Message& msg,
    const CallbackType& callback,
    scoped_refptr<TrackedCallback> reply_thread_hint) {
  TRACE_EVENT2("ppapi proxy", "PluginResource::Call",
               "Class", IPC_MESSAGE_ID_CLASS(msg.type()),
               "Line", IPC_MESSAGE_ID_LINE(msg.type()));
  ResourceMessageCallParams params(pp_resource(), next_sequence_number_++);
  // Stash the |callback| in |callbacks_| identified by the sequence number of
  // the call.
  scoped_refptr<PluginResourceCallbackBase> plugin_callback(
      new PluginResourceCallback<ReplyMsgClass, CallbackType>(callback));
  callbacks_.insert(std::make_pair(params.sequence(), plugin_callback));
  params.set_has_callback();

  if (resource_reply_thread_registrar_.get()) {
    resource_reply_thread_registrar_->Register(
        pp_resource(), params.sequence(), reply_thread_hint);
  }
  SendResourceCall(dest, params, msg);
  return params.sequence();
}

template <class ReplyMsgClass>
int32_t PluginResource::SyncCall(Destination dest, const IPC::Message& msg) {
  IPC::Message reply;
  ResourceMessageReplyParams reply_params;
  return GenericSyncCall(dest, msg, &reply, &reply_params);
}

template <class ReplyMsgClass, class A>
int32_t PluginResource::SyncCall(
    Destination dest, const IPC::Message& msg, A* a) {
  IPC::Message reply;
  ResourceMessageReplyParams reply_params;
  int32_t result = GenericSyncCall(dest, msg, &reply, &reply_params);

  if (UnpackMessage<ReplyMsgClass>(reply, a))
    return result;
  return PP_ERROR_FAILED;
}

template <class ReplyMsgClass, class A, class B>
int32_t PluginResource::SyncCall(
    Destination dest, const IPC::Message& msg, A* a, B* b) {
  IPC::Message reply;
  ResourceMessageReplyParams reply_params;
  int32_t result = GenericSyncCall(dest, msg, &reply, &reply_params);

  if (UnpackMessage<ReplyMsgClass>(reply, a, b))
    return result;
  return PP_ERROR_FAILED;
}

template <class ReplyMsgClass, class A, class B, class C>
int32_t PluginResource::SyncCall(
    Destination dest, const IPC::Message& msg, A* a, B* b, C* c) {
  IPC::Message reply;
  ResourceMessageReplyParams reply_params;
  int32_t result = GenericSyncCall(dest, msg, &reply, &reply_params);

  if (UnpackMessage<ReplyMsgClass>(reply, a, b, c))
    return result;
  return PP_ERROR_FAILED;
}

template <class ReplyMsgClass, class A, class B, class C, class D>
int32_t PluginResource::SyncCall(
    Destination dest, const IPC::Message& msg, A* a, B* b, C* c, D* d) {
  IPC::Message reply;
  ResourceMessageReplyParams reply_params;
  int32_t result = GenericSyncCall(dest, msg, &reply, &reply_params);

  if (UnpackMessage<ReplyMsgClass>(reply, a, b, c, d))
    return result;
  return PP_ERROR_FAILED;
}

template <class ReplyMsgClass, class A, class B, class C, class D, class E>
int32_t PluginResource::SyncCall(
    Destination dest, const IPC::Message& msg, A* a, B* b, C* c, D* d, E* e) {
  IPC::Message reply;
  ResourceMessageReplyParams reply_params;
  int32_t result = GenericSyncCall(dest, msg, &reply, &reply_params);

  if (UnpackMessage<ReplyMsgClass>(reply, a, b, c, d, e))
    return result;
  return PP_ERROR_FAILED;
}

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_H_
