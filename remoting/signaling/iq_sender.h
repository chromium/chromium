// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_IQ_SENDER_H_
#define REMOTING_SIGNALING_IQ_SENDER_H_

#include <map>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/signaling/signal_strategy.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting {

class IqRequest;
class SignalStrategy;

// IqSender handles sending iq requests and routing of responses to
// those requests.
class IqSender : public SignalStrategy::Listener {
 public:
  // Callback that is called when an Iq response is received. Called
  // with the |response| set to nullptr in case of a timeout.
  using ReplyCallback =
      base::OnceCallback<void(IqRequest* request,
                              const jingle_xmpp::XmlElement* response)>;

  explicit IqSender(SignalStrategy* signal_strategy);

  IqSender(const IqSender&) = delete;
  IqSender& operator=(const IqSender&) = delete;

  ~IqSender() override;

  // Send an iq stanza. Returns an IqRequest object that represends
  // the request. |callback| is called when response to |stanza| is
  // received. Destroy the returned IqRequest to cancel the callback.
  // Caller must take ownership of the result. Result must be
  // destroyed before sender is destroyed.
  std::unique_ptr<IqRequest> SendIq(
      std::unique_ptr<jingle_xmpp::XmlElement> stanza,
      ReplyCallback callback);

  // Same as above, but also formats the message.
  std::unique_ptr<IqRequest> SendIq(
      const std::string& type,
      const std::string& addressee,
      std::unique_ptr<jingle_xmpp::XmlElement> iq_body,
      ReplyCallback callback);

  // SignalStrategy::Listener implementation.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

 private:
  typedef std::map<std::string, raw_ptr<IqRequest, CtnExperimental>>
      IqRequestMap;
  friend class IqRequest;

  // Helper function used to create iq stanzas.
  static std::unique_ptr<jingle_xmpp::XmlElement> MakeIqStanza(
      const std::string& type,
      const std::string& addressee,
      std::unique_ptr<jingle_xmpp::XmlElement> iq_body);

  // Removes |request| from the list of pending requests. Called by IqRequest.
  void RemoveRequest(IqRequest* request);

  raw_ptr<SignalStrategy> signal_strategy_;
  IqRequestMap requests_;
};

// This call must only be used on the thread it was created on.
class IqRequest {
 public:
  IqRequest(IqSender* sender,
            IqSender::ReplyCallback callback,
            const std::string& addressee);

  IqRequest(const IqRequest&) = delete;
  IqRequest& operator=(const IqRequest&) = delete;

  ~IqRequest();

  // Sets timeout for the request. When the timeout expires the
  // callback is called with the |response| set to nullptr.
  void SetTimeout(base::TimeDelta timeout);

 private:
  friend class IqSender;

  void CallCallback(const jingle_xmpp::XmlElement* stanza);
  void OnTimeout();

  // Called by IqSender when a response is received.
  void OnResponse(const jingle_xmpp::XmlElement* stanza);

  void DeliverResponse(std::unique_ptr<jingle_xmpp::XmlElement> stanza);

  raw_ptr<IqSender> sender_;
  IqSender::ReplyCallback callback_;
  std::string addressee_;

  base::WeakPtrFactory<IqRequest> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_IQ_SENDER_H_
