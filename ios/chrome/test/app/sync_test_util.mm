// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/sync_test_util.h"

#import <set>
#import <string>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "base/uuid.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/metrics/demographics/demographic_metrics_test_utils.h"
#import "components/sync/base/pref_names.h"
#import "components/sync/base/time.h"
#import "components/sync/engine/loopback_server/loopback_server_entity.h"
#import "components/sync/protocol/device_info_specifics.pb.h"
#import "components/sync/protocol/session_specifics.pb.h"
#import "components/sync/protocol/sync_enums.pb.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_impl.h"
#import "components/sync/test/entity_builder_factory.h"
#import "components/sync/test/fake_server.h"
#import "components/sync/test/fake_server_network_resources.h"
#import "components/sync/test/fake_server_nigori_helper.h"
#import "components/sync/test/fake_server_verifier.h"
#import "components/sync/test/nigori_test_utils.h"
#import "components/sync/test/sessions_hierarchy.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/sync_device_info/device_info_util.h"
#import "components/sync_device_info/local_device_info_provider.h"
#import "components/sync_sessions/session_store.h"
#import "components/sync_sessions/session_sync_test_helper.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/distant_session.h"
#import "ios/chrome/browser/synced_sessions/distant_tab.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

fake_server::FakeServer* gSyncFakeServer = nullptr;

NSString* const kSyncTestErrorDomain = @"SyncTestDomain";

// Overrides the network callback of the current SyncServiceImpl with
// `create_http_post_provider_factory_cb`.
void OverrideSyncNetwork(const syncer::CreateHttpPostProviderFactory&
                             create_http_post_provider_factory_cb) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  DCHECK(browser_state);
  syncer::SyncServiceImpl* service =
      SyncServiceFactory::GetAsSyncServiceImplForBrowserStateForTesting(
          browser_state);
  service->OverrideNetworkForTest(create_http_post_provider_factory_cb);
}

// Returns a bookmark server entity based on `title` and `url`.
std::unique_ptr<syncer::LoopbackServerEntity> CreateBookmarkServerEntity(
    const std::string& title,
    const GURL& url) {
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  return bookmark_builder.BuildBookmark(url);
}

}  // namespace

namespace chrome_test_util {

bool IsFakeSyncServerSetUp() {
  return gSyncFakeServer;
}

void SetUpFakeSyncServer() {
  DCHECK(!gSyncFakeServer);
  gSyncFakeServer = new fake_server::FakeServer();
  OverrideSyncNetwork(fake_server::CreateFakeServerHttpPostProviderFactory(
      gSyncFakeServer->AsWeakPtr()));
}

void TearDownFakeSyncServer() {
  DCHECK(gSyncFakeServer);
  delete gSyncFakeServer;
  gSyncFakeServer = nullptr;
  OverrideSyncNetwork(syncer::CreateHttpPostProviderFactory());
}

void TriggerSyncCycle(syncer::ModelType type) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state);
  sync_service->TriggerRefresh({type});
}

void ClearSyncServerData() {
  // Allow the caller to preventively clear server data.
  if (gSyncFakeServer) {
    gSyncFakeServer->ClearServerData();
  }
}

int GetNumberOfSyncEntities(syncer::ModelType type) {
  base::Value::Dict entities = gSyncFakeServer->GetEntitiesAsDictForTesting();

  base::Value::List* entity_list =
      entities.FindList(ModelTypeToDebugString(type));
  DCHECK(entity_list);
  return static_cast<int>(entity_list->size());
}

BOOL VerifyNumberOfSyncEntitiesWithName(syncer::ModelType type,
                                        std::string name,
                                        size_t count,
                                        NSError** error) {
  DCHECK(gSyncFakeServer);
  fake_server::FakeServerVerifier verifier(gSyncFakeServer);
  testing::AssertionResult result =
      verifier.VerifyEntityCountByTypeAndName(count, type, name);
  if (result != testing::AssertionSuccess() && error != nil) {
    NSDictionary* errorInfo = @{
      NSLocalizedDescriptionKey : base::SysUTF8ToNSString(result.message())
    };
    *error = [NSError errorWithDomain:kSyncTestErrorDomain
                                 code:0
                             userInfo:errorInfo];
    return NO;
  }
  return result == testing::AssertionSuccess();
}

void AddBookmarkToFakeSyncServer(std::string url, std::string title) {
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(title);
  gSyncFakeServer->InjectEntity(bookmark_builder.BuildBookmark(GURL(url)));
}

