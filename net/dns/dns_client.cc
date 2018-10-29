// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_client.h"

#include <utility>

#include "base/bind.h"
#include "base/rand_util.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_socket_pool.h"
#include "net/dns/dns_transaction.h"
#include "net/socket/client_socket_factory.h"

namespace net {

namespace {

class DnsClientImpl : public DnsClient {
 public:
  DnsClientImpl(NetLog* net_log,
                ClientSocketFactory* socket_factory,
                const RandIntCallback& rand_int_callback)
      : address_sorter_(AddressSorter::CreateAddressSorter()),
        net_log_(net_log),
        socket_factory_(socket_factory),
        rand_int_callback_(rand_int_callback) {}

  void SetConfig(const DnsConfig& config) override {
    factory_.reset();
    session_ = nullptr;
    if (config.IsValid() && !config.unhandled_options) {
      std::unique_ptr<DnsSocketPool> socket_pool(
          config.randomize_ports
              ? DnsSocketPool::CreateDefault(socket_factory_,
                                             rand_int_callback_)
              : DnsSocketPool::CreateNull(socket_factory_, rand_int_callback_));
      session_ = new DnsSession(config, std::move(socket_pool),
                                rand_int_callback_, net_log_);
      factory_ = DnsTransactionFactory::CreateFactory(session_.get());
    }
  }

  const DnsConfig* GetConfig() const override {
    return session_.get() ? &session_->config() : NULL;
  }

  DnsTransactionFactory* GetTransactionFactory() override {
    return session_.get() ? factory_.get() : NULL;
  }

  AddressSorter* GetAddressSorter() override { return address_sorter_.get(); }

 private:
  scoped_refptr<DnsSession> session_;
  std::unique_ptr<DnsTransactionFactory> factory_;
  std::unique_ptr<AddressSorter> address_sorter_;

  NetLog* net_log_;

  ClientSocketFactory* socket_factory_;
  const RandIntCallback rand_int_callback_;

  DISALLOW_COPY_AND_ASSIGN(DnsClientImpl);
};

}  // namespace

// static
std::unique_ptr<DnsClient> DnsClient::CreateClient(NetLog* net_log) {
  return std::make_unique<DnsClientImpl>(
      net_log, ClientSocketFactory::GetDefaultFactory(),
      base::Bind(&base::RandInt));
}

// static
std::unique_ptr<DnsClient> DnsClient::CreateClientForTesting(
    NetLog* net_log,
    ClientSocketFactory* socket_factory,
    const RandIntCallback& rand_int_callback) {
  return std::make_unique<DnsClientImpl>(net_log, socket_factory,
                                         rand_int_callback);
}

}  // namespace net
