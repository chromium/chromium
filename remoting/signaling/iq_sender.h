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

namespace remoting {

class IqRequest;
class JingleMessage;
struct JingleMessageReply;
class SignalStrategy;

// Handles sending IQ requests and the routing of responses to those requests.
class IqSender : public SignalStrategy::Listener {
 public:
  // Called when an IQ response is received.
  using ReplyCallback =
      base::OnceCallback<void(IqRequest* request,
                              const JingleMessageReply& response)>;

  explicit IqSender(SignalStrategy* signal_strategy);

  IqSender(const IqSender&) = delete;
  IqSender& operator=(const IqSender&) = delete;

  ~IqSender() override;

  // Send a Jingle IQ. Returns an IqRequest instance which maps to the request.
  // |callback| is called when a response to |message| is received. Destroying
  // the IqRequest instance will cancel the callback. The IqRequest instance
  // must be destroyed before the IqSender instance is destroyed.
  std::unique_ptr<IqRequest> SendIq(JingleMessage&& message,
                                    ReplyCallback callback);

  // SignalStrategy::Listener implementation.
  void OnSignalingStateChanged(SignalStrategy::State state) override;
  bool OnSignalingReply(const SignalingAddress& sender_address,
                        const JingleMessageReply& message) override;

 private:
  typedef std::map<std::string, raw_ptr<IqRequest, CtnExperimental>>
      IqRequestMap;
  friend class IqRequest;

  // Removes |request| from the list of pending requests. Called by IqRequest.
  void RemoveRequest(IqRequest* request);

  raw_ptr<SignalStrategy> signal_strategy_;
  IqRequestMap requests_;
};

// IqRequest instances are bound to the thread they are created on.
class IqRequest {
 public:
  IqRequest(IqSender* sender,
            IqSender::ReplyCallback callback,
            const std::string& addressee);

  IqRequest(const IqRequest&) = delete;
  IqRequest& operator=(const IqRequest&) = delete;

  ~IqRequest();

  // Sets the timeout for the request. When the timeout expires, |callback| is
  // called with a JingleMessageReply with its text set to "timeout".
  void SetTimeout(base::TimeDelta timeout);

 private:
  friend class IqSender;

  void CallCallback(const JingleMessageReply& reply);
  void OnTimeout();

  // Called by IqSender when a response is received.
  void OnResponse(const JingleMessageReply& reply);

  void DeliverResponse(const JingleMessageReply& reply);

  raw_ptr<IqSender> sender_;
  IqSender::ReplyCallback callback_;
  std::string addressee_;

  base::WeakPtrFactory<IqRequest> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_IQ_SENDER_H_
