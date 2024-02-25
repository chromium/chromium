// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_database.h"

#include <string>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/public/browser/content_index_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/origin.h"

namespace content {
namespace {

using ::testing::_;

class MockContentIndexProvider : public ContentIndexProvider {
 public:
  MOCK_METHOD1(GetIconSizes,
               std::vector<gfx::Size>(blink::mojom::ContentCategory));
  MOCK_METHOD1(OnContentAdded, void(ContentIndexEntry entry));
  MOCK_METHOD3(OnContentDeleted,
               void(int64_t service_Worker_registration_id,
                    const url::Origin& origin,
                    const std::string& description_id));
};

class ContentIndexTestBrowserContext : public TestBrowserContext {
 public:
  ContentIndexTestBrowserContext()
      : delegate_(std::make_unique<MockContentIndexProvider>()) {}
  ~ContentIndexTestBrowserContext() override = default;

  MockContentIndexProvider* GetContentIndexProvider() override {
    return delegate_.get();
  }

 private:
  std::unique_ptr<MockContentIndexProvider> delegate_;
};

void DidRegisterServiceWorker(int64_t* out_service_worker_registration_id,
                              base::OnceClosure quit_closure,
                              blink::ServiceWorkerStatusCode status,
                              const std::string& status_message,
                              int64_t service_worker_registration_id) {
  DCHECK(out_service_worker_registration_id);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status) << status_message;

  *out_service_worker_registration_id = service_worker_registration_id;

  std::move(quit_closure).Run();
}

void DidFindServiceWorkerRegistration(
    scoped_refptr<ServiceWorkerRegistration>* out_service_worker_registration,
    base::OnceClosure quit_closure,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK(out_service_worker_registration);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);

  *out_service_worker_registration = service_worker_registration;

  std::move(quit_closure).Run();
}

void DatabaseErrorCallback(base::OnceClosure quit_closure,
                           blink::mojom::ContentIndexError* out_error,
                           blink::mojom::ContentIndexError error) {
  *out_error = error;
  std::move(quit_closure).Run();
}

void GetDescriptionsCallback(
    base::OnceClosure quit_closure,
    blink::mojom::ContentIndexError* out_error,
    std::vector<blink::mojom::ContentDescriptionPtr>* out_descriptions,
    blink::mojom::ContentIndexError error,
    std::vector<blink::mojom::ContentDescriptionPtr> descriptions) {
  if (out_error)
    *out_error = error;
  DCHECK(out_descriptions);
  *out_descriptions = std::move(descriptions);
  std::move(quit_closure).Run();
}

std::vector<SkBitmap> CreateTestIcons() {
  std::vector<SkBitmap> icons(2);
  icons[0].allocN32Pixels(42, 42);
  icons[1].allocN32Pixels(24, 24);
  return icons;
}

}  // namespace

