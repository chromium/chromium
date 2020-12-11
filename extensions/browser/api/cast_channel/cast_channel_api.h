// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/cast_channel/cast_channel_enum.h"
#include "components/cast_channel/cast_socket.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/cast_channel.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

class CastChannelAPITest;

namespace content {
class BrowserContext;
}

namespace net {
class IPEndPoint;
}

namespace cast_channel {
class CastSocketService;
}  // namespace cast_channel

namespace extensions {

struct Event;

class CastChannelAPI : public BrowserContextKeyedAPI,
                       public EventRouter::Observer,
                       public base::SupportsWeakPtr<CastChannelAPI> {
 public:
  explicit CastChannelAPI(content::BrowserContext* context);

  static CastChannelAPI* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<CastChannelAPI>* GetFactoryInstance();

  // Sets the CastSocket instance to be used for testing.
  void SetSocketForTest(
      std::unique_ptr<cast_channel::CastSocket> socket_for_test);

  // Returns a test CastSocket instance, if it is defined.
  // Otherwise returns a scoped_ptr with a nullptr value.
  std::unique_ptr<cast_channel::CastSocket> GetSocketForTest();

  // Returns the API browser context.
  content::BrowserContext* GetBrowserContext() const;

  // Sends an event to the extension's EventRouter, if it exists.
  void SendEvent(const std::string& extension_id, std::unique_ptr<Event> event);

  cast_channel::CastSocketService* cast_socket_service() {
    return cast_socket_service_;
  }

 private:
  friend class BrowserContextKeyedAPIFactory<CastChannelAPI>;
  friend class ::CastChannelAPITest;
  friend class CastTransportDelegate;

  // Defines a callback used to send events to the extension's
  // EventRouter.
  //     Parameter #0 is a unique pointer to the event payload.
  using EventDispatchCallback = base::Callback<void(std::unique_ptr<Event>)>;

  // Receives incoming messages and errors and provides additional API context.
  // Created on the UI thread. All methods, including the destructor, must be
  // invoked on the SeqeuncedTaskRunner given by |cast_socket_service_|.
  class CastMessageHandler : public cast_channel::CastSocket::Observer {
   public:
    CastMessageHandler(const EventDispatchCallback& ui_dispatch_cb,
                       cast_channel::CastSocketService* cast_socket_service);
    ~CastMessageHandler() override;

    // Adds |this| as an observer to |cast_socket_service_|.
    void Init();

    // CastSocket::Observer implementation.
    void OnError(const cast_channel::CastSocket& socket,
                 cast_channel::ChannelError error_state) override;
    void OnMessage(const cast_channel::CastSocket& socket,
                   const cast::channel::CastMessage& message) override;

   private:
    // Callback for sending events to the extension.
    // Should be bound to a weak pointer, to prevent any use-after-free
    // conditions.
    EventDispatchCallback const ui_dispatch_cb_;

    // The CastSocketService to observe.
    cast_channel::CastSocketService* const cast_socket_service_;

    SEQUENCE_CHECKER(sequence_checker_);

    DISALLOW_COPY_AND_ASSIGN(CastMessageHandler);
  };

  ~CastChannelAPI() override;

  // EventRouter::Observer:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "CastChannelAPI"; }

  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* const browser_context_;
  std::unique_ptr<cast_channel::CastSocket> socket_for_test_;
  // Created on UI thread, accessed and destroyed on |cast_socket_service_|'s
  // task runner.
  std::unique_ptr<CastMessageHandler> message_handler_;

  cast_channel::CastSocketService* const cast_socket_service_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelAPI);
};

template <>
void BrowserContextKeyedAPIFactory<
    CastChannelAPI>::DeclareFactoryDependencies();

class CastChannelAsyncApiFunction : public AsyncApiFunction {
 public:
  CastChannelAsyncApiFunction();

 protected:
  ~CastChannelAsyncApiFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;
  bool Respond() override;

  // Sets the function result to a ChannelInfo obtained from the state of
  // |socket|.
  void SetResultFromSocket(const cast_channel::CastSocket& socket);

  // Sets the function result to a ChannelInfo populated with |channel_id| and
  // |error|.
  void SetResultFromError(int channel_id,
                          api::cast_channel::ChannelError error);

  // Raw pointer of leaky singleton CastSocketService, which manages creating
  // and removing Cast sockets.
  cast_channel::CastSocketService* cast_socket_service_;

 private:
  // Sets the function result from |channel_info|.
  void SetResultFromChannelInfo(
      const api::cast_channel::ChannelInfo& channel_info);
};

class CastChannelOpenFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelOpenFunction();

 protected:
  ~CastChannelOpenFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.open", CAST_CHANNEL_OPEN)

  // Validates that |connect_info| represents a valid IP end point and returns a
  // new IPEndPoint if so.  Otherwise returns nullptr.
  static net::IPEndPoint* ParseConnectInfo(
      const api::cast_channel::ConnectInfo& connect_info);

  // |socket|: raw pointer of newly created cast channel. Does not take
  // ownership of |socket|.
  void OnOpen(cast_channel::CastSocket* socket);

  std::unique_ptr<api::cast_channel::Open::Params> params_;
  CastChannelAPI* api_;
  std::unique_ptr<net::IPEndPoint> ip_endpoint_;
  base::TimeDelta liveness_timeout_;
  base::TimeDelta ping_interval_;

  FRIEND_TEST_ALL_PREFIXES(CastChannelOpenFunctionTest, TestParseConnectInfo);
  DISALLOW_COPY_AND_ASSIGN(CastChannelOpenFunction);
};

class CastChannelSendFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelSendFunction();

 protected:
  ~CastChannelSendFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.send", CAST_CHANNEL_SEND)

  void OnSend(int result);

  std::unique_ptr<api::cast_channel::Send::Params> params_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelSendFunction);
};

class CastChannelCloseFunction : public CastChannelAsyncApiFunction {
 public:
  CastChannelCloseFunction();

 protected:
  ~CastChannelCloseFunction() override;

  // AsyncApiFunction:
  bool PrePrepare() override;
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  DECLARE_EXTENSION_FUNCTION("cast.channel.close", CAST_CHANNEL_CLOSE)

  void OnClose(int result);

  std::unique_ptr<api::cast_channel::Close::Params> params_;

  DISALLOW_COPY_AND_ASSIGN(CastChannelCloseFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_API_H_
