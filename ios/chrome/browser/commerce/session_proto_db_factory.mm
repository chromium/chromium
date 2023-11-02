// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/session_proto_db_factory.h"

#import "base/no_destructor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SessionProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>*
GetCommerceSubscriptionSessionProtoDBFactory() {
  static base::NoDestructor<SessionProtoDBFactory<
      commerce_subscription_db::CommerceSubscriptionContentProto>>
      instance;
  return instance.get();
}

template <>
SessionProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>*
SessionProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>::GetInstance() {
  return GetCommerceSubscriptionSessionProtoDBFactory();
}
