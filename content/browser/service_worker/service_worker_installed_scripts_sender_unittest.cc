// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_installed_scripts_sender.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

namespace content {

class ExpectedScriptInfo {
 public:
  ExpectedScriptInfo(
      int64_t resource_id,
      const GURL& script_url,
      const std::vector<std::pair<std::string, std::string>>& headers,
      const std::string& encoding,
      const std::string& body,
      const std::string& meta_data)
      : resource_id_(resource_id),
        script_url_(script_url),
        headers_(headers),
        encoding_(encoding),
        body_(body),
        meta_data_(meta_data) {}

  storage::mojom::ServiceWorkerResourceRecordPtr WriteToDiskCache(
      mojo::Remote<storage::mojom::ServiceWorkerStorageControl>& storage)
      const {
    return ::content::WriteToDiskCacheWithIdSync(
        storage, script_url_, resource_id_, headers_, body_, meta_data_);
  }

  void CheckIfIdentical(
      const blink::mojom::ServiceWorkerScriptInfoPtr& script_info) const {
    EXPECT_EQ(script_url_, script_info->script_url);
    EXPECT_EQ(encoding_, script_info->encoding);
    for (const auto& header : headers_) {
      EXPECT_TRUE(base::Contains(script_info->headers, header.first));
      EXPECT_EQ(header.second, script_info->headers[header.first]);
      script_info->headers.erase(header.first);
    }
    EXPECT_EQ(0u, script_info->headers.size());
    EXPECT_TRUE(script_info->body.is_valid());
    std::string body = ReadDataPipe(std::move(script_info->body));
    EXPECT_EQ(body_, body);
    if (meta_data_.size() == 0) {
      EXPECT_FALSE(script_info->meta_data.is_valid());
      EXPECT_EQ(0u, script_info->meta_data_size);
    } else {
      EXPECT_TRUE(script_info->meta_data.is_valid());
      std::string meta_data = ReadDataPipe(std::move(script_info->meta_data));
      EXPECT_EQ(meta_data_, meta_data);
    }
  }

  const GURL& script_url() const { return script_url_; }

 private:
  const int64_t resource_id_;
  const GURL script_url_;
  const std::vector<std::pair<std::string, std::string>> headers_;
  const std::string encoding_;
  const std::string body_;
  const std::string meta_data_;
};

class MockServiceWorkerInstalledScriptsManager
    : public blink::mojom::ServiceWorkerInstalledScriptsManager {
 public:
  explicit MockServiceWorkerInstalledScriptsManager(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerInstalledScriptsManager>
          receiver)
      : receiver_(this, std::move(receiver)) {}

  MockServiceWorkerInstalledScriptsManager(
      const MockServiceWorkerInstalledScriptsManager&) = delete;
  MockServiceWorkerInstalledScriptsManager& operator=(
      const MockServiceWorkerInstalledScriptsManager&) = delete;

  blink::mojom::ServiceWorkerScriptInfoPtr WaitUntilTransferInstalledScript() {
    EXPECT_TRUE(incoming_script_info_.is_null());
    EXPECT_FALSE(transfer_installed_script_waiter_);
    base::RunLoop loop;
    transfer_installed_script_waiter_ = loop.QuitClosure();
    loop.Run();
    EXPECT_FALSE(incoming_script_info_.is_null());
    return std::move(incoming_script_info_);
  }

  void TransferInstalledScript(
      blink::mojom::ServiceWorkerScriptInfoPtr script_info) override {
    EXPECT_TRUE(incoming_script_info_.is_null());
    EXPECT_TRUE(transfer_installed_script_waiter_);
    incoming_script_info_ = std::move(script_info);
    ASSERT_TRUE(transfer_installed_script_waiter_);
    std::move(transfer_installed_script_waiter_).Run();
  }

 private:
  mojo::Receiver<blink::mojom::ServiceWorkerInstalledScriptsManager> receiver_;
  base::OnceClosure transfer_installed_script_waiter_;
  blink::mojom::ServiceWorkerScriptInfoPtr incoming_script_info_;
};

class ServiceWorkerInstalledScriptsSenderTest : public testing::Test {
 public:
  ServiceWorkerInstalledScriptsSenderTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