class ContentIndexDatabaseTest : public ::testing::Test {
 public:
  ContentIndexDatabaseTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP),
        embedded_worker_test_helper_(base::FilePath() /* in memory */) {}

  ContentIndexDatabaseTest(const ContentIndexDatabaseTest&) = delete;
  ContentIndexDatabaseTest& operator=(const ContentIndexDatabaseTest&) = delete;

  ~ContentIndexDatabaseTest() override = default;

  void SetUp() override {
    // Register Service Worker.
    service_worker_registration_id_ = RegisterServiceWorker(origin_);
    ASSERT_NE(service_worker_registration_id_,
              blink::mojom::kInvalidServiceWorkerRegistrationId);
    database_ = std::make_unique<ContentIndexDatabase>(
        &browser_context_, embedded_worker_test_helper_.context_wrapper());
  }

  blink::mojom::ContentDescriptionPtr CreateDescription(const std::string& id) {
    auto icon_definition = blink::mojom::ContentIconDefinition::New();
    icon_definition->src = "https://example.com/image.png";
    icon_definition->type = "image/png";

    std::vector<blink::mojom::ContentIconDefinitionPtr> icons;
    icons.push_back(std::move(icon_definition));

    return blink::mojom::ContentDescription::New(
        id, "title", "description", blink::mojom::ContentCategory::HOME_PAGE,
        std::move(icons), "https://example.com");
  }

  blink::mojom::ContentIndexError AddEntry(
      blink::mojom::ContentDescriptionPtr description,
      std::vector<SkBitmap> icons = CreateTestIcons()) {
    base::RunLoop run_loop;
    blink::mojom::ContentIndexError error;
    database_->AddEntry(
        service_worker_registration_id_, origin_,
        /* is_top_level_context= */ true, std::move(description),
        std::move(icons), launch_url(),
        base::BindOnce(&DatabaseErrorCallback, run_loop.QuitClosure(), &error));
    run_loop.Run();

    return error;
  }

  blink::mojom::ContentIndexError DeleteEntry(const std::string& id) {
    base::RunLoop run_loop;
    blink::mojom::ContentIndexError error;
    database_->DeleteEntry(
        service_worker_registration_id_, origin_, id,
        base::BindOnce(&DatabaseErrorCallback, run_loop.QuitClosure(), &error));
    run_loop.Run();

    return error;
  }

  std::vector<blink::mojom::ContentDescriptionPtr> GetDescriptions(
      blink::mojom::ContentIndexError* out_error = nullptr) {
    base::RunLoop run_loop;
    std::vector<blink::mojom::ContentDescriptionPtr> descriptions;
    database_->GetDescriptions(
        service_worker_registration_id_, origin_,
        base::BindOnce(&GetDescriptionsCallback, run_loop.QuitClosure(),
                       out_error, &descriptions));
    run_loop.Run();
    return descriptions;
  }

  std::vector<SkBitmap> GetIcons(const std::string& id) {
    base::RunLoop run_loop;
    std::vector<SkBitmap> out_icons;
    database_->GetIcons(
        service_worker_registration_id_, id,
        base::BindLambdaForTesting([&](std::vector<SkBitmap> icons) {
          out_icons = std::move(icons);
          run_loop.Quit();
        }));
    run_loop.Run();
    return out_icons;
  }

  std::vector<ContentIndexEntry> GetAllEntries() {
    base::RunLoop run_loop;
    std::vector<ContentIndexEntry> out_entries;
    database_->GetAllEntries(
        base::BindLambdaForTesting([&](blink::mojom::ContentIndexError error,
                                       std::vector<ContentIndexEntry> entries) {
          ASSERT_EQ(error, blink::mojom::ContentIndexError::NONE);
          out_entries = std::move(entries);
          run_loop.Quit();
        }));
    run_loop.Run();
    return out_entries;
  }

  std::unique_ptr<ContentIndexEntry> GetEntry(
      const std::string& description_id) {
    base::RunLoop run_loop;
    std::unique_ptr<ContentIndexEntry> out_entry;
    database_->GetEntry(
        service_worker_registration_id_, description_id,
        base::BindLambdaForTesting([&](std::optional<ContentIndexEntry> entry) {
          if (entry)
            out_entry = std::make_unique<ContentIndexEntry>(std::move(*entry));
          run_loop.Quit();
        }));
    run_loop.Run();
    return out_entry;
  }

  MockContentIndexProvider* provider() {
    return browser_context_.GetContentIndexProvider();
  }

  int64_t service_worker_registration_id() {
    return service_worker_registration_id_;
  }

  void set_service_worker_registration_id(
      int64_t service_worker_registration_id) {
    service_worker_registration_id_ = service_worker_registration_id;
  }

  ContentIndexDatabase* database() { return database_.get(); }

  BrowserTaskEnvironment& task_environment() { return task_environment_; }

  const url::Origin& origin() { return origin_; }

  GURL launch_url() { return origin_.GetURL(); }

  int64_t RegisterServiceWorker(const url::Origin& origin) {
    GURL script_url(origin.GetURL().spec() + "sw.js");
    int64_t service_worker_registration_id =
        blink::mojom::kInvalidServiceWorkerRegistrationId;

    {
      blink::mojom::ServiceWorkerRegistrationOptions options;
      options.scope = origin.GetURL();
      const blink::StorageKey key = blink::StorageKey::CreateFirstParty(origin);
      base::RunLoop run_loop;
      embedded_worker_test_helper_.context()->RegisterServiceWorker(
          script_url, key, options,
          blink::mojom::FetchClientSettingsObject::New(),
          base::BindOnce(&DidRegisterServiceWorker,
                         &service_worker_registration_id,
                         run_loop.QuitClosure()),
          /*requesting_frame_id=*/GlobalRenderFrameHostId(),
          PolicyContainerPolicies());

      run_loop.Run();
    }

    if (service_worker_registration_id ==
        blink::mojom::kInvalidServiceWorkerRegistrationId) {
      ADD_FAILURE() << "Could not obtain a valid Service Worker registration";
      return blink::mojom::kInvalidServiceWorkerRegistrationId;
    }

    {
      base::RunLoop run_loop;
      embedded_worker_test_helper_.context()->registry()->FindRegistrationForId(
          service_worker_registration_id,
          blink::StorageKey::CreateFirstParty(origin),
          base::BindOnce(&DidFindServiceWorkerRegistration,
                         &service_worker_registration_,
                         run_loop.QuitClosure()));
      run_loop.Run();
    }

    // Wait for the worker to be activated.
    base::RunLoop().RunUntilIdle();

    if (!service_worker_registration_) {
      ADD_FAILURE() << "Could not find the new Service Worker registration.";
      return blink::mojom::kInvalidServiceWorkerRegistrationId;
    }

    return service_worker_registration_id;
  }

 private:
  BrowserTaskEnvironment task_environment_;  // Must be first member.
  ContentIndexTestBrowserContext browser_context_;
  url::Origin origin_ = url::Origin::Create(GURL("https://example.com"));
  int64_t service_worker_registration_id_ =
      blink::mojom::kInvalidServiceWorkerRegistrationId;
  EmbeddedWorkerTestHelper embedded_worker_test_helper_;
  scoped_refptr<ServiceWorkerRegistration> service_worker_registration_;
  std::unique_ptr<ContentIndexDatabase> database_;
};

