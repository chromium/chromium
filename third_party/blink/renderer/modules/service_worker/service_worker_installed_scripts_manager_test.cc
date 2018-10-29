// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_installed_scripts_manager.h"

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/waitable_event.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class BrowserSideSender
    : mojom::blink::ServiceWorkerInstalledScriptsManagerHost {
 public:
  BrowserSideSender() : binding_(this) {}
  ~BrowserSideSender() override = default;

  mojom::blink::ServiceWorkerInstalledScriptsInfoPtr CreateAndBind(
      const Vector<KURL>& installed_urls) {
    EXPECT_FALSE(manager_.is_bound());
    EXPECT_FALSE(body_handle_.is_valid());
    EXPECT_FALSE(meta_data_handle_.is_valid());
    auto scripts_info = mojom::blink::ServiceWorkerInstalledScriptsInfo::New();
    scripts_info->installed_urls = installed_urls;
    scripts_info->manager_request = mojo::MakeRequest(&manager_);
    binding_.Bind(mojo::MakeRequest(&scripts_info->manager_host_ptr));
    return scripts_info;
  }

  void TransferInstalledScript(const KURL& script_url,
                               const String& encoding,
                               const HashMap<String, String>& headers,
                               int64_t body_size,
                               int64_t meta_data_size) {
    EXPECT_FALSE(body_handle_.is_valid());
    EXPECT_FALSE(meta_data_handle_.is_valid());
    auto script_info = mojom::blink::ServiceWorkerScriptInfo::New();
    script_info->script_url = script_url;
    script_info->encoding = encoding;
    script_info->headers = headers;
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, &body_handle_, &script_info->body));
    EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, &meta_data_handle_,
                                                   &script_info->meta_data));
    script_info->body_size = body_size;
    script_info->meta_data_size = meta_data_size;
    manager_->TransferInstalledScript(std::move(script_info));
  }

  void PushBody(const std::string& data) {
    PushDataPipe(data, body_handle_.get());
  }

  void PushMetaData(const std::string& data) {
    PushDataPipe(data, meta_data_handle_.get());
  }

  void FinishTransferBody() { body_handle_.reset(); }

  void FinishTransferMetaData() { meta_data_handle_.reset(); }

  void ResetManager() { manager_.reset(); }

  void WaitForRequestInstalledScript(const KURL& script_url) {
    waiting_requested_url_ = script_url;
    base::RunLoop loop;
    requested_script_closure_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  void RequestInstalledScript(const KURL& script_url) override {
    EXPECT_EQ(waiting_requested_url_, script_url);
    ASSERT_TRUE(requested_script_closure_);
    std::move(requested_script_closure_).Run();
  }

  void PushDataPipe(const std::string& data,
                    const mojo::DataPipeProducerHandle& handle) {
    // Send |data| with null terminator.
    ASSERT_TRUE(handle.is_valid());
    uint32_t written_bytes = data.size() + 1;
    MojoResult rv = handle.WriteData(data.c_str(), &written_bytes,
                                     MOJO_WRITE_DATA_FLAG_NONE);
    ASSERT_EQ(MOJO_RESULT_OK, rv);
    ASSERT_EQ(data.size() + 1, written_bytes);
  }

  base::OnceClosure requested_script_closure_;
  KURL waiting_requested_url_;

  mojom::blink::ServiceWorkerInstalledScriptsManagerPtr manager_;
  mojo::Binding<mojom::blink::ServiceWorkerInstalledScriptsManagerHost>
      binding_;

  mojo::ScopedDataPipeProducerHandle body_handle_;
  mojo::ScopedDataPipeProducerHandle meta_data_handle_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSideSender);
};

CrossThreadHTTPHeaderMapData ToCrossThreadHTTPHeaderMapData(
    const HashMap<String, String>& headers) {
  CrossThreadHTTPHeaderMapData data;
  for (const auto& entry : headers)
    data.emplace_back(entry.key, entry.value);
  return data;
}

}  // namespace

