// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_store/cookie_change_subscription.h"

#include <utility>

#include "content/browser/cookie_store/cookie_change_subscriptions.pb.h"

namespace content {

namespace {

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

STATIC_ASSERT_ENUM(network::mojom::CookieMatchType::EQUALS,
                   proto::CookieMatchType::EQUALS);
STATIC_ASSERT_ENUM(network::mojom::CookieMatchType::STARTS_WITH,
                   proto::CookieMatchType::STARTS_WITH);

proto::CookieMatchType CookieMatchTypeToProto(
    network::mojom::CookieMatchType match_type) {
  switch (match_type) {
    case network::mojom::CookieMatchType::EQUALS:
      return proto::CookieMatchType::EQUALS;
    case ::network::mojom::CookieMatchType::STARTS_WITH:
      return proto::CookieMatchType::STARTS_WITH;
  }
  NOTREACHED();
  return proto::CookieMatchType::EQUALS;
}

network::mojom::CookieMatchType CookieMatchTypeFromProto(
    proto::CookieMatchType match_type_proto) {
  switch (match_type_proto) {
    case proto::CookieMatchType::EQUALS:
      return network::mojom::CookieMatchType::EQUALS;
    case proto::CookieMatchType::STARTS_WITH:
      return ::network::mojom::CookieMatchType::STARTS_WITH;
  }
  NOTREACHED();
  return network::mojom::CookieMatchType::EQUALS;
}

}  // namespace

// static
base::Optional<std::vector<CookieChangeSubscription>>
CookieChangeSubscription::DeserializeVector(
    const std::string& proto_string,
    int64_t service_worker_registration_id) {
  proto::CookieChangeSubscriptionsProto subscriptions_proto;
  if (!subscriptions_proto.ParseFromString(proto_string))
    return base::nullopt;

  std::vector<CookieChangeSubscription> subscriptions;
  int subscription_count = subscriptions_proto.subscriptions_size();
  subscriptions.reserve(subscription_count);
  for (int i = 0; i < subscription_count; ++i) {
    base::Optional<CookieChangeSubscription> subscription_opt =
        CookieChangeSubscription::Create(subscriptions_proto.subscriptions(i),
                                         service_worker_registration_id);
    if (!subscription_opt.has_value())
      continue;
    subscriptions.emplace_back(std::move(subscription_opt).value());
  }

  return base::make_optional(
      std::vector<CookieChangeSubscription>(std::move(subscriptions)));
}

// static
std::vector<CookieChangeSubscription> CookieChangeSubscription::FromMojoVector(
    std::vector<blink::mojom::CookieChangeSubscriptionPtr> mojo_subscriptions,
    int64_t service_worker_registration_id) {
  std::vector<CookieChangeSubscription> subscriptions;
  subscriptions.reserve(mojo_subscriptions.size());
  for (const auto& mojo_subscription : mojo_subscriptions) {
    subscriptions.emplace_back(
        std::move(mojo_subscription->url), std::move(mojo_subscription->name),
        mojo_subscription->match_type, service_worker_registration_id);
  }
  return subscriptions;
}

// static
std::string CookieChangeSubscription::SerializeVector(
    const std::vector<CookieChangeSubscription>& subscriptions) {
  proto::CookieChangeSubscriptionsProto subscriptions_proto;
  for (const auto& subscription : subscriptions)
    subscription.Serialize(subscriptions_proto.add_subscriptions());
  return subscriptions_proto.SerializeAsString();
}

// static
std::vector<blink::mojom::CookieChangeSubscriptionPtr>
CookieChangeSubscription::ToMojoVector(
    const std::vector<CookieChangeSubscription>& subscriptions) {
  std::vector<blink::mojom::CookieChangeSubscriptionPtr> mojo_subscriptions;
  mojo_subscriptions.reserve(subscriptions.size());
  for (const auto& subscription : subscriptions) {
    auto mojo_subscription = blink::mojom::CookieChangeSubscription::New();
    subscription.Serialize(mojo_subscription.get());
    mojo_subscriptions.emplace_back(std::move(mojo_subscription));
  }
  return mojo_subscriptions;
}

// static
base::Optional<CookieChangeSubscription> CookieChangeSubscription::Create(
    proto::CookieChangeSubscriptionProto proto,
    int64_t service_worker_registration_id) {
  if (!proto.has_url())
    return base::nullopt;
  GURL url = GURL(proto.url());
  if (!url.is_valid())
    return base::nullopt;

  std::string name = proto.has_name() ? proto.name() : "";
  ::network::mojom::CookieMatchType match_type =
      proto.has_match_type() ? CookieMatchTypeFromProto(proto.match_type())
                             : ::network::mojom::CookieMatchType::EQUALS;

  return CookieChangeSubscription(std::move(url), std::move(name), match_type,
                                  service_worker_registration_id);
}

CookieChangeSubscription::CookieChangeSubscription(CookieChangeSubscription&&) =
    default;

CookieChangeSubscription::~CookieChangeSubscription() = default;

CookieChangeSubscription::CookieChangeSubscription(
    GURL url,
    std::string name,
    ::network::mojom::CookieMatchType match_type,
    int64_t service_worker_registration_id)
    : url_(std::move(url)),
      name_(std::move(name)),
      match_type_(match_type),
      service_worker_registration_id_(service_worker_registration_id) {}

void CookieChangeSubscription::Serialize(
    proto::CookieChangeSubscriptionProto* proto) const {
  proto->set_match_type(CookieMatchTypeToProto(match_type_));
  proto->set_name(name_);
  proto->set_url(url_.spec());
}

void CookieChangeSubscription::Serialize(
    blink::mojom::CookieChangeSubscription* mojo_subscription) const {
  mojo_subscription->url = url_;
  mojo_subscription->name = name_;
  mojo_subscription->match_type = match_type_;
}

bool CookieChangeSubscription::ShouldObserveChangeTo(
    const net::CanonicalCookie& cookie,
    net::CookieAccessSemantics access_semantics) const {
  switch (match_type_) {
    case ::network::mojom::CookieMatchType::EQUALS:
      if (cookie.Name() != name_)
        return false;
      break;
    case ::network::mojom::CookieMatchType::STARTS_WITH:
      if (!base::StartsWith(cookie.Name(), name_, base::CompareCase::SENSITIVE))
        return false;
      break;
  }

  net::CookieOptions net_options;
  net_options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);

  return cookie.IncludeForRequestURL(url_, net_options, access_semantics)
      .IsInclude();
}

}  // namespace content