void AddLegacyBookmarkToFakeSyncServer(std::string url,
                                       std::string title,
                                       std::string originator_client_item_id) {
  DCHECK(
      !base::Uuid::ParseCaseInsensitive(originator_client_item_id).is_valid());
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::BookmarkEntityBuilder bookmark_builder =
      entity_builder_factory.NewBookmarkEntityBuilder(
          title, std::move(originator_client_item_id));
  gSyncFakeServer->InjectEntity(
      bookmark_builder
          .SetGeneration(fake_server::BookmarkEntityBuilder::
                             BookmarkGeneration::kWithoutTitleInSpecifics)
          .BuildBookmark(GURL(url)));
}

void AddSessionToFakeSyncServer(
    const synced_sessions::DistantSession& session) {
  std::vector<sync_pb::SessionSpecifics> specifics_list;
  SessionID window_id = SessionID::FromSerializedValue(1);
  // Tab specifics.
  std::vector<SessionID> tab_list;
  sync_sessions::SessionSyncTestHelper helper;
  for (const std::unique_ptr<synced_sessions::DistantTab>& distant_tab :
       session.tabs) {
    sync_pb::SessionSpecifics tab = helper.BuildTabSpecifics(
        session.tag, base::UTF16ToUTF8(distant_tab->title),
        distant_tab->virtual_url.spec(), window_id, distant_tab->tab_id);
    specifics_list.push_back(tab);
    tab_list.push_back(distant_tab->tab_id);
  }
  // Header specifics.
  sync_pb::SessionSpecifics header =
      sync_sessions::SessionSyncTestHelper::BuildHeaderSpecificsWithoutWindows(
          session.tag, session.form_factor);
  sync_sessions::SessionSyncTestHelper::AddWindowSpecifics(window_id, tab_list,
                                                           &header);
  specifics_list.push_back(header);
  // Add entities to fake server.
  for (const sync_pb::SessionSpecifics& specifics : specifics_list) {
    sync_pb::EntitySpecifics entity;
    *entity.mutable_session() = specifics;
    gSyncFakeServer->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"",
            sync_sessions::SessionStore::GetClientTag(entity.session()), entity,
            /*creation_time=*/syncer::TimeToProtoTime(session.modified_time),
            /*last_modified_time=*/
            syncer::TimeToProtoTime(session.modified_time)));
  }
}

bool IsSyncEngineInitialized() {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  DCHECK(browser_state);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browser_state);
  return syncService->IsEngineInitialized();
}

std::string GetSyncCacheGuid() {
  DCHECK(IsSyncEngineInitialized());
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  syncer::DeviceInfoSyncService* service =
      DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state);
  const syncer::LocalDeviceInfoProvider* info_provider =
      service->GetLocalDeviceInfoProvider();
  return info_provider->GetLocalDeviceInfo()->guid();
}

bool VerifySyncInvalidationFieldsPopulated() {
  DCHECK(IsFakeSyncServerSetUp());
  const std::string cache_guid = GetSyncCacheGuid();
  std::vector<sync_pb::SyncEntity> entities =
      gSyncFakeServer->GetSyncEntitiesByModelType(syncer::DEVICE_INFO);
  for (const sync_pb::SyncEntity& entity : entities) {
    if (entity.specifics().device_info().cache_guid() == cache_guid) {
      const sync_pb::InvalidationSpecificFields& invalidation_fields =
          entity.specifics().device_info().invalidation_fields();
      // TODO(crbug.com/1187481): check if `instance_id_token` is present once
      // fixed.
      return !invalidation_fields.interested_data_type_ids().empty();
    }
  }
  // The local DeviceInfo hasn't been committed yet.
  return false;
}

void AddUserDemographicsToSyncServer(
    int birth_year,
    metrics::UserDemographicsProto::Gender gender) {
  metrics::test::AddUserBirthYearAndGenderToSyncServer(
      gSyncFakeServer->AsWeakPtr(), birth_year, gender);
}

void AddAutofillProfileToFakeSyncServer(std::string guid,
                                        std::string full_name) {
  DCHECK(IsFakeSyncServerSetUp());

  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::AutofillProfileSpecifics* autofill_profile =
      entity_specifics.mutable_autofill_profile();
  autofill_profile->add_name_full(full_name);
  autofill_profile->set_guid(guid);

  std::unique_ptr<syncer::LoopbackServerEntity> entity =
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/guid, /*client_tag=*/guid, entity_specifics,
          12345, 12345);
  gSyncFakeServer->InjectEntity(std::move(entity));
}

