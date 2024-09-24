// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_TEMPLATES_H_
#define IPC_IPC_MESSAGE_TEMPLATES_H_

#include <stdint.h>

#include <tuple>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "base/tuple.h"
#include "build/build_config.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_sync_message.h"

namespace IPC {

template <typename Tuple, size_t... Ns>
auto TupleForwardImpl(Tuple&& tuple, std::index_sequence<Ns...>) -> decltype(
    std::forward_as_tuple(std::get<Ns>(std::forward<Tuple>(tuple))...)) {
  return std::forward_as_tuple(std::get<Ns>(std::forward<Tuple>(tuple))...);
}

// Transforms std::tuple contents to the forwarding form.
// Example:
//   std::tuple<int, int&, const int&, int&&>&&
//     -> std::tuple<int&&, int&, const int&, int&&>.
//   const std::tuple<int, const int&, int&&>&
//     -> std::tuple<const int&, int&, const int&, int&>.
//
// TupleForward(std::make_tuple(a, b, c)) is equivalent to
// std::forward_as_tuple(a, b, c).
template <typename Tuple>
auto TupleForward(Tuple&& tuple) -> decltype(TupleForwardImpl(
    std::forward<Tuple>(tuple),
    std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>())) {
  return TupleForwardImpl(
      std::forward<Tuple>(tuple),
      std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>());
}

// This function is for all the async IPCs that don't pass an extra parameter
// using IPC_BEGIN_MESSAGE_MAP_WITH_PARAM.
template <typename ObjT, typename Method, typename P, typename Tuple>
void DispatchToMethod(ObjT* obj, Method method, P*, Tuple&& tuple) {
  base::DispatchToMethod(obj, method, std::forward<Tuple>(tuple));
}

template <typename ObjT,
          typename Method,
          typename P,
          typename Tuple,
          size_t... Ns>
void DispatchToMethodImpl(ObjT* obj,
                          Method method,
                          P* parameter,
                          Tuple&& tuple,
                          std::index_sequence<Ns...>) {
  (obj->*method)(parameter, std::get<Ns>(std::forward<Tuple>(tuple))...);
}

// The following function is for async IPCs which have a dispatcher with an
// extra parameter specified using IPC_BEGIN_MESSAGE_MAP_WITH_PARAM.
template <typename ObjT, typename P, typename... Args, typename Tuple>
std::enable_if_t<sizeof...(Args) == std::tuple_size<std::decay_t<Tuple>>::value>
DispatchToMethod(ObjT* obj,
                 void (ObjT::*method)(P*, Args...),
                 P* parameter,
                 Tuple&& tuple) {
  constexpr size_t size = std::tuple_size<std::decay_t<Tuple>>::value;
  DispatchToMethodImpl(obj, method, parameter, std::forward<Tuple>(tuple),
                       std::make_index_sequence<size>());
}

enum class MessageKind {
  CONTROL,
  ROUTED,
};

// Routing is a helper struct so MessageT's private common constructor has a
// different type signature than the public "int32_t routing_id" one.
struct Routing {
  explicit Routing(int32_t id) : id(id) {}
  int32_t id;
};

// We want to restrict MessageT's constructors so that a routing_id is always
// provided for ROUTED messages and never provided for CONTROL messages, so
// use the SFINAE technique from N4387's "Implementation Hint" section.
#define IPC_MESSAGET_SFINAE(x) \
  template <bool X = (x), typename std::enable_if<X, bool>::type = false>

// MessageT is the common template used for all user-defined message types.
// It's intended to be used via the macros defined in ipc_message_macros.h.
template <typename Meta,
          typename InTuple = typename Meta::InTuple,
          typename OutTuple = typename Meta::OutTuple>
class MessageT;

// Asynchronous message partial specialization.
template <typename Meta, typename... Ins>
class MessageT<Meta, std::tuple<Ins...>, void> : public Message {
 public:
  using Param = std::tuple<Ins...>;
  enum { ID = Meta::ID };

  // TODO(mdempsky): Remove.  Uses of MyMessage::Schema::Param can be replaced
  // with just MyMessage::Param.
  using Schema = MessageT;

  IPC_MESSAGET_SFINAE(Meta::kKind == MessageKind::CONTROL)
  MessageT(const Ins&... ins) : MessageT(Routing(MSG_ROUTING_CONTROL), ins...) {
    DCHECK(Meta::kKind == MessageKind::CONTROL) << Meta::kName;
  }

  IPC_MESSAGET_SFINAE(Meta::kKind == MessageKind::ROUTED)
  MessageT(int32_t routing_id, const Ins&... ins)
      : MessageT(Routing(routing_id), ins...) {
    DCHECK(Meta::kKind == MessageKind::ROUTED) << Meta::kName;
  }

