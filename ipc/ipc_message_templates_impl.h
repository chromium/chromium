// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_TEMPLATES_IMPL_H_
#define IPC_IPC_MESSAGE_TEMPLATES_IMPL_H_

#include <tuple>

namespace IPC {

template <typename... Ts>
class ParamDeserializer : public MessageReplyDeserializer {
 public:
  explicit ParamDeserializer(const std::tuple<Ts&...>& out) : out_(out) {}

  bool SerializeOutputParameters(const IPC::Message& msg,
                                 base::PickleIterator iter) override {
    return ReadParam(&msg, &iter, &out_);
  }

  std::tuple<Ts&...> out_;
};

template <typename Meta, typename... Ins>
MessageT<Meta, std::tuple<Ins...>, void>::MessageT(Routing routing,
                                                    const Ins&... ins)
    : Message(routing.id, ID, PRIORITY_NORMAL) {
  WriteParam(this, std::tie(ins...));
}

template <typename Meta, typename... Ins>
bool MessageT<Meta, std::tuple<Ins...>, void>::Read(const Message* msg,
                                                     Param* p) {
  base::PickleIterator iter(*msg);
  return ReadParam(msg, &iter, p);
}

template <typename Meta, typename... Ins>
void MessageT<Meta, std::tuple<Ins...>, void>::Log(std::string* name,
                                                    const Message* msg,
                                                    std::string* l) {
  if (name)
    *name = Meta::kName;
  if (!msg || !l)
    return;
  Param p;
  if (Read(msg, &p))
    LogParam(p, l);
}

template <typename Meta, typename... Ins, typename... Outs>
MessageT<Meta, std::tuple<Ins...>, std::tuple<Outs...>>::MessageT(
    Routing routing,
    const Ins&... ins,
    Outs*... outs)
    : SyncMessage(
          routing.id,
          ID,
          PRIORITY_NORMAL,
          std::make_unique<ParamDeserializer<Outs...>>(std::tie(*outs...))) {
  WriteParam(this, std::tie(ins...));
}

template <typename Meta, typename... Ins, typename... Outs>
bool MessageT<Meta, std::tuple<Ins...>, std::tuple<Outs...>>::ReadSendParam(
    const Message* msg,
    SendParam* p) {
  base::PickleIterator iter = SyncMessage::GetDataIterator(msg);
  return ReadParam(msg, &iter, p);
}

template <typename Meta, typename... Ins, typename... Outs>
bool MessageT<Meta, std::tuple<Ins...>, std::tuple<Outs...>>::ReadReplyParam(
    const Message* msg,
    ReplyParam* p) {
  base::PickleIterator iter = SyncMessage::GetDataIterator(msg);
  return ReadParam(msg, &iter, p);
}

template <typename Meta, typename... Ins, typename... Outs>
void MessageT<Meta,
              std::tuple<Ins...>,
              std::tuple<Outs...>>::WriteReplyParams(Message* reply,
                                                      const Outs&... outs) {
  WriteParam(reply, std::tie(outs...));
}

template <typename Meta, typename... Ins, typename... Outs>
void MessageT<Meta, std::tuple<Ins...>, std::tuple<Outs...>>::Log(
    std::string* name,
    const Message* msg,
    std::string* l) {
  if (name)
    *name = Meta::kName;
  if (!msg || !l)
    return;
  if (msg->is_sync()) {
    SendParam p;
    if (ReadSendParam(msg, &p))
      LogParam(p, l);
    AddOutputParamsToLog(msg, l);
  } else {
    ReplyParam p;
    if (ReadReplyParam(msg, &p))
      LogParam(p, l);
  }
}

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_TEMPLATES_IMPL_H_
