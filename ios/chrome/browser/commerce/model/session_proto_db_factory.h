// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_MODEL_SESSION_PROTO_DB_FACTORY_H_
#define IOS_CHROME_BROWSER_COMMERCE_MODEL_SESSION_PROTO_DB_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "base/notreached.h"
#import "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#import "components/commerce/core/proto/parcel_tracking_db_content.pb.h"
#import "components/leveldb_proto/public/shared_proto_database_client_list.h"
#import "components/session_proto_db/session_proto_db.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace session_proto_db::internal {
const char kCommerceSubscriptionDBFolder[] = "commerce_subscription_db";
const char kParcelTrackingDBFolder[] = "parcel_tracking_db";

template <typename T>
std::unique_ptr<KeyedService> BuildSessionProtoDB(ProfileIOS* profile) {
  DCHECK(!profile->IsOffTheRecord());

  if constexpr (std::is_base_of<
                    commerce_subscription_db::CommerceSubscriptionContentProto,
                    T>::value) {
    return std::make_unique<SessionProtoDB<T>>(
        profile->GetProtoDatabaseProvider(),
        profile->GetStatePath().AppendASCII(kCommerceSubscriptionDBFolder),
        leveldb_proto::ProtoDbType::COMMERCE_SUBSCRIPTION_DATABASE,
        web::GetUIThreadTaskRunner({}));
  }

  if constexpr (std::is_base_of<parcel_tracking_db::ParcelTrackingContent,
                                T>::value) {
    return std::make_unique<SessionProtoDB<T>>(
        profile->GetProtoDatabaseProvider(),
        profile->GetStatePath().AppendASCII(kParcelTrackingDBFolder),
        leveldb_proto::ProtoDbType::COMMERCE_PARCEL_TRACKING_DATABASE,
        web::GetUIThreadTaskRunner({}));
  }

  // Must add in leveldb_proto::ProtoDbType and database directory folder
  // new protos.
  NOTREACHED() << "Provided template is not supported. To support add "
                  "unique folder in the above proto -> folder name mapping. "
                  "This check could also fail because the template is not "
                  "supported on current platform.";
}
}  // namespace session_proto_db::internal

template <typename T>
class SessionProtoDBFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static SessionProtoDBFactory<T>* GetInstance();
  static SessionProtoDB<T>* GetForProfile(ProfileIOS* profile);

  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<SessionProtoDBFactory<T>>;

  SessionProtoDBFactory();
  ~SessionProtoDBFactory() override = default;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

// static
template <typename T>
SessionProtoDB<T>* SessionProtoDBFactory<T>::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->template GetServiceForProfileAs<SessionProtoDB<T>>(
      profile, /*create=*/true);
}

template <typename T>
SessionProtoDBFactory<T>::TestingFactory
SessionProtoDBFactory<T>::GetDefaultFactory() {
  return base::BindOnce(&session_proto_db::internal::BuildSessionProtoDB<T>);
}

template <typename T>
SessionProtoDBFactory<T>::SessionProtoDBFactory()
    : ProfileKeyedServiceFactoryIOS("SessionProtoDB") {}

template <typename T>
std::unique_ptr<KeyedService> SessionProtoDBFactory<T>::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return session_proto_db::internal::BuildSessionProtoDB<T>(profile);
}

// Ensure all SessionProtoDB<T> factories are built for all values of T.
void EnsureSessionProtoDBFactoriesBuilt();

#endif  // IOS_CHROME_BROWSER_COMMERCE_MODEL_SESSION_PROTO_DB_FACTORY_H_