  static bool Read(const Message* msg, Param* p);
  static void Log(std::string* name, const Message* msg, std::string* l);

  template <class T, class S, class P, class Method>
  static bool Dispatch(const Message* msg,
                       T* obj,
                       S* sender,
                       P* parameter,
                       Method func) {
    TRACE_EVENT0("ipc", Meta::kName);
    Param p;
    if (Read(msg, &p)) {
      DispatchToMethod(obj, func, parameter, std::move(p));
      return true;
    }
    return false;
  }

 private:
  MessageT(Routing routing, const Ins&... ins);
};

// Synchronous message partial specialization.
template <typename Meta, typename... Ins, typename... Outs>
class MessageT<Meta, std::tuple<Ins...>, std::tuple<Outs...>>
    : public SyncMessage {
 public:
  using SendParam = std::tuple<Ins...>;
  using ReplyParam = std::tuple<Outs...>;
  enum { ID = Meta::ID };

  // TODO(mdempsky): Remove.  Uses of MyMessage::Schema::{Send,Reply}Param can
  // be replaced with just MyMessage::{Send,Reply}Param.
  using Schema = MessageT;

  IPC_MESSAGET_SFINAE(Meta::kKind == MessageKind::CONTROL)
  MessageT(const Ins&... ins, Outs*... outs)
      : MessageT(Routing(MSG_ROUTING_CONTROL), ins..., outs...) {
    DCHECK(Meta::kKind == MessageKind::CONTROL) << Meta::kName;
  }

  IPC_MESSAGET_SFINAE(Meta::kKind == MessageKind::ROUTED)
  MessageT(int32_t routing_id, const Ins&... ins, Outs*... outs)
      : MessageT(Routing(routing_id), ins..., outs...) {
    DCHECK(Meta::kKind == MessageKind::ROUTED) << Meta::kName;
  }

  static bool ReadSendParam(const Message* msg, SendParam* p);
  static bool ReadReplyParam(const Message* msg, ReplyParam* p);
  static void WriteReplyParams(Message* reply, const Outs&... outs);
  static void Log(std::string* name, const Message* msg, std::string* l);

  template <class T, class S, class P, class Method>
  static bool Dispatch(const Message* msg,
                       T* obj,
                       S* sender,
                       P* /* parameter */,
                       Method func) {
    TRACE_EVENT0("ipc", Meta::kName);
    SendParam send_params;
    bool ok = ReadSendParam(msg, &send_params);
    Message* reply = SyncMessage::GenerateReply(msg);
    if (!ok) {
      NOTREACHED() << "Error deserializing message " << msg->type();
    }

    ReplyParam reply_params;
    base::DispatchToMethod(obj, func, std::move(send_params), &reply_params);
    WriteParam(reply, reply_params);
    LogReplyParamsToMessage(reply_params, msg);
    sender->Send(reply);
    return true;
  }

  template <class T, class P, class Method>
  static bool DispatchDelayReply(const Message* msg,
                                 T* obj,
                                 P* /* parameter */,
                                 Method func) {
    TRACE_EVENT0("ipc", Meta::kName);
    SendParam send_params;
    bool ok = ReadSendParam(msg, &send_params);
    Message* reply = SyncMessage::GenerateReply(msg);
    if (!ok) {
      NOTREACHED() << "Error deserializing message " << msg->type();
    }

    std::tuple<Message&> t = std::tie(*reply);
    ConnectMessageAndReply(msg, reply);
    base::DispatchToMethod(obj, func, std::move(send_params), &t);
    return true;
  }

  template <class T, class P, class Method>
  static bool DispatchWithParamDelayReply(const Message* msg,
                                          T* obj,
                                          P* parameter,
                                          Method func) {
    TRACE_EVENT0("ipc", Meta::kName);
    SendParam send_params;
    bool ok = ReadSendParam(msg, &send_params);
    Message* reply = SyncMessage::GenerateReply(msg);
    if (!ok) {
      NOTREACHED() << "Error deserializing message " << msg->type();
    }

    std::tuple<Message&> t = std::tie(*reply);
    ConnectMessageAndReply(msg, reply);
    std::tuple<P*> parameter_tuple(parameter);
    base::DispatchToMethod(
        obj, func,
        std::tuple_cat(std::move(parameter_tuple), TupleForward(send_params)),
        &t);
    return true;
  }

 private:
  MessageT(Routing routing, const Ins&... ins, Outs*... outs);
};

}  // namespace IPC

#if defined(IPC_MESSAGE_IMPL)
#include "ipc/ipc_message_templates_impl.h"
#endif

#endif  // IPC_IPC_MESSAGE_TEMPLATES_H_
