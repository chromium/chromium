// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/instance_identity_token_getter.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "remoting/base/http_status.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {
// Per Cloud documentation, the identity token is valid for ~1 hour.
// TODO: joedow - Parse the identity token to set the exact expiration time.
constexpr base::TimeDelta kTokenLifetime = base::Minutes(50);
}  // namespace

InstanceIdentityTokenGetter::InstanceIdentityTokenGetter(
    std::string_view audience,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : audience_(audience), compute_engine_service_client_(url_loader_factory) {}

InstanceIdentityTokenGetter::~InstanceIdentityTokenGetter() = default;

void InstanceIdentityTokenGetter::RetrieveToken(TokenCallback on_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Check expiration, clear token if no longer valid.
  if ((last_fetch_time_ + kTokenLifetime) < base::Time::Now()) {
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
    identity_token_ = response.response_body();
    last_fetch_time_ = base::Time::Now();
  }

  // TODO: joedow - Add token validation and check expiration time.

  auto callbacks = std::move(queued_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(identity_token_);
  }
}

}  // namespace remoting
