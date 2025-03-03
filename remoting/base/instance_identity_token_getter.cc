// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/instance_identity_token_getter.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "remoting/base/http_status.h"
#include "remoting/base/instance_identity_token.h"
#include "remoting/base/logging.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

InstanceIdentityTokenGetter::InstanceIdentityTokenGetter(
    std::string_view audience,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : audience_(audience), compute_engine_service_client_(url_loader_factory) {}

InstanceIdentityTokenGetter::~InstanceIdentityTokenGetter() = default;

void InstanceIdentityTokenGetter::RetrieveToken(TokenCallback on_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check expiration, clear token if no longer valid. Reduce the expiration by
  // a small amount to ensure the token provided will remain valid in cases
  // where its usage may be delayed a bit.
  if ((token_expiration_time_ - base::Minutes(5)) < base::Time::Now()) {
    identity_token_.clear();
  }

  if (!identity_token_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_token), identity_token_));
    return;
  }

  queued_callbacks_.emplace_back(std::move(on_token));
  // Only make a service request for the first caller, the rest will be queued
  // and provided with the token when the request completes.
  if (queued_callbacks_.size() == 1) {
    compute_engine_service_client_.GetInstanceIdentityToken(
        audience_,
        base::BindOnce(&InstanceIdentityTokenGetter::OnTokenRetrieved,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void InstanceIdentityTokenGetter::OnTokenRetrieved(const HttpStatus& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (response.ok()) {
    auto jwt = response.response_body();
    auto validated_token = InstanceIdentityToken::Create(jwt);
    if (validated_token.has_value()) {
      HOST_LOG << "Retrieved fresh instance identity token";
      if (identity_token_.empty()) {
        // Log the token retrieved if this is the first successful fetch.
        HOST_LOG << "Instance identity token contents:\n" << *validated_token;
      }
      identity_token_ = std::move(jwt);

      auto token_exp_value = validated_token->payload().FindInt("exp");
      auto now = base::Time::Now();
      if (token_exp_value.has_value()) {
        token_expiration_time_ =
            base::Time::FromSecondsSinceUnixEpoch(*token_exp_value);
        LOG_IF(WARNING, token_expiration_time_ < now + base::Minutes(30) ||
                            token_expiration_time_ > now + base::Minutes(90))
            << "Token expiration is outside of the expected lifetime window "
            << "which is ~60 minutes.";
      } else {
        LOG(WARNING) << "Token payload missing valid 'exp' integer value which "
                     << "may mean other fields are also invalid: "
                     << validated_token->payload();
        token_expiration_time_ = now;
      }
    }
  } else {
    int error_code = static_cast<int>(response.error_code());
    LOG(WARNING) << "Failed to retrieve an Instance Identity token.\n"
                 << "  Error code: " << error_code << "\n"
                 << "  Message: " << response.error_message() << "\n"
                 << "  Body: " << response.response_body();
  }

  auto callbacks = std::move(queued_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(identity_token_);
  }
}

}  // namespace remoting
