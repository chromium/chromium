// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/instance_identity_token_getter.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "remoting/base/http_status.h"
#include "remoting/base/logging.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {
// Returns the 'payload' of the ID Token as a Dict if the token is well-formed,
// otherwise returns nullopt.
std::optional<base::Value::Dict> parseIdTokenPayload(std::string_view token) {
  // ID Tokens are comprised of three parts separated by periods:
  //   1) Base64 encoded JSON header
  //   2) Base64 encoded JSON payload
  //   3) Signature bytes
  auto parts =
      base::SplitString(token, ".", base::WhitespaceHandling::KEEP_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);
  // Return early if the token is malformed or missing the signature.
  if (parts.size() != 3) {
    LOG(WARNING) << "Invalid instance identity token: " << token;
    return std::nullopt;
  }
  // Parse and validate the header.
  auto encoded_header = parts[0];
  std::string decoded_header;
  if (!base::Base64Decode(encoded_header, &decoded_header,
                          base::Base64DecodePolicy::kForgiving)) {
    LOG(WARNING) << "Failed to decode instance identity token header: "
                 << encoded_header;
    return std::nullopt;
  }
  auto header = base::JSONReader::ReadDict(decoded_header);
  if (!header.has_value()) {
    LOG(WARNING) << "Failed to parse instance identity token header: "
                 << decoded_header;
    return std::nullopt;
  }
  if (!header->contains("kid") || !header->contains("alg") ||
      !header->contains("typ")) {
    LOG(WARNING) << "Invalid instance identity token header: " << *header;
    return std::nullopt;
  }

  // Parse and validate the payload.
  auto encoded_payload = parts[1];
  std::string decoded_payload;
  if (!base::Base64Decode(encoded_payload, &decoded_payload,
                          base::Base64DecodePolicy::kForgiving)) {
    LOG(WARNING) << "Failed to decode instance identity token payload: "
                 << encoded_payload;
    return std::nullopt;
  }
  auto payload = base::JSONReader::ReadDict(decoded_payload);
  if (!payload.has_value()) {
    LOG(WARNING) << "Failed to parse instance identity token payload: "
                 << decoded_payload;
    return std::nullopt;
  }
  if (!payload->contains("iss") || !payload->contains("aud") ||
      !payload->contains("iat") || !payload->contains("exp") ||
      !payload->contains("azp") || !payload->contains("google")) {
    LOG(WARNING) << "Invalid instance identity token payload: " << *payload;
    return std::nullopt;
  }
  auto* google_payload = payload->FindDict("google");
  auto* compute_engine_payload =
      google_payload ? google_payload->FindDict("compute_engine") : nullptr;
  if (compute_engine_payload == nullptr ||
      !compute_engine_payload->contains("instance_id") ||
      !compute_engine_payload->contains("instance_name") ||
      !compute_engine_payload->contains("project_id") ||
      !compute_engine_payload->contains("project_number") ||
      !compute_engine_payload->contains("zone")) {
    LOG(WARNING) << "Invalid instance identity token payload: " << *payload;
    return std::nullopt;
  }

  HOST_LOG << "Retrieved instance identity token: " << *header << ":"
           << *payload;

  return payload;
}

}  // namespace

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
    auto response_body = response.response_body();
    auto payload = parseIdTokenPayload(response_body);
    // If the payload can be retrieved then the token is correctly formatted.
    if (payload.has_value()) {
      identity_token_ = std::move(response_body);
      auto token_exp_value = payload->FindInt("exp");
      auto now = base::Time::Now();
      if (token_exp_value.has_value()) {
        token_expiration_time_ =
            base::Time::FromSecondsSinceUnixEpoch(*token_exp_value);
        HOST_LOG << "Instance identity token expires at: "
                 << token_expiration_time_;
        LOG_IF(WARNING, token_expiration_time_ < now + base::Minutes(30) ||
                            token_expiration_time_ > now + base::Minutes(90))
            << "Token expiration is outside of the expected lifetime window "
            << "which is ~60 minutes.";
      } else {
        LOG(WARNING) << "Token payload missing valid 'exp' integer value which "
                     << "may mean other fields are also invalid: " << *payload;
        token_expiration_time_ = now;
      }
    }
  }

  auto callbacks = std::move(queued_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(identity_token_);
  }
}

}  // namespace remoting
