// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_installed_scripts_manager.h"

#include <utility>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom-blink.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_embedded_worker.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class BrowserSideSender
    : mojom::blink::ServiceWorkerInstalledScriptsManagerHost {
 public:
  BrowserSideSender() = default;

  BrowserSideSender(const BrowserSideSender&) = delete;
  BrowserSideSender& operator=(const BrowserSideSender&) = delete;

  ~BrowserSideSender() override = default;

  mojom::blink::ServiceWorkerInstalledScriptsInfoPtr CreateAndBind(
      const Vector<KURL>& installed_urls) {
    EXPECT_FALSE(manager_.is_bound());
    EXPECT_FALSE(body_handle_.is_valid());
    EXPECT_FALSE(meta_data_handle_.is_valid());
    auto scripts_info = mojom::blink::ServiceWorkerInstalledScriptsInfo::New();
    scripts_info->installed_urls = installed_urls;
    scripts_info->manager_receiver = manager_.BindNewPipeAndPassReceiver();
    receiver_.Bind(
        scripts_info->manager_host_remote.InitWithNewPipeAndPassReceiver());
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
              mojo::CreateDataPipe(nullptr, body_handle_, script_info->body));
    EXPECT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(nullptr, meta_data_handle_,
                                                   script_info->meta_data));
    script_info->body_size = body_size;
    script_info->meta_data_size = meta_data_size;
    manager_->TransferInstalledScript(std::move(script_info));
  }

  void PushBody(const String& data) {
    PushDataPipe(data.Utf8(), body_handle_.get());
  }

  void PushBodyRawBytes(base::span<const uint8_t> bytes) {
    ASSERT_TRUE(body_handle_.is_valid());
    size_t actually_written_bytes = 0;
    MojoResult rv = body_handle_->WriteData(bytes, MOJO_WRITE_DATA_FLAG_NONE,
                                            actually_written_bytes);
    ASSERT_EQ(MOJO_RESULT_OK, rv);
    ASSERT_EQ(bytes.size(), actually_written_bytes);
  }

  void PushMetaData(const String& data) {
    PushDataPipe(data.Utf8(), meta_data_handle_.get());
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

  // Send |data| with null terminator.
  void PushDataPipe(const std::string& data,
                    const mojo::DataPipeProducerHandle& handle) {
    ASSERT_TRUE(handle.is_valid());

    size_t actually_written_bytes = 0;
    MojoResult rv =
        handle.WriteData(base::as_byte_span(data), MOJO_WRITE_DATA_FLAG_NONE,
                         actually_written_bytes);
    ASSERT_EQ(MOJO_RESULT_OK, rv);
    ASSERT_EQ(data.size(), actually_written_bytes);

    char nul_char = '\0';
    rv = handle.WriteData(base::byte_span_from_ref(nul_char),
                          MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
    ASSERT_EQ(MOJO_RESULT_OK, rv);
    ASSERT_EQ(1u, actually_written_bytes);
  }

  base::OnceClosure requested_script_closure_;
  KURL waiting_requested_url_;

  mojo::Remote<mojom::blink::ServiceWorkerInstalledScriptsManager> manager_;
  mojo::Receiver<mojom::blink::ServiceWorkerInstalledScriptsManagerHost>
      receiver_{this};

  mojo::ScopedDataPipeProducerHandle body_handle_;
  mojo::ScopedDataPipeProducerHandle meta_data_handle_;
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
      : io_thread_(NonMainThread::CreateThread(
            ThreadCreationParams(ThreadType::kTestThread)
                .SetThreadNameForTest("io thread"))),
        worker_thread_(NonMainThread::CreateThread(
            ThreadCreationParams(ThreadType::kTestThread)
                .SetThreadNameForTest("worker thread"))),
        worker_waiter_(std::make_unique<base::WaitableEvent>(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED)) {}

  ServiceWorkerInstalledScriptsManagerTest(
      const ServiceWorkerInstalledScriptsManagerTest&) = delete;
  ServiceWorkerInstalledScriptsManagerTest& operator=(
      const ServiceWorkerInstalledScriptsManagerTest&) = delete;

 protected:
  using RawScriptData = ThreadSafeScriptContainer::RawScriptData;

  void CreateInstalledScriptsManager(
      mojom::blink::ServiceWorkerInstalledScriptsInfoPtr
          installed_scripts_info) {
    auto installed_scripts_manager_params =
        std::make_unique<WebServiceWorkerInstalledScriptsManagerParams>(
            base::ToVector(std::move(installed_scripts_info->installed_urls),
                           ToWebURL),
            std::move(installed_scripts_info->manager_receiver),
            std::move(installed_scripts_info->manager_host_remote));
    installed_scripts_manager_ =
        std::make_unique<ServiceWorkerInstalledScriptsManager>(
            std::move(installed_scripts_manager_params),
            io_thread_->GetTaskRunner());
  }

  base::WaitableEvent* IsScriptInstalledOnWorkerThread(const String& script_url,
                                                       bool* out_installed) {
    PostCrossThreadTask(
        *worker_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            [](ServiceWorkerInstalledScriptsManager* installed_scripts_manager,
               const String& script_url, bool* out_installed,
               base::WaitableEvent* waiter) {
              *out_installed = installed_scripts_manager->IsScriptInstalled(
                  KURL(script_url));
              waiter->Signal();
            },
            CrossThreadUnretained(installed_scripts_manager_.get()), script_url,
            CrossThreadUnretained(out_installed),
            CrossThreadUnretained(worker_waiter_.get())));
    return worker_waiter_.get();
  }

  base::WaitableEvent* GetRawScriptDataOnWorkerThread(
      const String& script_url,
      std::unique_ptr<RawScriptData>* out_data) {
    PostCrossThreadTask(
        *worker_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            &ServiceWorkerInstalledScriptsManagerTest::CallGetRawScriptData,
            CrossThreadUnretained(this), script_url,
            CrossThreadUnretained(out_data),
            CrossThreadUnretained(worker_waiter_.get())));
    return worker_waiter_.get();
  }

  base::WaitableEvent* GetScriptDataOnWorkerThread(
      const String& script_url,
      std::unique_ptr<InstalledScriptsManager::ScriptData>* out_data) {
    PostCrossThreadTask(
        *worker_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            &ServiceWorkerInstalledScriptsManagerTest::CallGetScriptData,
            CrossThreadUnretained(this), script_url,
            CrossThreadUnretained(out_data),
            CrossThreadUnretained(worker_waiter_.get())));
    return worker_waiter_.get();
  }

 private:
  void CallGetRawScriptData(const String& script_url,
                            std::unique_ptr<RawScriptData>* out_data,
                            base::WaitableEvent* waiter) {
    *out_data = installed_scripts_manager_->GetRawScriptData(KURL(script_url));
    waiter->Signal();
  }

  void CallGetScriptData(
      const String& script_url,
      std::unique_ptr<InstalledScriptsManager::ScriptData>* out_data,
      base::WaitableEvent* waiter) {
    *out_data = installed_scripts_manager_->GetScriptData(KURL(script_url));
    waiter->Signal();
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<NonMainThread> io_thread_;
  std::unique_ptr<NonMainThread> worker_thread_;

  std::unique_ptr<base::WaitableEvent> worker_waiter_;

  std::unique_ptr<ServiceWorkerInstalledScriptsManager>
      installed_scripts_manager_;
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
    const String kExpectedBody = "This is a script body.";
    const String kExpectedMetaData = "This is a meta data.";
    const String kScriptInfoEncoding("utf8");
    const HashMap<String, String> kScriptInfoHeaders(
        {{"Cache-Control", "no-cache"}, {"User-Agent", "Chrome"}});

    base::WaitableEvent* get_raw_script_data_waiter =
        GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data);

    // Start transferring the script. +1 for null terminator.
    sender.TransferInstalledScript(
        kScriptUrl, kScriptInfoEncoding, kScriptInfoHeaders,
        kExpectedBody.length() + 1, kExpectedMetaData.length() + 1);
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
    Vector<uint8_t> script_text = script_data->TakeScriptText();
    Vector<uint8_t> meta_data = script_data->TakeMetaData();
    ASSERT_EQ(kExpectedBody.length() + 1, script_text.size());
    EXPECT_EQ(kExpectedBody,
              String(reinterpret_cast<const char*>(script_text.data())));
    ASSERT_EQ(kExpectedMetaData.length() + 1, meta_data.size());
    EXPECT_EQ(kExpectedMetaData,
              String(reinterpret_cast<const char*>(meta_data.data())));
    EXPECT_EQ(kScriptInfoEncoding, script_data->Encoding());
    EXPECT_EQ(ToCrossThreadHTTPHeaderMapData(kScriptInfoHeaders),
              *(script_data->TakeHeaders()));
  }

  {
    std::unique_ptr<RawScriptData> script_data;
    const String kExpectedBody = "This is another script body.";
    const String kExpectedMetaData = "This is another meta data.";
    const String kScriptInfoEncoding("ASCII");
    const HashMap<String, String> kScriptInfoHeaders(
        {{"Connection", "keep-alive"}, {"Content-Length", "512"}});

    // Request the same script again.
    base::WaitableEvent* get_raw_script_data_waiter =
        GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data);

    // It should call a Mojo IPC "RequestInstalledScript()" to the browser.
    sender.WaitForRequestInstalledScript(kScriptUrl);

    // Start transferring the script. +1 for null terminator.
    sender.TransferInstalledScript(
        kScriptUrl, kScriptInfoEncoding, kScriptInfoHeaders,
        kExpectedBody.length() + 1, kExpectedMetaData.length() + 1);
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
    Vector<uint8_t> script_text = script_data->TakeScriptText();
    Vector<uint8_t> meta_data = script_data->TakeMetaData();
    ASSERT_EQ(kExpectedBody.length() + 1, script_text.size());
    EXPECT_EQ(kExpectedBody,
              String(reinterpret_cast<const char*>(script_text.data())));
    ASSERT_EQ(kExpectedMetaData.length() + 1, meta_data.size());
    EXPECT_EQ(kExpectedMetaData,
              String(reinterpret_cast<const char*>(meta_data.data())));
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
    const String kExpectedBody = "This is a script body.";
    const String kExpectedMetaData = "This is a meta data.";
    base::WaitableEvent* get_raw_script_data_waiter =
        GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data);

    // Start transferring the script.
    // Body is expected to be 100 bytes larger than kExpectedBody, but sender
    // only sends kExpectedBody and a null byte (kExpectedBody.length() + 1
    // bytes in total).
    sender.TransferInstalledScript(
        kScriptUrl, "utf8", HashMap<String, String>(),
        kExpectedBody.length() + 100, kExpectedMetaData.length() + 1);
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
    const String kExpectedBody = "This is a script body.";
    const String kExpectedMetaData = "This is a meta data.";
    base::WaitableEvent* get_raw_script_data_waiter =
        GetRawScriptDataOnWorkerThread(kScriptUrl, &script_data);

    // Start transferring the script.
    // Meta data is expected to be 100 bytes larger than kExpectedMetaData, but
    // sender only sends kExpectedMetaData and a null byte
    // (kExpectedMetaData.length() + 1 bytes in total).
    sender.TransferInstalledScript(
        kScriptUrl, "utf8", HashMap<String, String>(),
        kExpectedBody.length() + 1, kExpectedMetaData.length() + 100);
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
    base::WaitableEvent* get_raw_script_data_waiter =
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

