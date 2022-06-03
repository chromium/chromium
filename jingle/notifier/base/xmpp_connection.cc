// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/xmpp_connection.h"

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jingle/glue/network_service_async_socket.h"
#include "jingle/glue/task_pump.h"
#include "jingle/notifier/base/weak_xmpp_client.h"
#include "net/socket/client_socket_factory.h"
#include "net/ssl/ssl_config_service.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "third_party/libjingle_xmpp/xmpp/xmppclientsettings.h"

namespace notifier {

XmppConnection::Delegate::~Delegate() {}

namespace {

jingle_xmpp::AsyncSocket* CreateSocket(
    const jingle_xmpp::XmppClientSettings& xmpp_client_settings,
    jingle_glue::GetProxyResolvingSocketFactoryCallback
        get_socket_factory_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  bool use_fake_ssl_client_socket =
      (xmpp_client_settings.protocol() == jingle_xmpp::PROTO_SSLTCP);
  // The default SSLConfig is good enough for us for now.
  const net::SSLConfig ssl_config;
  // These numbers were taken from similar numbers in
  // XmppSocketAdapter.
  const size_t kReadBufSize = 64U * 1024U;
  const size_t kWriteBufSize = 64U * 1024U;
  return new jingle_glue::NetworkServiceAsyncSocket(
      get_socket_factory_callback, use_fake_ssl_client_socket, kReadBufSize,
      kWriteBufSize, traffic_annotation);
}

}  // namespace

XmppConnection::XmppConnection(
    const jingle_xmpp::XmppClientSettings& xmpp_client_settings,
    jingle_glue::GetProxyResolvingSocketFactoryCallback
        get_socket_factory_callback,
    Delegate* delegate,
    jingle_xmpp::PreXmppAuth* pre_xmpp_auth,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : task_pump_(new jingle_glue::TaskPump()),
      on_connect_called_(false),
      delegate_(delegate) {
  DCHECK(delegate_);
  // Owned by |task_pump_|, but is guaranteed to live at least as long
  // as this function.
  WeakXmppClient* weak_xmpp_client = new WeakXmppClient(task_pump_.get());
  weak_xmpp_client->SignalStateChange.connect(
      this, &XmppConnection::OnStateChange);
  weak_xmpp_client->SignalLogInput.connect(
      this, &XmppConnection::OnInputLog);
  weak_xmpp_client->SignalLogOutput.connect(
      this, &XmppConnection::OnOutputLog);
  const char kLanguage[] = "en";
  jingle_xmpp::XmppReturnStatus connect_status = weak_xmpp_client->Connect(
      xmpp_client_settings, kLanguage,
      CreateSocket(xmpp_client_settings, get_socket_factory_callback,
                   traffic_annotation),
      pre_xmpp_auth);
  // jingle_xmpp::XmppClient::Connect() should never fail.
  DCHECK_EQ(connect_status, jingle_xmpp::XMPP_RETURN_OK);
  weak_xmpp_client->Start();
  weak_xmpp_client_ = weak_xmpp_client->AsWeakPtr();
}

XmppConnection::~XmppConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearClient();
  task_pump_->Stop();
  // We do this because XmppConnection may get destroyed as a result
  // of a signal from XmppClient.  If we delete |task_pump_| here, bad
  // things happen when the stack pops back up to the XmppClient's
  // (which is deleted by |task_pump_|) function.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                  std::move(task_pump_));
}

void XmppConnection::OnStateChange(jingle_xmpp::XmppEngine::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "XmppClient state changed to " << state;
  if (!weak_xmpp_client_.get()) {
    LOG(DFATAL) << "weak_xmpp_client_ unexpectedly NULL";
    return;
  }
  if (!delegate_) {
    LOG(DFATAL) << "delegate_ unexpectedly NULL";
    return;
  }
  switch (state) {
    case jingle_xmpp::XmppEngine::STATE_OPEN:
      if (on_connect_called_) {
        LOG(DFATAL) << "State changed to STATE_OPEN more than once";
      } else {
        delegate_->OnConnect(weak_xmpp_client_);
        on_connect_called_ = true;
      }
      break;
    case jingle_xmpp::XmppEngine::STATE_CLOSED: {
      int subcode = 0;
      jingle_xmpp::XmppEngine::Error error =
          weak_xmpp_client_->GetError(&subcode);
      const jingle_xmpp::XmlElement* stream_error =
          weak_xmpp_client_->GetStreamError();
      ClearClient();
      Delegate* delegate = delegate_;
      delegate_ = nullptr;
      delegate->OnError(error, subcode, stream_error);
      break;
    }
    default:
      // Do nothing.
      break;
  }
}

void XmppConnection::OnInputLog(const char* data, int len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "XMPP Input: " << base::StringPiece(data, len);
}

void XmppConnection::OnOutputLog(const char* data, int len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "XMPP Output: " << base::StringPiece(data, len);
}

void XmppConnection::ClearClient() {
  if (weak_xmpp_client_.get()) {
    weak_xmpp_client_->Invalidate();
    DCHECK(!weak_xmpp_client_.get());
  }
}

}  // namespace notifier
