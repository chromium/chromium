// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/utility/websocket/websocket_api.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/websocket.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef SendMessage
#undef SendMessage
#endif

namespace pp {

class WebSocketAPI::Implement : public WebSocket {
 public:
  Implement(Instance* instance, WebSocketAPI* api)
      : WebSocket(instance),
        api_(api),
        callback_factory_(this) {
  }

  virtual ~Implement() {}

  int32_t Connect(const Var& url, const Var protocols[],
                  uint32_t protocol_count) {
    CompletionCallback callback =
        callback_factory_.NewOptionalCallback(&Implement::DidConnect);
    int32_t result =
        WebSocket::Connect(url, protocols, protocol_count, callback);
    if (result != PP_OK_COMPLETIONPENDING) {
      // In synchronous cases, consumes callback here and invokes callback
      // with PP_ERROR_ABORTED instead of result in order to avoid side effects
      // in DidConnect. DidConnect ignores this invocation and doesn't call
      // any delegate virtual method.
      callback.Run(PP_ERROR_ABORTED);
    }
    return result;
  }

  int32_t Close(uint16_t code, const Var& reason) {
    CompletionCallback callback =
        callback_factory_.NewOptionalCallback(&Implement::DidClose);
    int32_t result = WebSocket::Close(code, reason, callback);
    if (result != PP_OK_COMPLETIONPENDING) {
      // In synchronous cases, consumes callback here and invokes callback
      // with PP_ERROR_ABORTED instead of result in order to avoid side effects
      // in DidConnect. DidConnect ignores this invocation and doesn't call
      // any delegate virtual method.
      callback.Run(PP_ERROR_ABORTED);
    }
    return result;
  }

  void Receive() {
    int32_t result;
    do {
      CompletionCallback callback =
          callback_factory_.NewOptionalCallback(&Implement::DidReceive);
      result = WebSocket::ReceiveMessage(&receive_message_var_, callback);
      if (result != PP_OK_COMPLETIONPENDING)
        callback.Run(result);
    } while (result == PP_OK);
  }

  void DidConnect(int32_t result) {
    if (result == PP_OK) {
      api_->WebSocketDidOpen();
      Receive();
    } else if (result != PP_ERROR_ABORTED) {
      DidClose(result);
    }
  }

  void DidReceive(int32_t result) {
    if (result == PP_OK) {
      api_->HandleWebSocketMessage(receive_message_var_);
      Receive();
    } else if (result != PP_ERROR_ABORTED) {
      DidClose(result);
    }
  }

  void DidClose(int32_t result) {
    if (result == PP_ERROR_ABORTED)
      return;
    bool was_clean = GetCloseWasClean();
    if (!was_clean)
      api_->HandleWebSocketError();
    api_->WebSocketDidClose(was_clean, GetCloseCode(), GetCloseReason());
  }

 private:
  WebSocketAPI* api_;
  CompletionCallbackFactory<Implement> callback_factory_;
  Var receive_message_var_;
};

WebSocketAPI::WebSocketAPI(Instance* instance)
    : impl_(new Implement(instance, this)) {
}

WebSocketAPI::~WebSocketAPI() {
  delete impl_;
}

int32_t WebSocketAPI::Connect(const Var& url, const Var protocols[],
                              uint32_t protocol_count) {
  return impl_->Connect(url, protocols, protocol_count);
}

int32_t WebSocketAPI::Close(uint16_t code, const Var& reason) {
  return impl_->Close(code, reason);
}

int32_t WebSocketAPI::Send(const Var& data) {
  return impl_->SendMessage(data);
}

uint64_t WebSocketAPI::GetBufferedAmount() {
  return impl_->GetBufferedAmount();
}

Var WebSocketAPI::GetExtensions() {
  return impl_->GetExtensions();
}

Var WebSocketAPI::GetProtocol() {
  return impl_->GetProtocol();
}

PP_WebSocketReadyState WebSocketAPI::GetReadyState() {
  return impl_->GetReadyState();
}

Var WebSocketAPI::GetURL() {
  return impl_->GetURL();
}

}  // namespace pp