TEST_F(ServiceWorkerInstalledScriptsManagerTest, GetScriptDataEncoding) {
  const KURL kScriptUrl1("https://example.com/shift_jis.js");
  const KURL kScriptUrl2("https://example.com/default_utf8.js");
  const KURL kScriptUrl3("https://example.com/invalid_encoding.js");
  const KURL kScriptUrl4("https://example.com/utf16le_bom.js");

  BrowserSideSender sender;
  CreateInstalledScriptsManager(sender.CreateAndBind(
      {kScriptUrl1, kScriptUrl2, kScriptUrl3, kScriptUrl4}));

  {
    std::unique_ptr<InstalledScriptsManager::ScriptData> script_data;
    const String kExpectedBody = "var x = 1;";
    base::WaitableEvent* waiter =
        GetScriptDataOnWorkerThread(kScriptUrl1, &script_data);
    sender.TransferInstalledScript(kScriptUrl1, "Shift_JIS",
                                   HashMap<String, String>(),
                                   kExpectedBody.length() + 1, 0);
    sender.PushBody(kExpectedBody);
    sender.FinishTransferBody();
    sender.FinishTransferMetaData();
    waiter->Wait();
    ASSERT_TRUE(script_data);
    EXPECT_EQ(TextEncoding("Shift_JIS"), script_data->GetScriptEncoding());
  }

  {
    std::unique_ptr<InstalledScriptsManager::ScriptData> script_data;
    const String kExpectedBody = "var x = 2;";
    base::WaitableEvent* waiter =
        GetScriptDataOnWorkerThread(kScriptUrl2, &script_data);
    sender.TransferInstalledScript(kScriptUrl2, g_empty_string,
                                   HashMap<String, String>(),
                                   kExpectedBody.length() + 1, 0);
    sender.PushBody(kExpectedBody);
    sender.FinishTransferBody();
    sender.FinishTransferMetaData();
    waiter->Wait();
    ASSERT_TRUE(script_data);
    EXPECT_EQ(Utf8Encoding(), script_data->GetScriptEncoding());
  }

  {
    std::unique_ptr<InstalledScriptsManager::ScriptData> script_data;
    const String kExpectedBody = "var x = 3;";
    base::WaitableEvent* waiter =
        GetScriptDataOnWorkerThread(kScriptUrl3, &script_data);
    sender.TransferInstalledScript(kScriptUrl3, "invalid-encoding",
                                   HashMap<String, String>(),
                                   kExpectedBody.length() + 1, 0);
    sender.PushBody(kExpectedBody);
    sender.FinishTransferBody();
    sender.FinishTransferMetaData();
    waiter->Wait();
    ASSERT_TRUE(script_data);
    EXPECT_EQ(Latin1Encoding(), script_data->GetScriptEncoding());
  }

  {
    std::unique_ptr<InstalledScriptsManager::ScriptData> script_data;
    // UTF-16LE BOM (0xFF, 0xFE) followed by 'a' (0x61, 0x00) overrides the
    // response header encoding.
    const uint8_t kUtf16LeBytes[] = {0xFF, 0xFE, 0x61, 0x00};
    base::WaitableEvent* waiter =
        GetScriptDataOnWorkerThread(kScriptUrl4, &script_data);
    sender.TransferInstalledScript(kScriptUrl4, "Shift_JIS",
                                   HashMap<String, String>(),
                                   std::size(kUtf16LeBytes), 0);
    sender.PushBodyRawBytes(kUtf16LeBytes);
    sender.FinishTransferBody();
    sender.FinishTransferMetaData();
    waiter->Wait();
    ASSERT_TRUE(script_data);
    EXPECT_EQ(TextEncoding("UTF-16LE"), script_data->GetScriptEncoding());
    String installed_source = script_data->TakeSourceText();
    EXPECT_EQ("a", installed_source);

    auto* loader = MakeGarbageCollected<WorkerClassicScriptLoader>();
    EXPECT_EQ(Utf8Encoding(), loader->GetScriptEncoding());
    loader->DidReceiveData(base::as_chars(base::span(kUtf16LeBytes)));
    loader->DidFinishLoading(0);
    EXPECT_EQ(script_data->GetScriptEncoding(), loader->GetScriptEncoding());
    EXPECT_EQ(installed_source, loader->SourceText());
  }
}