 protected:
  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());

    scope_ = GURL("http://www.example.com/test/");
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope_;
    registration_ = ServiceWorkerRegistration::Create(
        options,
        blink::StorageKey::CreateFirstParty(url::Origin::Create(scope_)), 1L,
        context()->AsWeakPtr(), blink::mojom::AncestorFrameType::kNormalFrame);
    version_ = CreateNewServiceWorkerVersion(
        context()->registry(), registration_.get(),
        GURL("http://www.example.com/test/service_worker.js"),
        blink::mojom::ScriptType::kClassic);
    version_->set_fetch_handler_type(
        ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
    version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
  }

  EmbeddedWorkerTestHelper* helper() { return helper_.get(); }
  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerRegistry* registry() { return context()->registry(); }
  ServiceWorkerVersion* version() { return version_.get(); }

 private:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;

  GURL scope_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
};

using FinishedReason = ServiceWorkerInstalledScriptReader::FinishedReason;

TEST_F(ServiceWorkerInstalledScriptsSenderTest, SendScripts) {
  const GURL kMainScriptURL = version()->script_url();
  std::string long_body = "I'm the script body!";
  std::string long_meta_data = "I'm the meta data!";
  long_body.resize(1E6, '!');
  long_meta_data.resize(1E6, '!');
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "1000000"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        std::move(long_body),
        std::move(long_meta_data)}},
      {GURL("https://example.com/imported1"),
       {2,
        GURL("https://example.com/imported1"),
        {{"Content-Length", "22"},
         {"Content-Type", "text/javascript; charset=euc-jp"},
         {"TestHeader", "BlahBlah"}},
        "euc-jp",
        "I'm imported script 1!",
        "I'm the meta data for imported script 1!"}},
      {GURL("https://example.com/imported2"),
       {3,
        GURL("https://example.com/imported2"),
        {{"Content-Length", "0"},
         {"Content-Type", "text/javascript; charset=shift_jis"},
         {"TestHeader", "BlahBlah"}},
        "shift_jis",
        "",
        ""}},
  };

  {
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
    for (const auto& info : kExpectedScriptInfoMap)
      records.push_back(
          info.second.WriteToDiskCache(context()->GetStorageControl()));
    version()->script_cache_map()->SetResources(records);
  }

  auto sender =
      std::make_unique<ServiceWorkerInstalledScriptsSender>(version());

  std::unique_ptr<MockServiceWorkerInstalledScriptsManager> renderer_manager;
  {
    blink::mojom::ServiceWorkerInstalledScriptsInfoPtr scripts_info =
        sender->CreateInfoAndBind();
    ASSERT_TRUE(scripts_info);
    ASSERT_EQ(kExpectedScriptInfoMap.size(),
              scripts_info->installed_urls.size());
    for (const auto& url : scripts_info->installed_urls)
      EXPECT_TRUE(base::Contains(kExpectedScriptInfoMap, url));
    EXPECT_TRUE(scripts_info->manager_receiver.is_valid());
    renderer_manager =
        std::make_unique<MockServiceWorkerInstalledScriptsManager>(
            std::move(scripts_info->manager_receiver));
  }
  ASSERT_TRUE(renderer_manager);

  sender->Start();

  // Stream the installed scripts once.
  for (const auto& expected_script : kExpectedScriptInfoMap) {
    const ExpectedScriptInfo& info = expected_script.second;
    EXPECT_EQ(FinishedReason::kNotFinished, sender->last_finished_reason());
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_EQ(info.script_url(), script_info->script_url);
    info.CheckIfIdentical(script_info);
  }

  // Wait until the last send finishes.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FinishedReason::kSuccess, sender->last_finished_reason());
}