TEST_F(ContentIndexDatabaseTest, DatabaseOperations) {
  // Initially database will be empty.
  {
    blink::mojom::ContentIndexError error;
    auto descriptions = GetDescriptions(&error);
    EXPECT_TRUE(descriptions.empty());
    EXPECT_EQ(error, blink::mojom::ContentIndexError::NONE);
  }

  // Insert entries and expect to find them.
  EXPECT_EQ(AddEntry(CreateDescription("id1")),
            blink::mojom::ContentIndexError::NONE);
  EXPECT_EQ(AddEntry(CreateDescription("id2")),
            blink::mojom::ContentIndexError::NONE);
  EXPECT_EQ(GetDescriptions().size(), 2u);

  // Remove an entry.
  EXPECT_EQ(DeleteEntry("id2"), blink::mojom::ContentIndexError::NONE);

  // Inspect the last remaining element.
  auto descriptions = GetDescriptions();
  ASSERT_EQ(descriptions.size(), 1u);
  auto expected_description = CreateDescription("id1");
  EXPECT_TRUE(descriptions[0]->Equals(*expected_description));
}

TEST_F(ContentIndexDatabaseTest, DatabaseOperationsBadSWID) {
  url::Origin other_origin = url::Origin::Create(GURL("https://other.com"));
  int64_t other_service_worker_registration_id =
      RegisterServiceWorker(other_origin);
  ASSERT_NE(other_service_worker_registration_id,
            blink::mojom::kInvalidServiceWorkerRegistrationId);
  set_service_worker_registration_id(other_service_worker_registration_id);

  blink::mojom::ContentIndexError error;
  auto descriptions = GetDescriptions(&error);
  EXPECT_TRUE(descriptions.empty());
  EXPECT_EQ(error, blink::mojom::ContentIndexError::STORAGE_ERROR);

  EXPECT_EQ(AddEntry(CreateDescription("id1")),
            blink::mojom::ContentIndexError::STORAGE_ERROR);
  EXPECT_EQ(DeleteEntry("id2"), blink::mojom::ContentIndexError::STORAGE_ERROR);
}

TEST_F(ContentIndexDatabaseTest, AddDuplicateIdWillOverwrite) {
  auto description1 = CreateDescription("id");
  description1->title = "title1";
  auto description2 = CreateDescription("id");
  description2->title = "title2";

  EXPECT_EQ(AddEntry(std::move(description1)),
            blink::mojom::ContentIndexError::NONE);
  EXPECT_EQ(AddEntry(std::move(description2)),
            blink::mojom::ContentIndexError::NONE);

  auto descriptions = GetDescriptions();
  ASSERT_EQ(descriptions.size(), 1u);
  EXPECT_EQ(descriptions[0]->id, "id");
  EXPECT_EQ(descriptions[0]->title, "title2");
}

TEST_F(ContentIndexDatabaseTest, DeleteNonExistentEntry) {
  auto descriptions = GetDescriptions();
  EXPECT_TRUE(descriptions.empty());

  EXPECT_EQ(DeleteEntry("id"), blink::mojom::ContentIndexError::NONE);
}