TEST_F(ServiceWorkerInstalledScriptsManagerTest, GetScriptDataFlush) {
  const KURL kScriptUrl1("https://example.com/short.js");
  const KURL kScriptUrl2("https://example.com/incomplete_utf8.js");

  BrowserSideSender sender;
  CreateInstalledScriptsManager(
      sender.CreateAndBind({kScriptUrl1, kScriptUrl2}));

  // 1-byte body is smaller than the 3-byte BOM buffer in TextResourceDecoder.
  {
    std::unique_ptr<InstalledScriptsManager::ScriptData> script_data;
    const uint8_t kShortBody[] = {'a'};
    base::WaitableEvent* waiter =
        GetScriptDataOnWorkerThread(kScriptUrl1, &script_data);
    sender.TransferInstalledScript(kScriptUrl1, "utf-8",
                                   HashMap<String, String>(),
                                   std::size(kShortBody), 0);
    sender.PushBodyRawBytes(kShortBody);
    sender.FinishTransferBody();
    sender.FinishTransferMetaData();
    waiter->Wait();
    ASSERT_TRUE(script_data);
    String installed_source = script_data->TakeSourceText();
    EXPECT_EQ("a", installed_source);

    auto* loader = MakeGarbageCollected<WorkerClassicScriptLoader>();
    loader->DidReceiveData(base::as_chars(base::span(kShortBody)));
    loader->DidFinishLoading(0);
    EXPECT_EQ(installed_source, loader->SourceText());
  }

  // Incomplete 3-byte UTF-8 sequence (0xE3, 0x81) at EOF flushes to U+FFFD.
  {
    std::unique_ptr<InstalledScriptsManager::ScriptData> script_data;
    const uint8_t kIncompleteUtf8[] = {'a', 'b', 'c', 0xE3, 0x81};
    base::WaitableEvent* waiter =
        GetScriptDataOnWorkerThread(kScriptUrl2, &script_data);
    sender.TransferInstalledScript(kScriptUrl2, "utf-8",
                                   HashMap<String, String>(),
                                   std::size(kIncompleteUtf8), 0);
    sender.PushBodyRawBytes(kIncompleteUtf8);
    sender.FinishTransferBody();
    sender.FinishTransferMetaData();
    waiter->Wait();
    ASSERT_TRUE(script_data);
    const UChar kExpected[] = {'a', 'b', 'c', 0xFFFD};
    String installed_source = script_data->TakeSourceText();
    EXPECT_EQ(String(base::span(kExpected)), installed_source);

    auto* loader = MakeGarbageCollected<WorkerClassicScriptLoader>();
    loader->DidReceiveData(base::as_chars(base::span(kIncompleteUtf8)));
    loader->DidFinishLoading(0);
    EXPECT_EQ(installed_source, loader->SourceText());
  }
}

