// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/token_validator_factory_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringize_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "crypto/random.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/token_validator_base.h"
#include "url/gurl.h"

namespace {

// Length in bytes of the cryptographic nonce used to salt the token scope.
const size_t kNonceLength = 16;  // 128 bits.

}  // namespace

namespace remoting {

class TokenValidatorImpl : public TokenValidatorBase {
 public:
  TokenValidatorImpl(
      const ThirdPartyAuthConfig& third_party_auth_config,
      scoped_refptr<RsaKeyPair> key_pair,
      const std::string& local_jid,
      const std::string& remote_jid,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);

  TokenValidatorImpl(const TokenValidatorImpl&) = delete;
  TokenValidatorImpl& operator=(const TokenValidatorImpl&) = delete;

 protected:
  void StartValidateRequest(const std::string& token) override;

 private:
  static std::string CreateScope(const std::string& local_jid,
                                 const std::string& remote_jid);

  std::string post_body_;
  scoped_refptr<RsaKeyPair> key_pair_;
};

TokenValidatorImpl::TokenValidatorImpl(
    const ThirdPartyAuthConfig& third_party_auth_config,
    scoped_refptr<RsaKeyPair> key_pair,
    const std::string& local_jid,
    const std::string& remote_jid,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : TokenValidatorBase(third_party_auth_config,
                         CreateScope(local_jid, remote_jid),
                         request_context_getter),
      key_pair_(key_pair) {
  DCHECK(key_pair_.get());
}

// TokenValidator interface.
void TokenValidatorImpl::StartValidateRequest(const std::string& token) {
  post_body_ = "code=" + base::EscapeUrlEncodedData(token, true) +
               "&client_id=" +
               base::EscapeUrlEncodedData(key_pair_->GetPublicKey(), true) +
               "&client_secret=" +
               base::EscapeUrlEncodedData(key_pair_->SignMessage(token), true) +
               "&grant_type=authorization_code";

  request_ = request_context_getter_->GetURLRequestContext()->CreateRequest(
      third_party_auth_config_.token_validation_url, net::DEFAULT_PRIORITY,
      this, MISSING_TRAFFIC_ANNOTATION);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string app_name = "Chrome Remote Desktop";
#else
  std::string app_name = "Chromoting";
#endif
#ifndef VERSION
#error VERSION is not set.
#endif
  // Set a user-agent for logging/auditing purposes.
  request_->SetExtraRequestHeaderByName(
      net::HttpRequestHeaders::kUserAgent,
      app_name + " " + STRINGIZE(VERSION), true);

  request_->SetExtraRequestHeaderByName(net::HttpRequestHeaders::kContentType,
                                        "application/x-www-form-urlencoded",
                                        true);
  request_->set_method("POST");
  std::unique_ptr<net::UploadElementReader> reader(
      new net::UploadBytesElementReader(post_body_.data(), post_body_.size()));
  request_->set_upload(
      net::ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));
  request_->Start();
}

std::string TokenValidatorImpl::CreateScope(const std::string& local_jid,
                                            const std::string& remote_jid) {
  std::string nonce_bytes;
  crypto::RandBytes(base::WriteInto(&nonce_bytes, kNonceLength + 1),
                    kNonceLength);
  std::string nonce;
  base::Base64Encode(nonce_bytes, &nonce);
  // Note that because of how FTL signaling IDs are managed, |local_jid| will
  // not change between connections to a given host instance. We do expect that
  // |remote_jid| will be different for each connection (clients should not
  // reuse the same channel for connections) but the host does not control this.
  // Since at least one of the JIDs will be reused between connections, we rely
  // on the nonce to guarantee that the scope string is unique and cannot be
  // reused for multiple connections.
  return "client:" + remote_jid + " host:" + local_jid + " nonce:" + nonce;
}

TokenValidatorFactoryImpl::TokenValidatorFactoryImpl(
    const ThirdPartyAuthConfig& third_party_auth_config,
    scoped_refptr<RsaKeyPair> key_pair,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : third_party_auth_config_(third_party_auth_config),
      key_pair_(key_pair),
      request_context_getter_(request_context_getter) {}

TokenValidatorFactoryImpl::~TokenValidatorFactoryImpl() = default;

std::unique_ptr<protocol::TokenValidator>
TokenValidatorFactoryImpl::CreateTokenValidator(const std::string& local_jid,
                                                const std::string& remote_jid) {
  return std::make_unique<TokenValidatorImpl>(third_party_auth_config_,
                                              key_pair_, local_jid, remote_jid,
                                              request_context_getter_);
}

}  // namespace remoting
