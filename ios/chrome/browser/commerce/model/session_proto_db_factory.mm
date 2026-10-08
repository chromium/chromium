// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/commerce/model/session_proto_db_factory.h"

void EnsureSessionProtoDBFactoriesBuilt() {
  SessionProtoDBFactory<commerce_subscription_db::
                            CommerceSubscriptionContentProto>::GetInstance();
  SessionProtoDBFactory<
      parcel_tracking_db::ParcelTrackingContent>::GetInstance();
}