void DeleteAutofillProfileFromFakeSyncServer(std::string guid) {
  DCHECK(IsFakeSyncServerSetUp());

  std::vector<sync_pb::SyncEntity> autofill_profiles =
      gSyncFakeServer->GetSyncEntitiesByModelType(syncer::AUTOFILL_PROFILE);
  std::string entity_id;
  std::string client_tag_hash;
  for (const sync_pb::SyncEntity& autofill_profile : autofill_profiles) {
    if (autofill_profile.specifics().autofill_profile().guid() == guid) {
      entity_id = autofill_profile.id_string();
      client_tag_hash = autofill_profile.client_tag_hash();
      break;
    }
  }
  // Delete the entity if it exists.
  if (!entity_id.empty()) {
    std::unique_ptr<syncer::LoopbackServerEntity> entity;
    entity = syncer::PersistentTombstoneEntity::CreateNew(entity_id,
                                                          client_tag_hash);
    gSyncFakeServer->InjectEntity(std::move(entity));
  }
}

bool IsAutofillProfilePresent(std::string guid, std::string full_name) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  autofill::PersonalDataManager* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browser_state);
  autofill::AutofillProfile* autofill_profile =
      personal_data_manager->GetProfileByGUID(guid);

  if (autofill_profile) {
    std::string actual_full_name =
        base::UTF16ToUTF8(autofill_profile->GetRawInfo(autofill::NAME_FULL));
    return actual_full_name == full_name;
  }
  return false;
}

void ClearAutofillProfile(std::string guid) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  autofill::PersonalDataManager* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(browser_state);
  personal_data_manager->RemoveByGUID(guid);
}

BOOL VerifySessionsOnSyncServer(const std::multiset<std::string>& expected_urls,
                                NSError** error) {
  DCHECK(gSyncFakeServer);
  fake_server::SessionsHierarchy expected_sessions;
  expected_sessions.AddWindow(expected_urls);
  fake_server::FakeServerVerifier verifier(gSyncFakeServer);
  testing::AssertionResult result = verifier.VerifySessions(expected_sessions);
  if (result != testing::AssertionSuccess() && error != nil) {
    NSDictionary* errorInfo = @{
      NSLocalizedDescriptionKey : base::SysUTF8ToNSString(result.message())
    };
    *error = [NSError errorWithDomain:kSyncTestErrorDomain
                                 code:0
                             userInfo:errorInfo];
    return NO;
  }
  return result == testing::AssertionSuccess();
}

BOOL VerifyHistoryOnSyncServer(const std::multiset<GURL>& expected_urls,
                               NSError** error) {
  DCHECK(gSyncFakeServer);
  fake_server::FakeServerVerifier verifier(gSyncFakeServer);
  testing::AssertionResult result = verifier.VerifyHistory(expected_urls);
  if (result != testing::AssertionSuccess() && error != nil) {
    NSDictionary* errorInfo = @{
      NSLocalizedDescriptionKey : base::SysUTF8ToNSString(result.message())
    };
    *error = [NSError errorWithDomain:kSyncTestErrorDomain
                                 code:0
                             userInfo:errorInfo];
    return NO;
  }
  return result == testing::AssertionSuccess();
}

void AddTypedURLToClient(const GURL& url) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  history::HistoryService* historyService =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);

  historyService->AddPage(url, base::Time::Now(), 0, 1, GURL(),
                          history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
                          history::SOURCE_BROWSED, false);
}

void AddTypedURLToFakeSyncServer(const std::string& url) {
  sync_pb::EntitySpecifics entitySpecifics;
  sync_pb::TypedUrlSpecifics* typedUrl = entitySpecifics.mutable_typed_url();
  typedUrl->set_url(url);
  typedUrl->set_title(url);
  typedUrl->add_visits(base::Time::Max().ToInternalValue());
  typedUrl->add_visit_transitions(sync_pb::SyncEnums::TYPED);

  std::unique_ptr<syncer::LoopbackServerEntity> entity =
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/std::string(), /*client_tag=*/url,
          entitySpecifics, 12345, 12345);
  gSyncFakeServer->InjectEntity(std::move(entity));
}

void AddHistoryVisitToFakeSyncServer(const GURL& url) {
  sync_pb::EntitySpecifics entitySpecifics;
  sync_pb::HistorySpecifics* history = entitySpecifics.mutable_history();
  history->set_visit_time_windows_epoch_micros(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  history->set_originator_cache_guid("originator_cache_guid");
  history->mutable_page_transition()->set_core_transition(
      sync_pb::SyncEnums_PageTransition_LINK);
  auto* redirect_entry = history->add_redirect_entries();
  redirect_entry->set_url(url.spec());
  std::unique_ptr<syncer::LoopbackServerEntity> entity =
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/std::string(), /*client_tag=*/
          base::NumberToString(history->visit_time_windows_epoch_micros()),
          entitySpecifics, /*creation_time=*/12345,
          /*last_modified_time=*/12345);
  gSyncFakeServer->InjectEntity(std::move(entity));
}