TEST_F(ServiceWorkerInstalledScriptsSenderTest, FailedToSendBody) {
  const GURL kMainScriptURL = version()->script_url();
  std::string long_body = "I'm the body";
  long_body.resize(1E6, '!');
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "1000000"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        std::move(long_body),
        "I'm the meta data!"}},
  };

  {
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
    for (const auto& info : kExpectedScriptInfoMap)
      records.push_back(
          info.second.WriteToDiskCache(context()->GetStorageControl()));
    version()->script_cache_map()->SetResources(records);
  }

  auto sender =
      std::make_unique<ServiceWorkerInstalledScriptsSender>(version());

  std::unique_ptr<MockServiceWorkerInstalledScriptsManager> renderer_manager;
  {
    blink::mojom::ServiceWorkerInstalledScriptsInfoPtr scripts_info =
        sender->CreateInfoAndBind();
    ASSERT_TRUE(scripts_info);
    ASSERT_EQ(kExpectedScriptInfoMap.size(),
              scripts_info->installed_urls.size());
    for (const auto& url : scripts_info->installed_urls)
      EXPECT_TRUE(base::Contains(kExpectedScriptInfoMap, url));
    EXPECT_TRUE(scripts_info->manager_receiver.is_valid());
    renderer_manager =
        std::make_unique<MockServiceWorkerInstalledScriptsManager>(
            std::move(scripts_info->manager_receiver));
  }
  ASSERT_TRUE(renderer_manager);

  sender->Start();
  EXPECT_EQ(FinishedReason::kNotFinished, sender->last_finished_reason());

  {
    // Reset a data pipe during sending the body.
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_TRUE(
        base::Contains(kExpectedScriptInfoMap, script_info->script_url));
    script_info->body.reset();
    kExpectedScriptInfoMap.erase(script_info->script_url);
    // Wait until the error is triggered on the sender side.
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_EQ(FinishedReason::kConnectionError, sender->last_finished_reason());
}

TEST_F(ServiceWorkerInstalledScriptsSenderTest, FailedToSendMetaData) {
  const GURL kMainScriptURL = version()->script_url();
  std::string long_meta_data = "I'm the meta data!";
  long_meta_data.resize(blink::BlobUtils::GetDataPipeCapacity(3E6) + 1, '!');
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "0"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        "",
        std::move(long_meta_data)}},
  };

  {
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
    for (const auto& info : kExpectedScriptInfoMap)
      records.push_back(
          info.second.WriteToDiskCache(context()->GetStorageControl()));
    version()->script_cache_map()->SetResources(records);
  }

  auto sender =
      std::make_unique<ServiceWorkerInstalledScriptsSender>(version());

  std::unique_ptr<MockServiceWorkerInstalledScriptsManager> renderer_manager;
  {
    blink::mojom::ServiceWorkerInstalledScriptsInfoPtr scripts_info =
        sender->CreateInfoAndBind();
    ASSERT_TRUE(scripts_info);
    ASSERT_EQ(kExpectedScriptInfoMap.size(),
              scripts_info->installed_urls.size());
    for (const auto& url : scripts_info->installed_urls)
      EXPECT_TRUE(base::Contains(kExpectedScriptInfoMap, url));
    EXPECT_TRUE(scripts_info->manager_receiver.is_valid());
    renderer_manager =
        std::make_unique<MockServiceWorkerInstalledScriptsManager>(
            std::move(scripts_info->manager_receiver));
  }
  ASSERT_TRUE(renderer_manager);

  sender->Start();
  EXPECT_EQ(FinishedReason::kNotFinished, sender->last_finished_reason());

  {
    // Reset a data pipe during sending the meta data.
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_TRUE(
        base::Contains(kExpectedScriptInfoMap, script_info->script_url));
    script_info->meta_data.reset();
    kExpectedScriptInfoMap.erase(script_info->script_url);
    // Wait until the error is triggered on the sender side.
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_EQ(FinishedReason::kMetaDataSenderError,
            sender->last_finished_reason());
}