class ServiceWorkerInstalledScriptsManagerTest : public testing::Test {
 public:
  ServiceWorkerInstalledScriptsManagerTest()
      : io_thread_(Platform::Current()->CreateThread(
            ThreadCreationParams(WebThreadType::kTestThread)
                .SetThreadNameForTest("io thread"))),
        worker_thread_(Platform::Current()->CreateThread(
            ThreadCreationParams(WebThreadType::kTestThread)
                .SetThreadNameForTest("worker thread"))) {}

 protected:
  using RawScriptData = ThreadSafeScriptContainer::RawScriptData;

  void CreateInstalledScriptsManager(
      mojom::blink::ServiceWorkerInstalledScriptsInfoPtr
          installed_scripts_info) {
    installed_scripts_manager_ =
        std::make_unique<ServiceWorkerInstalledScriptsManager>(
            std::move(installed_scripts_info->installed_urls),
            std::move(installed_scripts_info->manager_request),
            std::move(installed_scripts_info->manager_host_ptr),
            io_thread_->GetTaskRunner());
  }

  WaitableEvent* IsScriptInstalledOnWorkerThread(const String& script_url,
                                                 bool* out_installed) {
    PostCrossThreadTask(
        *worker_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBind(
            [](ServiceWorkerInstalledScriptsManager* installed_scripts_manager,
               const String& script_url, bool* out_installed,
               WaitableEvent* waiter) {
              *out_installed = installed_scripts_manager->IsScriptInstalled(
                  KURL(script_url));
              waiter->Signal();
            },
            CrossThreadUnretained(installed_scripts_manager_.get()), script_url,
            CrossThreadUnretained(out_installed),
            CrossThreadUnretained(&worker_waiter_)));
    return &worker_waiter_;
  }

  WaitableEvent* GetRawScriptDataOnWorkerThread(
      const String& script_url,
      std::unique_ptr<RawScriptData>* out_data) {
    PostCrossThreadTask(
        *worker_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBind(
            &ServiceWorkerInstalledScriptsManagerTest::CallGetRawScriptData,
            CrossThreadUnretained(this), script_url,
            CrossThreadUnretained(out_data),
            CrossThreadUnretained(&worker_waiter_)));
    return &worker_waiter_;
  }

 private:
  void CallGetRawScriptData(const String& script_url,
                            std::unique_ptr<RawScriptData>* out_data,
                            WaitableEvent* waiter) {
    *out_data = installed_scripts_manager_->GetRawScriptData(KURL(script_url));
    waiter->Signal();
  }

  std::unique_ptr<Thread> io_thread_;
  std::unique_ptr<Thread> worker_thread_;

  WaitableEvent worker_waiter_;

  std::unique_ptr<ServiceWorkerInstalledScriptsManager>
      installed_scripts_manager_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerInstalledScriptsManagerTest);
};