void AddDeviceInfoToFakeSyncServer(const std::string& device_name,
                                   base::Time last_updated_timestamp) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::DeviceInfoSpecifics& device_info = *specifics.mutable_device_info();
  device_info.set_cache_guid("cache_guid_" + device_name);
  device_info.set_client_name(device_name);
  device_info.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_PHONE);
  device_info.set_sync_user_agent("UserAgent");
  device_info.set_chrome_version("1.0");
  device_info.set_signin_scoped_device_id("Id");
  int64_t mtime = syncer::TimeToProtoTime(last_updated_timestamp);
  device_info.set_last_updated_timestamp(mtime);
  device_info.mutable_feature_fields()->set_send_tab_to_self_receiving_enabled(
      true);

  gSyncFakeServer->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name",
          syncer::DeviceInfoUtil::SpecificsToTag(device_info), specifics,
          /*creation_time=*/mtime, mtime));
}

BOOL IsUrlPresentOnClient(const GURL& url,
                          BOOL expect_present,
                          NSError** error) {
  // Call the history service.
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);

  const GURL block_safe_url(url);
  std::set<GURL> origins;
  origins.insert(block_safe_url);

  __block bool history_service_callback_called = false;
  __block int count = 0;
  history_service->GetCountsAndLastVisitForOriginsForTesting(
      origins, base::BindOnce(^(history::OriginCountAndLastVisitMap result) {
        auto iter = result.find(block_safe_url);
        if (iter != result.end())
          count = iter->second.first;
        history_service_callback_called = true;
      }));

  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:4.0];
  while (!history_service_callback_called &&
         [[NSDate date] compare:deadline] != NSOrderedDescending) {
    base::test::ios::SpinRunLoopWithMaxDelay(base::Seconds(0.1));
  }

  NSString* error_message = nil;
  if (!history_service_callback_called) {
    error_message = @"History::GetCountsAndLastVisitForOrigins callback never "
                     "called, app will probably crash later.";
  } else if (count == 0 && expect_present) {
    error_message = @"URL isn't found in HistoryService.";
  } else if (count > 0 && !expect_present) {
    error_message = @"URL isn't supposed to be in HistoryService.";
  }

  if (error_message != nil && error != nil) {
    NSDictionary* error_info = @{NSLocalizedDescriptionKey : error_message};
    *error = [NSError errorWithDomain:kSyncTestErrorDomain
                                 code:0
                             userInfo:error_info];
    return NO;
  }
  return error_message == nil;
}

void DeleteTypedUrlFromClient(const GURL& url) {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);

  history_service->DeleteURLs({url});
}

void DeleteTypedUrlFromFakeSyncServer(std::string url) {
  std::vector<sync_pb::SyncEntity> typed_urls =
      gSyncFakeServer->GetSyncEntitiesByModelType(syncer::TYPED_URLS);
  std::string entity_id;
  for (const sync_pb::SyncEntity& typed_url : typed_urls) {
    if (typed_url.specifics().typed_url().url() == url) {
      entity_id = typed_url.id_string();
      break;
    }
  }
  if (!entity_id.empty()) {
    std::unique_ptr<syncer::LoopbackServerEntity> entity;
    entity =
        syncer::PersistentTombstoneEntity::CreateNew(entity_id, std::string());
    gSyncFakeServer->InjectEntity(std::move(entity));
  }
}

void AddBookmarkWithSyncPassphrase(const std::string& sync_passphrase) {
  syncer::KeyParamsForTesting key_params =
      syncer::Pbkdf2PassphraseKeyParamsForTesting(sync_passphrase);
  std::unique_ptr<syncer::LoopbackServerEntity> server_entity =
      CreateBookmarkServerEntity("PBKDF2-encrypted bookmark",
                                 GURL("http://example.com/doesnt-matter"));
  server_entity->SetSpecifics(GetEncryptedBookmarkEntitySpecifics(
      server_entity->GetSpecifics().bookmark(), key_params));
  gSyncFakeServer->InjectEntity(std::move(server_entity));
  fake_server::SetNigoriInFakeServer(
      syncer::BuildCustomPassphraseNigoriSpecifics(key_params),
      gSyncFakeServer);
}

}  // namespace chrome_test_util