TEST_F(ServiceWorkerInstalledScriptsSenderTest, Histograms) {
  const GURL kMainScriptURL = version()->script_url();
  // Use script bodies small enough to be read by one
  // ServiceWorkerResourceReader::ReadData(). The number of
  // ServiceWorker.DiskCache.ReadResponseResult will be two per script (one is
  // reading the body and the other is saying EOD).
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "17"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        "Small script body",
        "I'm the meta data!"}},
      {GURL("https://example.com/imported1"),
       {2,
        GURL("https://example.com/imported1"),
        {{"Content-Length", "21"},
         {"Content-Type", "text/javascript; charset=euc-jp"},
         {"TestHeader", "BlahBlah"}},
        "euc-jp",
        "Small imported script",
        "I'm the meta data for imported script 1!"}},
  };

  {
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
    for (const auto& info : kExpectedScriptInfoMap)
      records.push_back(
          info.second.WriteToDiskCache(context()->GetStorageControl()));
    version()->script_cache_map()->SetResources(records);
  }

  auto sender =
      std::make_unique<ServiceWorkerInstalledScriptsSender>(version());

  std::unique_ptr<MockServiceWorkerInstalledScriptsManager> renderer_manager;
  {
    blink::mojom::ServiceWorkerInstalledScriptsInfoPtr scripts_info =
        sender->CreateInfoAndBind();
    ASSERT_TRUE(scripts_info);
    ASSERT_EQ(kExpectedScriptInfoMap.size(),
              scripts_info->installed_urls.size());
    for (const auto& url : scripts_info->installed_urls)
      EXPECT_TRUE(base::Contains(kExpectedScriptInfoMap, url));
    EXPECT_TRUE(scripts_info->manager_receiver.is_valid());
    renderer_manager =
        std::make_unique<MockServiceWorkerInstalledScriptsManager>(
            std::move(scripts_info->manager_receiver));
  }
  ASSERT_TRUE(renderer_manager);

  base::HistogramTester histogram_tester;
  sender->Start();

  // Stream the installed scripts once.
  for (const auto& expected_script : kExpectedScriptInfoMap) {
    const ExpectedScriptInfo& info = expected_script.second;
    EXPECT_EQ(FinishedReason::kNotFinished, sender->last_finished_reason());
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_EQ(info.script_url(), script_info->script_url);
    info.CheckIfIdentical(script_info);
  }

  // Wait until the last send finishes.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FinishedReason::kSuccess, sender->last_finished_reason());

  // The histogram should be recorded when reading the script.
  // The count should be two: reading the response body of a main script and an
  // imported script.
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.DiskCache.ReadResponseResult",
      ServiceWorkerMetrics::ReadResponseResult::READ_OK, 2);
}

