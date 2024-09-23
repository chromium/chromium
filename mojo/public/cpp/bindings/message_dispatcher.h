// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_DISPATCHER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_DISPATCHER_H_

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) MessageDispatcher
    : public MessageReceiver {
 public:
  // Doesn't take ownership of |sink|. Therefore |sink| has to stay alive while
  // this object is alive.
  explicit MessageDispatcher(MessageReceiver* sink = nullptr);

  MessageDispatcher(MessageDispatcher&& other);
  MessageDispatcher& operator=(MessageDispatcher&& other);

  MessageDispatcher(const MessageDispatcher&) = delete;
  MessageDispatcher& operator=(const MessageDispatcher&) = delete;

  ~MessageDispatcher() override;

  void SetValidator(std::unique_ptr<MessageReceiver> validator);
  void SetFilter(std::unique_ptr<MessageFilter> filter);

  // Doesn't take ownership of |sink|. Therefore |sink| has to stay alive while
  // this object is alive.
  void SetSink(MessageReceiver* sink);

  // MessageReceiver:
  bool Accept(Message* message) override;

 private:
  std::unique_ptr<MessageReceiver> validator_;
  std::unique_ptr<MessageFilter> filter_;

  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of speedometer3).
  RAW_PTR_EXCLUSION MessageReceiver* sink_ = nullptr;

  base::WeakPtrFactory<MessageDispatcher> weak_factory_{this};
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_DISPATCHER_H_
