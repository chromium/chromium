// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_DISPATCHER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_DISPATCHER_H_

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
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

  MessageReceiver* sink_;

  base::WeakPtrFactory<MessageDispatcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MessageDispatcher);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_DISPATCHER_H_