TEST_F(ServiceWorkerInstalledScriptsSenderTest, RequestScriptBeforeStreaming) {
  const GURL kMainScriptURL = version()->script_url();
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "35"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        "I'm script body for the main script",
        "I'm meta data for the main script"}},
      {GURL("https://example.com/imported1"),
       {2,
        GURL("https://example.com/imported1"),
        {{"Content-Length", "22"},
         {"Content-Type", "text/javascript; charset=euc-jp"},
         {"TestHeader", "BlahBlah"}},
        "euc-jp",
        "I'm imported script 1!",
        "I'm the meta data for imported script 1!"}},
      {GURL("https://example.com/imported2"),
       {3,
        GURL("https://example.com/imported2"),
        {{"Content-Length", "0"},
         {"Content-Type", "text/javascript; charset=shift_jis"},
         {"TestHeader", "BlahBlah"}},
        "shift_jis",
        "",
        ""}},
  };

  {
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
    for (const auto& info : kExpectedScriptInfoMap)
      records.push_back(
          info.second.WriteToDiskCache(context()->GetStorageControl()));
    version()->script_cache_map()->SetResources(records);
  }

  auto sender =
      std::make_unique<ServiceWorkerInstalledScriptsSender>(version());

  std::unique_ptr<MockServiceWorkerInstalledScriptsManager> renderer_manager;
  mojo::Remote<blink::mojom::ServiceWorkerInstalledScriptsManagerHost>
      manager_host;
  {
    blink::mojom::ServiceWorkerInstalledScriptsInfoPtr scripts_info =
        sender->CreateInfoAndBind();
    ASSERT_TRUE(scripts_info);
    ASSERT_EQ(kExpectedScriptInfoMap.size(),
              scripts_info->installed_urls.size());
    for (const auto& url : scripts_info->installed_urls)
      EXPECT_TRUE(base::Contains(kExpectedScriptInfoMap, url));
    EXPECT_TRUE(scripts_info->manager_receiver.is_valid());
    renderer_manager =
        std::make_unique<MockServiceWorkerInstalledScriptsManager>(
            std::move(scripts_info->manager_receiver));
    manager_host.Bind(std::move(scripts_info->manager_host_remote));
  }
  ASSERT_TRUE(renderer_manager);

  sender->Start();

  // Request the main script again before receiving the other scripts. It'll be
  // handled after all of script transfer.
  manager_host->RequestInstalledScript(kMainScriptURL);

  // Stream the installed scripts once.
  for (const auto& expected_script : kExpectedScriptInfoMap) {
    const ExpectedScriptInfo& info = expected_script.second;
    EXPECT_EQ(FinishedReason::kNotFinished, sender->last_finished_reason());
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_EQ(info.script_url(), script_info->script_url);
    info.CheckIfIdentical(script_info);
  }
  EXPECT_EQ(FinishedReason::kNotFinished, sender->last_finished_reason());

  // Handle requested installed scripts.
  {
    const ExpectedScriptInfo& info = kExpectedScriptInfoMap.at(kMainScriptURL);
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_EQ(info.script_url(), script_info->script_url);
    info.CheckIfIdentical(script_info);
  }

  // Wait until the last send finishes.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FinishedReason::kSuccess, sender->last_finished_reason());
}

TEST_F(ServiceWorkerInstalledScriptsSenderTest, RequestScriptAfterStreaming) {
  const GURL kMainScriptURL = version()->script_url();
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "35"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        "I'm script body for the main script",
        "I'm meta data for the main script"}},
      {GURL("https://example.com/imported1"),
       {2,
        GURL("https://example.com/imported1"),
        {{"Content-Length", "22"},
         {"Content-Type", "text/javascript; charset=euc-jp"},
         {"TestHeader", "BlahBlah"}},
        "euc-jp",
        "I'm imported script 1!",
        "I'm the meta data for imported script 1!"}},
      {GURL("https://example.com/imported2"),
       {3,
        GURL("https://example.com/imported2"),
        {{"Content-Length", "0"},
         {"Content-Type", "text/javascript; charset=shift_jis"},
         {"TestHeader", "BlahBlah"}},
        "shift_jis",
        "",
        ""}},
  };

  {
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
    for (const auto& info : kExpectedScriptInfoMap)
      records.push_back(
          info.second.WriteToDiskCache(context()->GetStorageControl()));
    version()->script_cache_map()->SetResources(records);
  }

  auto sender =
      std::make_unique<ServiceWorkerInstalledScriptsSender>(version());

  std::unique_ptr<MockServiceWorkerInstalledScriptsManager> renderer_manager;
  mojo::Remote<blink::mojom::ServiceWorkerInstalledScriptsManagerHost>
      manager_host;
  {
    blink::mojom::ServiceWorkerInstalledScriptsInfoPtr scripts_info =
        sender->CreateInfoAndBind();
    ASSERT_TRUE(scripts_info);
    ASSERT_EQ(kExpectedScriptInfoMap.size(),
              scripts_info->installed_urls.size());
    for (const auto& url : scripts_info->installed_urls)
      EXPECT_TRUE(base::Contains(kExpectedScriptInfoMap, url));
    EXPECT_TRUE(scripts_info->manager_receiver.is_valid());
    renderer_manager =
        std::make_unique<MockServiceWorkerInstalledScriptsManager>(
            std::move(scripts_info->manager_receiver));
    manager_host.Bind(std::move(scripts_info->manager_host_remote));
  }
  ASSERT_TRUE(renderer_manager);

  sender->Start();

  // Stream the installed scripts once.
  for (const auto& expected_script : kExpectedScriptInfoMap) {
    const ExpectedScriptInfo& info = expected_script.second;
    EXPECT_EQ(FinishedReason::kNotFinished, sender->last_finished_reason());
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_EQ(info.script_url(), script_info->script_url);
    info.CheckIfIdentical(script_info);
  }
  // Wait until the initial "streaming" ends.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FinishedReason::kSuccess, sender->last_finished_reason());

  // Request the main script again before receiving the other scripts.
  manager_host->RequestInstalledScript(kMainScriptURL);

  // Handle requested installed scripts.
  {
    const ExpectedScriptInfo& info = kExpectedScriptInfoMap.at(kMainScriptURL);
    auto script_info = renderer_manager->WaitUntilTransferInstalledScript();
    EXPECT_EQ(info.script_url(), script_info->script_url);
    info.CheckIfIdentical(script_info);
  }
  // Wait until the second send for the main script finishes.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FinishedReason::kSuccess, sender->last_finished_reason());
}

