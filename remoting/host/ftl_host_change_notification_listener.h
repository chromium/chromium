// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FTL_HOST_CHANGE_NOTIFICATION_LISTENER_H_
#define REMOTING_HOST_FTL_HOST_CHANGE_NOTIFICATION_LISTENER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/signaling/signal_strategy.h"

namespace remoting {

// FtlHostChangeNotificationListener listens for messages from remoting backend
// indicating that its host entry has been changed in the directory.
// If a message is received indicating that the host was deleted, it uses the
// OnHostDeleted callback to shut down the host.
class FtlHostChangeNotificationListener : public SignalStrategy::Listener {
 public:
  class Listener {
   protected:
    virtual ~Listener() {}
    // Invoked when a notification that the host was deleted is received.
   public:
    virtual void OnHostDeleted() = 0;
  };

  // Both listener and signal_strategy are expected to outlive this object.
  FtlHostChangeNotificationListener(Listener* listener,
                                    SignalStrategy* signal_strategy);

  FtlHostChangeNotificationListener(const FtlHostChangeNotificationListener&) =
      delete;
  FtlHostChangeNotificationListener& operator=(
      const FtlHostChangeNotificationListener&) = delete;

  ~FtlHostChangeNotificationListener() override;

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;
  bool OnSignalStrategyIncomingMessage(
      const ftl::Id& sender_id,
      const std::string& sender_registration_id,
      const ftl::ChromotingMessage& message) override;

 private:
  void OnHostDeleted();

  raw_ptr<Listener> listener_;
  raw_ptr<SignalStrategy> signal_strategy_;
  base::WeakPtrFactory<FtlHostChangeNotificationListener> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_FTL_HOST_CHANGE_NOTIFICATION_LISTENER_H_
