// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GIN_PORT_H_
#define EXTENSIONS_RENDERER_GIN_PORT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace extensions {
class APIEventHandler;
struct Message;

// A gin::Wrappable implementation of runtime.Port exposed to extensions. This
// provides a means for extensions to communicate with themselves and each
// other. This message-passing usually involves IPCs to the browser; we delegate
// out this responsibility. This class only handles the JS interface (both calls
// from JS and forward events to JS).
class GinPort final : public gin::Wrappable<GinPort> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Posts a message to the port.
    virtual void PostMessageToPort(v8::Local<v8::Context> context,
                                   const PortId& port_id,
                                   int routing_id,
                                   std::unique_ptr<Message> message) = 0;

    // Closes the port.
    virtual void ClosePort(v8::Local<v8::Context> context,
                           const PortId& port_id,
                           int routing_id) = 0;
  };

  GinPort(v8::Local<v8::Context> context,
          const PortId& port_id,
          int routing_id,
          const std::string& name,
          APIEventHandler* event_handler,
          Delegate* delegate);
  ~GinPort() override;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;
  const char* GetTypeName() override;

  // Dispatches an event to any listeners of the onMessage event.
  void DispatchOnMessage(v8::Local<v8::Context> context,
                         const Message& message);

  // Dispatches an event to any listeners of the onDisconnect event and closes
  // the port.
  void DispatchOnDisconnect(v8::Local<v8::Context> context);

  // Sets the |sender| property on the port. Note: this can only be called
  // before the `sender` property is accessed on the JS object, since it is
  // lazily set as a data property in first access.
  void SetSender(v8::Local<v8::Context> context, v8::Local<v8::Value> sender);

  const PortId& port_id() const { return port_id_; }
  int routing_id() const { return routing_id_; }
  const std::string& name() const { return name_; }

  bool is_closed_for_testing() const { return state_ == kDisconnected; }

 private:
  enum State {
    kActive,        // The port is currently active.
    kDisconnected,  // The port was disconnected by calling port.disconnect().
    kInvalidated,   // The associated v8::Context has been invalidated.
  };

  // Handlers for the gin::Wrappable.
  // Port.disconnect()
  void DisconnectHandler(gin::Arguments* arguments);
  // Port.postMessage()
  void PostMessageHandler(gin::Arguments* arguments,
                          v8::Local<v8::Value> v8_message);

  // Port.name
  std::string GetName();
  // Port.onDisconnect
  v8::Local<v8::Value> GetOnDisconnectEvent(gin::Arguments* arguments);
  // Port.onMessage
  v8::Local<v8::Value> GetOnMessageEvent(gin::Arguments* arguments);
  // Port.sender
  v8::Local<v8::Value> GetSender(gin::Arguments* arguments);

  // Helper method to return the event with the given |name| (either
  // onDisconnect or onMessage).
  v8::Local<v8::Object> GetEvent(v8::Local<v8::Context> context,
                                 base::StringPiece event_name);

  // Helper method to dispatch an event.
  void DispatchEvent(v8::Local<v8::Context> context,
                     std::vector<v8::Local<v8::Value>>* args,
                     base::StringPiece event_name);

  // Invalidates the port (due to the context being removed). Any further calls
  // to postMessage() or instantiating new events will fail.
  void OnContextInvalidated();

  // Invalidates the port's events after the port has been disconnected.
  void InvalidateEvents(v8::Local<v8::Context> context);

  // Throws the given |error|.
  void ThrowError(v8::Isolate* isolate, base::StringPiece error);

  // The current state of the port.
  State state_ = kActive;

  // The associated port id.
  PortId port_id_;

  // The routing id associated with the port's context's render frame.
  // TODO(devlin/lazyboy): This won't work with service workers.
  int routing_id_;

  // The port's name.
  std::string name_;

  // The associated APIEventHandler. Guaranteed to outlive this object.
  APIEventHandler* const event_handler_;

  // The delegate for handling the message passing between ports. Guaranteed to
  // outlive this object.
  Delegate* const delegate_;

  // Whether the `sender` property has been accessed, and thus set on the
  // port JS object.
  bool accessed_sender_;

  // A listener for context invalidation. Note: this isn't actually optional;
  // it just needs to be created after |weak_factory_|, which needs to be the
  // final member.
  base::Optional<binding::ContextInvalidationListener>
      context_invalidation_listener_;

  base::WeakPtrFactory<GinPort> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GinPort);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GIN_PORT_H_