// Test that the scripts sender aborts gracefully on ServiceWorkerContext
// shutdown.
TEST_F(ServiceWorkerInstalledScriptsSenderTest, NoContext) {
  const GURL kMainScriptURL = version()->script_url();
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "35"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        "I'm script body for the main script",
        "I'm meta data for the main script"}}};
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  for (const auto& info : kExpectedScriptInfoMap)
    records.push_back(
        info.second.WriteToDiskCache(context()->GetStorageControl()));
  version()->script_cache_map()->SetResources(records);
  auto sender =
      std::make_unique<ServiceWorkerInstalledScriptsSender>(version());

  helper()->ShutdownContext();
  base::RunLoop().RunUntilIdle();

  sender->Start();
  EXPECT_EQ(sender->last_finished_reason(), FinishedReason::kNoContextError);
}

// Test that the scripts sender aborts gracefully when a remote connection to
// the Storage Service is disconnected.
TEST_F(ServiceWorkerInstalledScriptsSenderTest, RemoteStorageDisconnection) {
  const GURL kMainScriptURL = version()->script_url();
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "35"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        "I'm script body for the main script",
        "I'm meta data for the main script"}}};
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  for (const auto& info : kExpectedScriptInfoMap)
    records.push_back(
        info.second.WriteToDiskCache(context()->GetStorageControl()));
  version()->script_cache_map()->SetResources(records);
  auto sender =
      std::make_unique<ServiceWorkerInstalledScriptsSender>(version());

  sender->Start();

  helper()->SimulateStorageRestartForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(sender->last_finished_reason(), FinishedReason::kConnectionError);
}

// Test that the scripts sender aborts gracefully when the storage is disabled.
TEST_F(ServiceWorkerInstalledScriptsSenderTest, StorageDisabled) {
  const GURL kMainScriptURL = version()->script_url();
  std::map<GURL, ExpectedScriptInfo> kExpectedScriptInfoMap = {
      {kMainScriptURL,
       {1,
        kMainScriptURL,
        {{"Content-Length", "35"},
         {"Content-Type", "text/javascript; charset=utf-8"},
         {"TestHeader", "BlahBlah"}},
        "utf-8",
        "I'm script body for the main script",
        "I'm meta data for the main script"}}};
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  for (const auto& info : kExpectedScriptInfoMap)
    records.push_back(
        info.second.WriteToDiskCache(context()->GetStorageControl()));
  version()->script_cache_map()->SetResources(records);

  base::RunLoop loop;
  registry()->DisableStorageForTesting(loop.QuitClosure());
  loop.Run();

  auto sender =
      std::make_unique<ServiceWorkerInstalledScriptsSender>(version());
  sender->Start();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(sender->last_finished_reason(), FinishedReason::kConnectionError);
}

}  // namespace content
