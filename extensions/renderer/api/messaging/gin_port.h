// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_MESSAGING_GIN_PORT_H_
#define EXTENSIONS_RENDERER_API_MESSAGING_GIN_PORT_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/mojom/message_port.mojom.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "gin/wrappable.h"
#include "v8/include/v8-forward.h"

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
                                   std::unique_ptr<Message> message) = 0;

    // Closes the port.
    virtual void ClosePort(v8::Local<v8::Context> context,
                           const PortId& port_id) = 0;
  };

  GinPort(v8::Local<v8::Context> context,
          const PortId& port_id,
          const std::string& name,
          const mojom::ChannelType channel_type,
          APIEventHandler* event_handler,
          Delegate* delegate);

  GinPort(const GinPort&) = delete;
  GinPort& operator=(const GinPort&) = delete;

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
                                 std::string_view event_name);

  // Helper method to dispatch an event.
  void DispatchEvent(v8::Local<v8::Context> context,
                     v8::LocalVector<v8::Value>* args,
                     std::string_view event_name);

  // Invalidates the port (due to the context being removed). Any further calls
  // to postMessage() or instantiating new events will fail.
  void OnContextInvalidated();

  // Invalidates the port's events after the port has been disconnected.
  void InvalidateEvents(v8::Local<v8::Context> context);

  // Throws the given |error|.
  void ThrowError(v8::Isolate* isolate, std::string_view error);

  // The current state of the port.
  State state_ = kActive;

  // The associated port id.
  const PortId port_id_;

  // The port's name.
  const std::string name_;

  // The type of the associated channel.
  const mojom::ChannelType channel_type_;

  // The associated APIEventHandler. Guaranteed to outlive this object.
  const raw_ptr<APIEventHandler> event_handler_;

  // The delegate for handling the message passing between ports. Guaranteed to
  // outlive this object.
  const raw_ptr<Delegate, DanglingUntriaged> delegate_;

  // Whether the `sender` property has been accessed, and thus set on the
  // port JS object.
  bool accessed_sender_;

  // A listener for context invalidation. Note: this isn't actually optional;
  // it just needs to be created after |weak_factory_|, which needs to be the
  // final member.
  std::optional<binding::ContextInvalidationListener>
      context_invalidation_listener_;

  base::WeakPtrFactory<GinPort> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_MESSAGING_GIN_PORT_H_