TEST_F(ContentIndexDatabaseTest, ProviderUpdated) {
  {
    std::unique_ptr<ContentIndexEntry> out_entry;
    EXPECT_CALL(*provider(), OnContentAdded(_))
        .WillOnce(testing::Invoke([&](auto entry) {
          out_entry = std::make_unique<ContentIndexEntry>(std::move(entry));
        }));
    EXPECT_EQ(AddEntry(CreateDescription("id")),
              blink::mojom::ContentIndexError::NONE);

    // Wait for the provider to receive the OnContentAdded event.
    task_environment().RunUntilIdle();

    ASSERT_TRUE(out_entry);
    ASSERT_TRUE(out_entry->description);
    EXPECT_EQ(out_entry->service_worker_registration_id,
              service_worker_registration_id());
    EXPECT_EQ(out_entry->description->id, "id");
    EXPECT_EQ(out_entry->launch_url, launch_url());
    EXPECT_FALSE(out_entry->registration_time.is_null());
  }

  {
    EXPECT_CALL(*provider(), OnContentDeleted(service_worker_registration_id(),
                                              origin(), "id"));
    EXPECT_EQ(DeleteEntry("id"), blink::mojom::ContentIndexError::NONE);
    task_environment().RunUntilIdle();
  }
}

TEST_F(ContentIndexDatabaseTest, IconLifetimeTiedToEntry) {
  // Initially we don't have an icon.
  EXPECT_TRUE(GetIcons("id").empty());

  EXPECT_EQ(AddEntry(CreateDescription("id")),
            blink::mojom::ContentIndexError::NONE);
  auto icons = GetIcons("id");
  ASSERT_EQ(icons.size(), 2u);
  if (icons[0].width() > icons[1].width())
    std::swap(icons[0], icons[1]);

  EXPECT_FALSE(icons[0].isNull());
  EXPECT_FALSE(icons[0].drawsNothing());
  EXPECT_EQ(icons[0].width(), 24);
  EXPECT_EQ(icons[0].height(), 24);
  EXPECT_FALSE(icons[1].isNull());
  EXPECT_FALSE(icons[1].drawsNothing());
  EXPECT_EQ(icons[1].width(), 42);
  EXPECT_EQ(icons[1].height(), 42);

  EXPECT_EQ(DeleteEntry("id"), blink::mojom::ContentIndexError::NONE);
  EXPECT_TRUE(GetIcons("id").empty());
}

TEST_F(ContentIndexDatabaseTest, NoIconsAreSupported) {
  // Initially we don't have an icon.
  EXPECT_TRUE(GetIcons("id").empty());

  // Create an entry with no icons.
  EXPECT_EQ(AddEntry(CreateDescription("id"), /* icons= */ {}),
            blink::mojom::ContentIndexError::NONE);

  // No icons should be found.
  EXPECT_TRUE(GetIcons("id").empty());
}

TEST_F(ContentIndexDatabaseTest, GetEntries) {
  // Initially there are no entries.
  EXPECT_FALSE(GetEntry("any-id"));
  EXPECT_TRUE(GetAllEntries().empty());

  std::unique_ptr<ContentIndexEntry> added_entry;
  {
    EXPECT_CALL(*provider(), OnContentAdded(_))
        .WillOnce(testing::Invoke([&](auto entry) {
          added_entry = std::make_unique<ContentIndexEntry>(std::move(entry));
        }));
    EXPECT_EQ(AddEntry(CreateDescription("id")),
              blink::mojom::ContentIndexError::NONE);
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(added_entry);
  }

  // Check the notified entries match the queried entries.
  {
    auto entry = GetEntry("id");
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->description->Equals(*added_entry->description));

    auto entries = GetAllEntries();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_TRUE(entries[0].description->Equals(*added_entry->description));
  }

  // Add one more entry.
  {
    EXPECT_CALL(*provider(), OnContentAdded(_));
    EXPECT_EQ(AddEntry(CreateDescription("id-2")),
              blink::mojom::ContentIndexError::NONE);
    auto entries = GetAllEntries();
    EXPECT_EQ(entries.size(), 2u);
  }
}

TEST_F(ContentIndexDatabaseTest, BlockedOriginsCannotRegisterContent) {
  // Initially adding is fine.
  EXPECT_EQ(AddEntry(CreateDescription("id1")),
            blink::mojom::ContentIndexError::NONE);

  // Two delete events were dispatched.
  database()->BlockOrigin(origin());
  database()->BlockOrigin(origin());

  // Content can't be registered while the origin is blocked.
  EXPECT_EQ(AddEntry(CreateDescription("id2")),
            blink::mojom::ContentIndexError::STORAGE_ERROR);

  // First event dispatch completed.
  database()->UnblockOrigin(origin());

  // Content still can't be registered.
  EXPECT_EQ(AddEntry(CreateDescription("id3")),
            blink::mojom::ContentIndexError::STORAGE_ERROR);

  // Last event dispatch completed.
  database()->UnblockOrigin(origin());

  // Registering is OK now.
  EXPECT_EQ(AddEntry(CreateDescription("id4")),
            blink::mojom::ContentIndexError::NONE);
}

}  // namespace content