TEST_F(ServiceWorkerInstalledScriptsManagerTest, GetRawScriptData) {
  const KURL kScriptUrl("https://example.com/installed1.js");
  const KURL kUnknownScriptUrl("https://example.com/not_installed.js");

  BrowserSideSender sender;
  CreateInstalledScriptsManager(sender.CreateAndBind({kScriptUrl}));

  {
    bool result = false;
    IsScriptInstalledOnWorkerThread(kScriptUrl, &result)->Wait();
    // IsScriptInstalled returns correct answer even before script transfer
    // hasn't been started yet.
    EXPECT_TRUE(result);
  }

  {
    bool result = true;
    IsScriptInstalledOnWorkerThread(kUnknownScriptUrl, &result)->Wait();
    // IsScriptInstalled returns correct answer even before script transfer
    // hasn't been started yet.
    EXPECT_FALSE(result);
  }

  {
    std::unique_ptr<RawScriptData> script_data;
    const std::string kExpectedBody = "This is a script body.";
    const std::string kExpectedMetaData = "This is a meta data.";
    const String kScriptInfoEncoding("utf8");
    const HashMap<String, String> kScriptInfoHeaders(
        {{"Cache-Control", "no-cache"}, {"User-Agent", "Chrome"}});

    WaitableEvent* get_raw_script_data_waiter =
        GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data);

    // Start transferring the script. +1 for null terminator.
    sender.TransferInstalledScript(kScriptUrl, kScriptInfoEncoding,
                                   kScriptInfoHeaders, kExpectedBody.size() + 1,
                                   kExpectedMetaData.size() + 1);
    sender.PushBody(kExpectedBody);
    sender.PushMetaData(kExpectedMetaData);
    // GetRawScriptData should be blocked until body and meta data transfer are
    // finished.
    EXPECT_FALSE(get_raw_script_data_waiter->IsSignaled());
    sender.FinishTransferBody();
    sender.FinishTransferMetaData();

    // Wait for the script's arrival.
    get_raw_script_data_waiter->Wait();
    EXPECT_TRUE(script_data);
    ASSERT_EQ(1u, script_data->ScriptTextChunks().size());
    ASSERT_EQ(kExpectedBody.size() + 1,
              script_data->ScriptTextChunks()[0].size());
    EXPECT_STREQ(kExpectedBody.data(),
                 script_data->ScriptTextChunks()[0].data());
    ASSERT_EQ(1u, script_data->MetaDataChunks().size());
    ASSERT_EQ(kExpectedMetaData.size() + 1,
              script_data->MetaDataChunks()[0].size());
    EXPECT_STREQ(kExpectedMetaData.data(),
                 script_data->MetaDataChunks()[0].data());
    EXPECT_EQ(kScriptInfoEncoding, script_data->Encoding());
    EXPECT_EQ(ToCrossThreadHTTPHeaderMapData(kScriptInfoHeaders),
              *(script_data->TakeHeaders()));
  }

  {
    std::unique_ptr<RawScriptData> script_data;
    const std::string kExpectedBody = "This is another script body.";
    const std::string kExpectedMetaData = "This is another meta data.";
    const String kScriptInfoEncoding("ASCII");
    const HashMap<String, String> kScriptInfoHeaders(
        {{"Connection", "keep-alive"}, {"Content-Length", "512"}});

    // Request the same script again.
    WaitableEvent* get_raw_script_data_waiter =
        GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data);

    // It should call a Mojo IPC "RequestInstalledScript()" to the browser.
    sender.WaitForRequestInstalledScript(kScriptUrl);

    // Start transferring the script. +1 for null terminator.
    sender.TransferInstalledScript(kScriptUrl, kScriptInfoEncoding,
                                   kScriptInfoHeaders, kExpectedBody.size() + 1,
                                   kExpectedMetaData.size() + 1);
    sender.PushBody(kExpectedBody);
    sender.PushMetaData(kExpectedMetaData);
    // GetRawScriptData should be blocked until body and meta data transfer are
    // finished.
    EXPECT_FALSE(get_raw_script_data_waiter->IsSignaled());
    sender.FinishTransferBody();
    sender.FinishTransferMetaData();

    // Wait for the script's arrival.
    get_raw_script_data_waiter->Wait();
    EXPECT_TRUE(script_data);
    ASSERT_EQ(1u, script_data->ScriptTextChunks().size());
    ASSERT_EQ(kExpectedBody.size() + 1,
              script_data->ScriptTextChunks()[0].size());
    EXPECT_STREQ(kExpectedBody.data(),
                 script_data->ScriptTextChunks()[0].data());
    ASSERT_EQ(1u, script_data->MetaDataChunks().size());
    ASSERT_EQ(kExpectedMetaData.size() + 1,
              script_data->MetaDataChunks()[0].size());
    EXPECT_STREQ(kExpectedMetaData.data(),
                 script_data->MetaDataChunks()[0].data());
    EXPECT_EQ(kScriptInfoEncoding, script_data->Encoding());
    EXPECT_EQ(ToCrossThreadHTTPHeaderMapData(kScriptInfoHeaders),
              *(script_data->TakeHeaders()));
  }
}

