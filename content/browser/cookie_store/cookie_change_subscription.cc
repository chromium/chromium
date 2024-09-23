// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_store/cookie_change_subscription.h"

#include <utility>

#include "content/browser/cookie_store/cookie_change_subscriptions.pb.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

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
  NOTREACHED_IN_MIGRATION();
  return proto::CookieMatchType::EQUALS;
}

network::mojom::CookieMatchType CookieMatchTypeFromProto(
    proto::CookieMatchType match_type_proto) {
  switch (match_type_proto) {
    case proto::CookieMatchType::EQUALS:
      return network::mojom::CookieMatchType::EQUALS;
    case proto::CookieMatchType::STARTS_WITH:
      return ::network::mojom::CookieMatchType::STARTS_WITH;
    default:
      // The on-disk value was corrupted.
      return network::mojom::CookieMatchType::EQUALS;
  }
}

}  // namespace

// static
std::vector<std::unique_ptr<CookieChangeSubscription>>
CookieChangeSubscription::DeserializeVector(
    const std::string& proto_string,
    int64_t service_worker_registration_id) {
  std::vector<std::unique_ptr<CookieChangeSubscription>> subscriptions;

  proto::CookieChangeSubscriptionsProto subscriptions_proto;
  if (!subscriptions_proto.ParseFromString(proto_string))
    return subscriptions;

  int subscription_count = subscriptions_proto.subscriptions_size();
  subscriptions.reserve(subscription_count);
  for (int i = 0; i < subscription_count; ++i) {
    std::unique_ptr<CookieChangeSubscription> subscription =
        CookieChangeSubscription::Create(subscriptions_proto.subscriptions(i),
                                         service_worker_registration_id);
    if (!subscription)
      continue;
    subscriptions.push_back(std::move(subscription));
  }

  return subscriptions;
}

// static
std::string CookieChangeSubscription::SerializeVector(
    const std::vector<std::unique_ptr<CookieChangeSubscription>>&
        subscriptions) {
  DCHECK(!subscriptions.empty());
  proto::CookieChangeSubscriptionsProto subscriptions_proto;
  for (const auto& subscription : subscriptions)
    subscription->Serialize(subscriptions_proto.add_subscriptions());
  return subscriptions_proto.SerializeAsString();
}

// static
std::vector<blink::mojom::CookieChangeSubscriptionPtr>
CookieChangeSubscription::ToMojoVector(
    const std::vector<std::unique_ptr<CookieChangeSubscription>>&
        subscriptions) {
  std::vector<blink::mojom::CookieChangeSubscriptionPtr> mojo_subscriptions;
  mojo_subscriptions.reserve(subscriptions.size());
  for (const auto& subscription : subscriptions) {
    auto mojo_subscription = blink::mojom::CookieChangeSubscription::New();
    subscription->Serialize(mojo_subscription.get());
    mojo_subscriptions.push_back(std::move(mojo_subscription));
  }
  return mojo_subscriptions;
}

// static
std::unique_ptr<CookieChangeSubscription> CookieChangeSubscription::Create(
    proto::CookieChangeSubscriptionProto proto,
    int64_t service_worker_registration_id) {
  GURL url(proto.url());
  if (!url.is_valid())
    return nullptr;

  std::string name = proto.name();
  ::network::mojom::CookieMatchType match_type =
      CookieMatchTypeFromProto(proto.match_type());

  return std::make_unique<CookieChangeSubscription>(
      std::move(url), std::move(name), match_type,
      service_worker_registration_id);
}

CookieChangeSubscription::~CookieChangeSubscription() = default;

CookieChangeSubscription::CookieChangeSubscription(
    blink::mojom::CookieChangeSubscriptionPtr mojo_subscription,
    int64_t service_worker_registration_id)
    : url_(std::move(mojo_subscription->url)),
      name_(std::move(mojo_subscription->name)),
      match_type_(mojo_subscription->match_type),
      service_worker_registration_id_(service_worker_registration_id) {}

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

  // We assume that this is a same-site context.
  net::CookieOptions net_options;
  net_options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  return cookie
      .IncludeForRequestURL(url_, net_options,
                            net::CookieAccessParams{
                                access_semantics,
                                network::IsUrlPotentiallyTrustworthy(url_),
                            })
      .status.IsInclude();
}

bool operator==(const CookieChangeSubscription& lhs,
                const CookieChangeSubscription& rhs) {
  return (lhs.match_type() == rhs.match_type()) && (lhs.name() == rhs.name()) &&
         (lhs.url() == rhs.url());
}

}  // namespace content
