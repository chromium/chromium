// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_INTERNAL_MESSAGE_DISPATCHER_H_
#define HEADLESS_PUBLIC_INTERNAL_MESSAGE_DISPATCHER_H_

#include "base/functional/callback_forward.h"

namespace base {
class Value;
}

namespace headless {
namespace internal {

// An internal interface for sending DevTools messages from the domain agents.
class MessageDispatcher {
 public:
  virtual void SendMessage(
      const char* method,
      base::Value params,
      base::OnceCallback<void(const base::Value&)> callback) = 0;
  virtual void SendMessage(const char* method,
                           base::Value params,
                           base::OnceClosure callback) = 0;

  virtual void RegisterEventHandler(
      const char* method,
      base::RepeatingCallback<void(const base::Value&)> callback) = 0;

 protected:
  virtual ~MessageDispatcher() {}
};

}  // namespace internal
}  // namespace headless

#endif  // HEADLESS_PUBLIC_INTERNAL_MESSAGE_DISPATCHER_H_
