// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/test/tab_context_app_interface.h"

#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "base/uuid.h"
#import "components/sync/base/data_type.h"
#import "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#import "components/sync/protocol/encrypted_tab_context_container_specifics.pb.h"
#import "components/sync/protocol/encrypted_tab_context_item_specifics.pb.h"
#import "components/sync/protocol/entity_specifics.pb.h"
#import "components/sync/protocol/sync_entity.pb.h"
#import "components/sync/test/fake_server.h"
#import "components/sync_tab_context/container_id.h"
#import "components/sync_tab_context/tab_context_sync_service.h"
#import "components/sync_tab_context/upload_outcome.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/tab_context_sync_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/sync_test_util.h"

namespace {

// Timeout for `UploadPageContext()` to complete background compression and
// encryption, pending ephemeral key fetch, and the Sync commit cycle.
constexpr base::TimeDelta kUploadTimeout = base::Seconds(30);

}  // namespace

@implementation TabContextAppInterface

+ (void)injectEphemeralContainerWithID:(NSString*)containerID
                           accessToken:(NSString*)accessToken {
  const std::string uuidStr = base::SysNSStringToUTF8(containerID);
  sync_pb::EntitySpecifics specifics;
  sync_pb::EncryptedTabContextContainerSpecifics* const containerSpecifics =
      specifics.mutable_encrypted_tab_context_container();
  containerSpecifics->set_uuid(uuidStr);
  containerSpecifics->set_container_access_token(
      base::SysNSStringToUTF8(accessToken));
  chrome_test_util::InjectFakeSyncServerEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/uuidStr,
          /*client_tag=*/uuidStr, specifics,
          /*creation_time=*/0,
          /*last_modified_time=*/0));
}

+ (NSString*)createEphemeralContainer {
  ProfileIOS* const profile = chrome_test_util::GetOriginalProfile();
  CHECK(profile);
  sync_tab_context::TabContextSyncService* const service =
      TabContextSyncServiceFactory::GetForProfile(profile);
  CHECK(service);
  const std::optional<sync_tab_context::ContainerId> containerID =
      service->CreateContainerPossiblyUnencryptedEphemeral();
  if (!containerID.has_value()) {
    return nil;
  }
  return base::SysUTF8ToNSString(containerID->value().AsLowercaseString());
}

+ (BOOL)uploadPageContext:(NSString*)pageContext
                  entryID:(NSString*)entryID
              containerID:(NSString*)containerID {
  ProfileIOS* const profile = chrome_test_util::GetOriginalProfile();
  CHECK(profile);
  sync_tab_context::TabContextSyncService* const service =
      TabContextSyncServiceFactory::GetForProfile(profile);
  CHECK(service);

  const base::Uuid uuid =
      base::Uuid::ParseLowercase(base::SysNSStringToUTF8(containerID));
  CHECK(uuid.is_valid());

  __block std::optional<sync_tab_context::UploadOutcome> outcome;
  service->UploadPageContext(
      sync_tab_context::ContainerId(uuid), base::SysNSStringToUTF8(entryID),
      base::SysNSStringToUTF8(pageContext),
      base::BindOnce(^(sync_tab_context::UploadOutcome result) {
        outcome = result;
      }));

  const bool finished =
      base::test::ios::WaitUntilConditionOrTimeout(kUploadTimeout, ^bool {
        return outcome.has_value();
      });
  return finished && outcome == sync_tab_context::UploadOutcome::kSucceeded;
}

+ (NSData*)encryptedContentForServerItemWithContainerID:(NSString*)containerID
                                                entryID:(NSString*)entryID {
  fake_server::FakeServer* const fakeServer =
      chrome_test_util::GetFakeSyncServer();
  CHECK(fakeServer);
  const std::string expectedContainerID = base::SysNSStringToUTF8(containerID);
  const std::string expectedEntryID = base::SysNSStringToUTF8(entryID);

  const std::vector<sync_pb::SyncEntity> entities =
      fakeServer->GetSyncEntitiesByDataType(syncer::ENCRYPTED_TAB_CONTEXT_ITEM);
  for (const sync_pb::SyncEntity& entity : entities) {
    const sync_pb::EncryptedTabContextItemSpecifics& specifics =
        entity.specifics().encrypted_tab_context_item();
    if (specifics.container_id() == expectedContainerID &&
        specifics.item_id() == expectedEntryID &&
        specifics.has_encrypted_content()) {
      const std::string serialized =
          specifics.encrypted_content().SerializeAsString();
      return [NSData dataWithBytes:serialized.data() length:serialized.size()];
    }
  }
  return nil;
}

@end
