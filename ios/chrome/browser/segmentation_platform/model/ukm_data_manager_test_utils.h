// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_UKM_DATA_MANAGER_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_UKM_DATA_MANAGER_TEST_UTILS_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "ios/chrome/browser/segmentation_platform/model/ukm_database_client.h"

class GURL;

namespace history {
class HistoryService;
}

namespace segmentation_platform {

// Utility used for testing UKM based engine.
class UkmDataManagerTestUtils {
 public:
  // `owned_db_client` is used for unittests that require multiple clients in
  // the same process.
  explicit UkmDataManagerTestUtils(ukm::TestUkmRecorder* ukm_recorder,
                                   bool owned_db_client = true);
  ~UkmDataManagerTestUtils();

  UkmDataManagerTestUtils(const UkmDataManagerTestUtils&) = delete;
  UkmDataManagerTestUtils& operator=(const UkmDataManagerTestUtils&) = delete;

  // Must be called before the first profile initialization, sets up default
  // model overrides for the given `default_overrides`
  void PreProfileInit(
      const std::map<proto::SegmentId, proto::SegmentationModelMetadata>&
          default_overrides);

  // Sets up the UKM testing for the `profile`. Can be called multiple times in
  // the same process for different profiles, but WillDestroyProfile() must be
  // called before setting up the next profile.
  void SetupForProfile(ProfileIOS* profile);

  // Must be called before destroying `profile`.
  void WillDestroyProfile(ProfileIOS* profile);

  // The UKM observers are registered after platform initialization. Wait for it
  // to register observers, so that the UKM signals written by tests will be
  // recorded in database.
  void WaitForUkmObserverRegistration();

  // Creates a sample page load UKM based model metadata, with a simple SQL
  // feature with `query`.
  proto::SegmentationModelMetadata GetSamplePageLoadMetadata(
      const std::string& query);

  // Records a page load and 2 valid UKM metrics associated with it. May record
  // other UKM metrics that are unrelated to the metadata provided by
  // GetSamplePageLoadMetadata().
  void RecordPageLoadUkm(const GURL& url, base::Time history_timestamp);

  // Returns whether the `url` is part of the UKM database.
  bool IsUrlInDatabase(const GURL& url);

  // Returns the model provider override for the `segment_id`.
  MockDefaultModelProvider* GetDefaultOverride(proto::SegmentId segment_id);

  // History service is needed for validating test URLs written to database.
  void set_history_service(history::HistoryService* history_service) {
    history_service_ = history_service;
  }

  UkmDatabaseClient* ukm_database_client() {
    return ukm_database_client_.get();
  }

 private:
  const raw_ptr<ukm::TestUkmRecorder> ukm_recorder_;
  int source_id_counter_ = 1;
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<UkmDatabaseClient> ukm_database_client_;

  std::unique_ptr<UkmDatabaseClient> owned_db_client_;

  std::map<proto::SegmentId, MockDefaultModelProvider*> default_overrides_;

  base::WeakPtrFactory<UkmDataManagerTestUtils> weak_factory_{this};
};

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_UKM_DATA_MANAGER_TEST_UTILS_H_
