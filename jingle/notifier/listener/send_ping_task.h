// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Methods for sending the update stanza to notify peers via xmpp.

#ifndef JINGLE_NOTIFIER_LISTENER_SEND_PING_TASK_H_
#define JINGLE_NOTIFIER_LISTENER_SEND_PING_TASK_H_

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/libjingle_xmpp/xmpp/xmpptask.h"

namespace jingle_xmpp {
class XmlElement;
}  // namespace

namespace notifier {

class SendPingTask : public jingle_xmpp::XmppTask {
 public:
  class Delegate {
   public:
    virtual void OnPingResponseReceived() = 0;

   protected:
    virtual ~Delegate();
  };

  SendPingTask(jingle_xmpp::XmppTaskParentInterface* parent, Delegate* delegate);
  ~SendPingTask() override;

  // Overridden from jingle_xmpp::XmppTask.
  int ProcessStart() override;
  int ProcessResponse() override;
  bool HandleStanza(const jingle_xmpp::XmlElement* stanza) override;

 private:
  static jingle_xmpp::XmlElement* MakePingStanza(const std::string& task_id);

  FRIEND_TEST_ALL_PREFIXES(SendPingTaskTest, MakePingStanza);

  std::string ping_task_id_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SendPingTask);
};

typedef SendPingTask::Delegate SendPingTaskDelegate;

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_LISTENER_SEND_PING_TASK_H_