TEST_F(ServiceWorkerInstalledScriptsManagerTest,
       CachedMetadataHandlerEncoding) {
  const KURL kScriptUrl("https://example.com/sw.js");

  auto* utf8_handler =
      MakeGarbageCollected<ServiceWorkerScriptCachedMetadataHandler>(
          nullptr, kScriptUrl, nullptr, TextEncoding("UTF-8"));
  auto* sjis_handler =
      MakeGarbageCollected<ServiceWorkerScriptCachedMetadataHandler>(
          nullptr, kScriptUrl, nullptr, TextEncoding("Shift_JIS"));
  auto* empty_encoding_handler =
      MakeGarbageCollected<ServiceWorkerScriptCachedMetadataHandler>(
          nullptr, kScriptUrl, nullptr, TextEncoding());
  auto* invalid_handler =
      MakeGarbageCollected<ServiceWorkerScriptCachedMetadataHandler>(
          nullptr, kScriptUrl, nullptr, TextEncoding("invalid-encoding"));

  EXPECT_EQ("UTF-8", utf8_handler->Encoding());
  EXPECT_EQ("Shift_JIS", sjis_handler->Encoding());
  EXPECT_EQ(Latin1Encoding().GetName(), empty_encoding_handler->Encoding());
  EXPECT_EQ(Latin1Encoding().GetName(), invalid_handler->Encoding());

  uint32_t utf8_tag = V8CodeCache::TagForCodeCache(utf8_handler);
  uint32_t sjis_tag = V8CodeCache::TagForCodeCache(sjis_handler);
  uint32_t invalid_tag = V8CodeCache::TagForCodeCache(invalid_handler);
  EXPECT_NE(utf8_tag, sjis_tag);
  EXPECT_NE(utf8_tag, invalid_tag);
  EXPECT_EQ(V8CodeCache::TagForCodeCache(empty_encoding_handler), invalid_tag);

  {
    ScopedServiceWorkerScriptEncodingForTest scoped_feature(false);
    EXPECT_EQ(g_empty_string, utf8_handler->Encoding());
    EXPECT_EQ(g_empty_string, sjis_handler->Encoding());
    EXPECT_EQ(g_empty_string, empty_encoding_handler->Encoding());
    EXPECT_EQ(g_empty_string, invalid_handler->Encoding());
    EXPECT_EQ(V8CodeCache::TagForCodeCache(utf8_handler),
              V8CodeCache::TagForCodeCache(sjis_handler));
  }
}

}  // namespace blink
