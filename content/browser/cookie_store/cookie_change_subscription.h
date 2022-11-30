// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOKIE_STORE_COOKIE_CHANGE_SUBSCRIPTION_H_
#define CONTENT_BROWSER_COOKIE_STORE_COOKIE_CHANGE_SUBSCRIPTION_H_

#include <memory>
#include <string>

#include "base/containers/linked_list.h"
#include "third_party/blink/public/mojom/cookie_store/cookie_store.mojom.h"
#include "url/gurl.h"

namespace net {
class CanonicalCookie;
enum class CookieAccessSemantics;
}  // namespace net

namespace content {

namespace proto {

class CookieChangeSubscriptionProto;

}  // namespace proto

// Represents a single subscription to the list of cookies sent to a URL.
//
// The included linked list node and service worker registration ID are used by
// CookieStoreManager. They are ignored when comparing instances.
class CookieChangeSubscription
    : public base::LinkNode<CookieChangeSubscription> {
 public:
  // Used to read a service worker's subscriptions from the persistent store.
  //
  // Returns an empty vector if deserialization failed.
  static std::vector<std::unique_ptr<CookieChangeSubscription>>
  DeserializeVector(const std::string& proto_string,
                    int64_t service_worker_registration_id);

  // Used to write a service worker's subscriptions to the service worker store.
  //
  // The subscriptions vector should not be empty. If a registration does not
  // have subscriptions, it should not be serialized.
  //
  // Returns the empty string in case of a serialization error.
  static std::string SerializeVector(
      const std::vector<std::unique_ptr<CookieChangeSubscription>>&
          subscriptions);

  // Converts a service worker's subscriptions to a Mojo API call result.
  static std::vector<blink::mojom::CookieChangeSubscriptionPtr> ToMojoVector(
      const std::vector<std::unique_ptr<CookieChangeSubscription>>&
          subscription);

  // Public for testing.
  //
  // Production code should use the vector-based factory methods above.
  //
  // Returns null in case of deserialization error.
  static std::unique_ptr<CookieChangeSubscription> Create(
      proto::CookieChangeSubscriptionProto proto,
      int64_t service_worker_registration_id);

  // Converts a subscription from a Mojo API call.
  CookieChangeSubscription(
      blink::mojom::CookieChangeSubscriptionPtr mojo_subscription,
      int64_t service_worker_registration_id);

  // Public for testing.
  //
  // Production code should use the vector-based factory methods above.
  CookieChangeSubscription(GURL url,
                           std::string name,
                           ::network::mojom::CookieMatchType match_type,
                           int64_t service_worker_registration_id);

  CookieChangeSubscription(const CookieChangeSubscription&) = delete;
  CookieChangeSubscription& operator=(const CookieChangeSubscription&) = delete;

  ~CookieChangeSubscription();

  // The URL whose cookie list is watched for changes.
  const GURL& url() const { return url_; }

  // Operator for name-based matching.
  //
  // This is used to implement both equality and prefix-based name matching.
  // Supporting the latter helps avoid wasting battery by waking up service
  // workers unnecessarily.
  ::network::mojom::CookieMatchType match_type() const { return match_type_; }

  // Operand for the name-based matching operator above.
  //
  // For EQUAL matching, the cookie name must precisely match name(). For
  // STARTS_WITH matching, the cookie name must be prefixed by name().
  const std::string& name() const { return name_; }

  // The service worker registration that this subscription belongs to.
  int64_t service_worker_registration_id() const {
    return service_worker_registration_id_;
  }

  // Writes the subscription to the given protobuf.
  void Serialize(proto::CookieChangeSubscriptionProto* proto) const;
  // Writes the subscription to the given Mojo object.
  void Serialize(
      blink::mojom::CookieChangeSubscription* mojo_subscription) const;

  // True if the subscription covers a change to the given cookie.
  bool ShouldObserveChangeTo(const net::CanonicalCookie& cookie,
                             net::CookieAccessSemantics access_semantics) const;

 private:
  const GURL url_;
  const std::string name_;
  const ::network::mojom::CookieMatchType match_type_;
  const int64_t service_worker_registration_id_;
};

// Used to deduplicate equivalent subscriptons.
//
// Ignores the service worker registration ID and the linked list node.
bool operator==(const CookieChangeSubscription& lhs,
                const CookieChangeSubscription& rhs);

}  // namespace content

#endif  // CONTENT_BROWSER_COOKIE_STORE_COOKIE_CHANGE_SUBSCRIPTION_H_
