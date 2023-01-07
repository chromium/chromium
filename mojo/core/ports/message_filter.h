// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_MESSAGE_FILTER_H_
#define MOJO_CORE_PORTS_MESSAGE_FILTER_H_

namespace mojo {
namespace core {
namespace ports {

class UserMessageEvent;

// An interface which can be implemented to user message events according to
// arbitrary policy.
class MessageFilter {
 public:
  virtual ~MessageFilter() = default;

  // Returns true if |message| should be accepted by whomever is applying this
  // filter. See MessageQueue::GetNextMessage(), for example.
  virtual bool Match(const UserMessageEvent& message) = 0;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_MESSAGE_FILTER_H_