TEST_F(ServiceWorkerInstalledScriptsManagerTest, EarlyDisconnectionBody) {
  const KURL kScriptUrl("https://example.com/installed1.js");
  const KURL kUnknownScriptUrl("https://example.com/not_installed.js");

  BrowserSideSender sender;
  CreateInstalledScriptsManager(sender.CreateAndBind({kScriptUrl}));

  {
    std::unique_ptr<RawScriptData> script_data;
    const std::string kExpectedBody = "This is a script body.";
    const std::string kExpectedMetaData = "This is a meta data.";
    WaitableEvent* get_raw_script_data_waiter =
        GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data);

    // Start transferring the script.
    // Body is expected to be 100 bytes larger than kExpectedBody, but sender
    // only sends kExpectedBody and a null byte (kExpectedBody.size() + 1 bytes
    // in total).
    sender.TransferInstalledScript(
        kScriptUrl, String::FromUTF8("utf8"), HashMap<String, String>(),
        kExpectedBody.size() + 100, kExpectedMetaData.size() + 1);
    sender.PushBody(kExpectedBody);
    sender.PushMetaData(kExpectedMetaData);
    // GetRawScriptData should be blocked until body and meta data transfer are
    // finished.
    EXPECT_FALSE(get_raw_script_data_waiter->IsSignaled());
    sender.FinishTransferBody();
    sender.FinishTransferMetaData();

    // Wait for the script's arrival.
    get_raw_script_data_waiter->Wait();
    // |script_data| should be null since the data pipe for body
    // gets disconnected during sending.
    EXPECT_FALSE(script_data);
  }

  {
    std::unique_ptr<RawScriptData> script_data;
    GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data)->Wait();
    // |script_data| should be null since the data wasn't received on the
    // renderer process.
    EXPECT_FALSE(script_data);
  }
}

TEST_F(ServiceWorkerInstalledScriptsManagerTest, EarlyDisconnectionMetaData) {
  const KURL kScriptUrl("https://example.com/installed1.js");
  const KURL kUnknownScriptUrl("https://example.com/not_installed.js");

  BrowserSideSender sender;
  CreateInstalledScriptsManager(sender.CreateAndBind({kScriptUrl}));

  {
    std::unique_ptr<RawScriptData> script_data;
    const std::string kExpectedBody = "This is a script body.";
    const std::string kExpectedMetaData = "This is a meta data.";
    WaitableEvent* get_raw_script_data_waiter =
        GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data);

    // Start transferring the script.
    // Meta data is expected to be 100 bytes larger than kExpectedMetaData, but
    // sender only sends kExpectedMetaData and a null byte
    // (kExpectedMetaData.size() + 1 bytes in total).
    sender.TransferInstalledScript(
        kScriptUrl, String::FromUTF8("utf8"), HashMap<String, String>(),
        kExpectedBody.size() + 1, kExpectedMetaData.size() + 100);
    sender.PushBody(kExpectedBody);
    sender.PushMetaData(kExpectedMetaData);
    // GetRawScriptData should be blocked until body and meta data transfer are
    // finished.
    EXPECT_FALSE(get_raw_script_data_waiter->IsSignaled());
    sender.FinishTransferBody();
    sender.FinishTransferMetaData();

    // Wait for the script's arrival.
    get_raw_script_data_waiter->Wait();
    // |script_data| should be null since the data pipe for meta data gets
    // disconnected during sending.
    EXPECT_FALSE(script_data);
  }

  {
    std::unique_ptr<RawScriptData> script_data;
    GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data)->Wait();
    // |script_data| should be null since the data wasn't received on the
    // renderer process.
    EXPECT_FALSE(script_data);
  }
}

TEST_F(ServiceWorkerInstalledScriptsManagerTest, EarlyDisconnectionManager) {
  const KURL kScriptUrl("https://example.com/installed1.js");
  const KURL kUnknownScriptUrl("https://example.com/not_installed.js");

  BrowserSideSender sender;
  CreateInstalledScriptsManager(sender.CreateAndBind({kScriptUrl}));

  {
    std::unique_ptr<RawScriptData> script_data;
    WaitableEvent* get_raw_script_data_waiter =
        GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data);

    // Reset the Mojo connection before sending the script.
    EXPECT_FALSE(get_raw_script_data_waiter->IsSignaled());
    sender.ResetManager();

    // Wait for the script's arrival.
    get_raw_script_data_waiter->Wait();
    // |script_data| should be nullptr since no data will arrive.
    EXPECT_FALSE(script_data);
  }

  {
    std::unique_ptr<RawScriptData> script_data;
    // This should not be blocked because data will not arrive anymore.
    GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data)->Wait();
    // |script_data| should be null since the data wasn't received on the
    // renderer process.
    EXPECT_FALSE(script_data);
  }
}

}  // namespace blink
