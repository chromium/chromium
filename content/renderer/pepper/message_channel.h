// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_MESSAGE_CHANNEL_H_
#define CONTENT_RENDERER_PEPPER_MESSAGE_CHANNEL_H_

#include <list>
#include <map>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/pepper/v8_var_converter.h"
#include "gin/handle.h"
#include "gin/interceptor.h"
#include "gin/wrappable.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/shared_impl/resource.h"
#include "third_party/blink/public/web/web_serialized_script_value.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"
#include "v8/include/v8-util.h"

struct PP_Var;

namespace gin {
class Arguments;
}  // namespace gin

namespace ppapi {
class ScopedPPVar;
}  // namespace ppapi

namespace content {

class PepperPluginInstanceImpl;
class PluginObject;

// MessageChannel implements bidirectional postMessage functionality, allowing
// calls from JavaScript to plugins and vice-versa. See
// PPB_Messaging::PostMessage and PPP_Messaging::HandleMessage for more
// information.
//
// Currently, only 1 MessageChannel can exist, to implement postMessage
// functionality for the instance interfaces.  In the future, when we create a
// MessagePort type in PPAPI, those may be implemented here as well with some
// refactoring.
//   - Separate message ports won't require the passthrough object.
//   - The message target won't be limited to instance, and should support
//     either plugin-provided or JS objects.
// TODO(dmichael):  Add support for separate MessagePorts.
class MessageChannel :
    public gin::Wrappable<MessageChannel>,
    public gin::NamedPropertyInterceptor,
    public ppapi::proxy::HostDispatcher::SyncMessageStatusObserver {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Creates a MessageChannel, returning a pointer to it and sets |result| to
  // the v8 object which is backed by the message channel. The returned pointer
  // is only valid as long as the object in |result| is alive.
  static MessageChannel* Create(PepperPluginInstanceImpl* instance,
                                v8::Persistent<v8::Object>* result);

  MessageChannel(const MessageChannel&) = delete;
  MessageChannel& operator=(const MessageChannel&) = delete;

  ~MessageChannel() override;

  // Called when the instance is deleted. The MessageChannel might outlive the
  // plugin instance because it is garbage collected.
  void InstanceDeleted();

  // Post a message to the onmessage handler for this channel's instance
  // asynchronously.
  void PostMessageToJavaScript(PP_Var message_data);

  // Messages are queued initially. After the PepperPluginInstanceImpl is ready
  // to send and handle messages, users of MessageChannel should call
  // Start().
  void Start();

  // Set the V8Object to which we should forward any calls which aren't
  // related to postMessage. Note that this can be empty; it only gets set if
  // there is a scriptable 'InstanceObject' associated with this channel's
  // instance.
  void SetPassthroughObject(v8::Local<v8::Object> passthrough);

  PepperPluginInstanceImpl* instance() { return instance_; }

  void SetReadOnlyProperty(PP_Var key, PP_Var value);

 private:
  // Struct for storing the result of a v8 object being converted to a PP_Var.
  struct VarConversionResult;

  explicit MessageChannel(PepperPluginInstanceImpl* instance);

  // gin::NamedPropertyInterceptor
  v8::Local<v8::Value> GetNamedProperty(v8::Isolate* isolate,
                                        const std::string& property) override;
  bool SetNamedProperty(v8::Isolate* isolate,
                        const std::string& property,
                        v8::Local<v8::Value> value) override;
  std::vector<std::string> EnumerateNamedProperties(
      v8::Isolate* isolate) override;

  // gin::Wrappable
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // ppapi::proxy::HostDispatcher::SyncMessageStatusObserver
  void BeginBlockOnSyncMessage() override;
  void EndBlockOnSyncMessage() override;

  // Post a message to the plugin's HandleMessage function for this channel's
  // instance.
  void PostMessageToNative(gin::Arguments* args);
  // Post a message to the plugin's HandleBlocking Message function for this
  // channel's instance synchronously, and return a result.
  void PostBlockingMessageToNative(gin::Arguments* args);

  // Post a message to the onmessage handler for this channel's instance
  // synchronously.  This is used by PostMessageToJavaScript.
  void PostMessageToJavaScriptImpl(
      const blink::WebSerializedScriptValue& message_data);

  PluginObject* GetPluginObject(v8::Isolate* isolate);

  void EnqueuePluginMessage(v8::Isolate* isolate,
                            v8::Local<v8::Value> v8_value);

  void FromV8ValueComplete(VarConversionResult* result_holder,
                           const ppapi::ScopedPPVar& result_var,
                           bool success);

  // Drain the queue of messages that are going to the plugin. All "completed"
  // messages at the head of the queue will be sent; any messages awaiting
  // conversion as well as messages after that in the queue will not be sent.
  void DrainCompletedPluginMessages();
  // Drain the queue of messages that are going to JavaScript.
  void DrainJSMessageQueue();
  // PostTask to call DrainJSMessageQueue() soon. Use this when you want to
  // send the messages, but can't immediately (e.g., because the instance is
  // not ready or JavaScript is on the stack).
  void DrainJSMessageQueueSoon();

  void UnregisterSyncMessageStatusObserver();

  v8::Local<v8::FunctionTemplate> GetFunctionTemplate(
      v8::Isolate* isolate,
      const std::string& name,
      void (MessageChannel::*memberFuncPtr)(gin::Arguments* args));

  raw_ptr<PepperPluginInstanceImpl> instance_;

  // We pass all non-postMessage calls through to the passthrough_object_.
  // This way, a plugin can use PPB_Class or PPP_Class_Deprecated and also
  // postMessage.  This is necessary to support backwards-compatibility, and
  // also trusted plugins for which we will continue to support synchronous
  // scripting.
  v8::Persistent<v8::Object> passthrough_object_;

  enum MessageQueueState {
    WAITING_TO_START,  // Waiting for Start() to be called. Queue messages.
    QUEUE_MESSAGES,  // Queue messages temporarily.
    SEND_DIRECTLY,   // Post messages directly.
  };

  // This queue stores values being posted to JavaScript.
  base::circular_deque<blink::WebSerializedScriptValue> js_message_queue_;
  MessageQueueState js_message_queue_state_;

  // True if there is already a posted task to drain the JS message queue.
  bool drain_js_message_queue_scheduled_;

  // When the renderer is sending a blocking message to the plugin, we will
  // queue Plugin->JS messages temporarily to avoid re-entering JavaScript. This
  // counts how many blocking renderer->plugin messages are on the stack so that
  // we only begin sending messages to JavaScript again when the depth reaches
  // zero.
  int blocking_message_depth_;

  // This queue stores vars that are being sent to the plugin. Because
  // conversion can happen asynchronously for object types, the queue stores
  // the var until all previous vars have been converted and sent. This
  // preserves the order in which JS->plugin messages are processed.
  //
  // Note we rely on raw VarConversionResult* pointers remaining valid after
  // calls to push_back or pop_front; hence why we're using list. (deque would
  // probably also work, but is less clearly specified).
  std::list<VarConversionResult> plugin_message_queue_;
  MessageQueueState plugin_message_queue_state_;

  std::map<std::string, ppapi::ScopedPPVar> internal_named_properties_;

  V8VarConverter var_converter_;

  // A callback to invoke at shutdown to ensure we unregister ourselves as
  // Observers for sync messages.
  base::OnceClosure unregister_observer_callback_;

  v8::StdGlobalValueMap<std::string, v8::FunctionTemplate> template_cache_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<MessageChannel> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_MESSAGE_CHANNEL_H_
