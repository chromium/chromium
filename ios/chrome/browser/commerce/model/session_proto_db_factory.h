// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMMERCE_MODEL_SESSION_PROTO_DB_FACTORY_H_
#define IOS_CHROME_BROWSER_COMMERCE_MODEL_SESSION_PROTO_DB_FACTORY_H_

#include <memory>

#import "base/no_destructor.h"
#import "components/commerce/core/proto/commerce_subscription_db_content.pb.h"
#import "components/commerce/core/proto/parcel_tracking_db_content.pb.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/leveldb_proto/public/shared_proto_database_client_list.h"
#import "components/session_proto_db/session_proto_db.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace session_proto_db::internal {
const char kCommerceSubscriptionDBFolder[] = "commerce_subscription_db";
const char kParcelTrackingDBFolder[] = "parcel_tracking_db";

template <typename T>
std::unique_ptr<KeyedService> BuildSessionProtoDB(web::BrowserState* state) {
  DCHECK(!state->IsOffTheRecord());

  if (std::is_base_of<
          commerce_subscription_db::CommerceSubscriptionContentProto,
          T>::value) {
    return std::make_unique<SessionProtoDB<T>>(
        state->GetProtoDatabaseProvider(),
        state->GetStatePath().AppendASCII(kCommerceSubscriptionDBFolder),
        leveldb_proto::ProtoDbType::COMMERCE_SUBSCRIPTION_DATABASE,
        web::GetUIThreadTaskRunner({}));
  } else if (std::is_base_of<parcel_tracking_db::ParcelTrackingContent,
                             T>::value) {
    return std::make_unique<SessionProtoDB<T>>(
        state->GetProtoDatabaseProvider(),
        state->GetStatePath().AppendASCII(kParcelTrackingDBFolder),
        leveldb_proto::ProtoDbType::COMMERCE_PARCEL_TRACKING_DATABASE,
        web::GetUIThreadTaskRunner({}));
  } else {
    // Must add in leveldb_proto::ProtoDbType and database directory folder
    // new protos.
    DCHECK(false) << "Provided template is not supported. To support add "
                     "unique folder in the above proto -> folder name mapping. "
                     "This check could also fail because the template is not "
                     "supported on current platform.";
  }
}
}  // namespace session_proto_db::internal

template <typename T>
class SessionProtoDBFactory : public BrowserStateKeyedServiceFactory {
 public:
  SessionProtoDBFactory(const SessionProtoDBFactory&) = delete;
  SessionProtoDBFactory& operator=(const SessionProtoDBFactory&) = delete;

  static SessionProtoDBFactory<T>* GetInstance();
  static SessionProtoDB<T>* GetForProfile(ProfileIOS* profile);
  // Deprecated: use GetForProfile(...).
  static SessionProtoDB<T>* GetForBrowserState(ProfileIOS* profile);

  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<SessionProtoDBFactory<T>>;

  SessionProtoDBFactory();
  ~SessionProtoDBFactory() override = default;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* state) const override;
};

// static
template <typename T>
SessionProtoDB<T>* SessionProtoDBFactory<T>::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<SessionProtoDB<T>*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
template <typename T>
SessionProtoDB<T>* SessionProtoDBFactory<T>::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

template <typename T>
BrowserStateKeyedServiceFactory::TestingFactory
SessionProtoDBFactory<T>::GetDefaultFactory() {
  return base::BindRepeating(
      &session_proto_db::internal::BuildSessionProtoDB<T>);
}

template <typename T>
SessionProtoDBFactory<T>::SessionProtoDBFactory()
    : BrowserStateKeyedServiceFactory(
          "SessionProtoDB",
          BrowserStateDependencyManager::GetInstance()) {}

template <typename T>
std::unique_ptr<KeyedService> SessionProtoDBFactory<T>::BuildServiceInstanceFor(
    web::BrowserState* state) const {
  return session_proto_db::internal::BuildSessionProtoDB<T>(state);
}

// Ensure all SessionProtoDB<T> factories are built for all values of T.
void EnsureSessionProtoDBFactoriesBuilt();

#endif  // IOS_CHROME_BROWSER_COMMERCE_MODEL_SESSION_PROTO_DB_FACTORY_H_
