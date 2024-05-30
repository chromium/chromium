// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/session_proto_db_factory.h"

#import "base/no_destructor.h"

template <>
SessionProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>*
SessionProtoDBFactory<
    commerce_subscription_db::CommerceSubscriptionContentProto>::GetInstance() {
  static base::NoDestructor<SessionProtoDBFactory<
      commerce_subscription_db::CommerceSubscriptionContentProto>>
      instance;
  return instance.get();
}

template <>
SessionProtoDBFactory<parcel_tracking_db::ParcelTrackingContent>*
SessionProtoDBFactory<
    parcel_tracking_db::ParcelTrackingContent>::GetInstance() {
  static base::NoDestructor<
      SessionProtoDBFactory<parcel_tracking_db::ParcelTrackingContent>>
      instance;
  return instance.get();
}

void EnsureSessionProtoDBFactoriesBuilt() {
  SessionProtoDBFactory<commerce_subscription_db::
                            CommerceSubscriptionContentProto>::GetInstance();
  SessionProtoDBFactory<
      parcel_tracking_db::ParcelTrackingContent>::GetInstance();
}
