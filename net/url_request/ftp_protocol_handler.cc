// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/ftp_protocol_handler.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/ftp/ftp_auth_cache.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_ftp_job.h"
#include "url/gurl.h"

namespace net {

std::unique_ptr<FtpProtocolHandler> FtpProtocolHandler::Create(
    HostResolver* host_resolver,
    FtpAuthCache* auth_cache) {
  DCHECK(auth_cache);
  return base::WrapUnique(new FtpProtocolHandler(
      base::WrapUnique(new FtpNetworkLayer(host_resolver)), auth_cache));
}

std::unique_ptr<FtpProtocolHandler> FtpProtocolHandler::CreateForTesting(
    std::unique_ptr<FtpTransactionFactory> ftp_transaction_factory,
    FtpAuthCache* auth_cache) {
  return base::WrapUnique(
      new FtpProtocolHandler(std::move(ftp_transaction_factory), auth_cache));
}

FtpProtocolHandler::~FtpProtocolHandler() = default;

URLRequestJob* FtpProtocolHandler::MaybeCreateJob(
    URLRequest* request, NetworkDelegate* network_delegate) const {
  DCHECK_EQ("ftp", request->url().scheme());

  if (!IsPortAllowedForScheme(request->url().EffectiveIntPort(),
                              request->url().scheme_piece())) {
    return new URLRequestErrorJob(request, network_delegate, ERR_UNSAFE_PORT);
  }

  return new URLRequestFtpJob(request, network_delegate,
                              ftp_transaction_factory_.get(), ftp_auth_cache_);
}

FtpProtocolHandler::FtpProtocolHandler(
    std::unique_ptr<FtpTransactionFactory> ftp_transaction_factory,
    FtpAuthCache* auth_cache)
    : ftp_transaction_factory_(std::move(ftp_transaction_factory)),
      ftp_auth_cache_(auth_cache) {
  DCHECK(ftp_transaction_factory_);
}

}  // namespace net
