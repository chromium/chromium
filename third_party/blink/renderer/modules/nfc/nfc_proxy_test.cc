// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/nfc/ndef_reader.h"
#include "third_party/blink/renderer/modules/nfc/ndef_scan_options.h"
#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace {

using ::testing::_;
using ::testing::Invoke;

static const char kTestUrl[] = "https://w3c.github.io/web-nfc/";
static const char kFakeNfcTagSerialNumber[] = "c0:45:00:02";

MATCHER_P(MessageEquals, expected, "") {
  // Only check the first data array.
  if (arg.data.size() != 1)
    return false;

  const auto& received_data = arg.data[0]->data;
  if (received_data.size() != expected.size())
    return false;

  for (WTF::wtf_size_t i = 0; i < received_data.size(); i++) {
    if (received_data[i] != expected[i]) {
      return false;
    }
  }
  return true;
}

class MockNDEFReader : public NDEFReader {
 public:
  explicit MockNDEFReader(ExecutionContext* execution_context)
      : NDEFReader(execution_context) {}

  MOCK_METHOD2(OnReading,
               void(const String& serial_number,
                    const device::mojom::blink::NDEFMessage& message));
  MOCK_METHOD1(OnError, void(device::mojom::blink::NDEFErrorType error));
};

class FakeNfcService : public device::mojom::blink::NFC {
 public:
  FakeNfcService() : receiver_(this) {}
  ~FakeNfcService() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(
        mojo::PendingReceiver<device::mojom::blink::NFC>(std::move(handle)));
    receiver_.set_disconnect_handler(
        WTF::Bind(&FakeNfcService::OnConnectionError, WTF::Unretained(this)));
  }

  void OnConnectionError() {
    receiver_.reset();
    client_.reset();
  }

  void TriggerWatchEvent() {
    if (!client_ || !tag_message_)
      return;

    // Only match the watches using |url| in options.
    WTF::Vector<uint32_t> ids;
    for (auto& pair : watches_) {
      if (pair.second->url == tag_message_->url) {
        ids.push_back(pair.first);
      }
    }

    if (!ids.IsEmpty()) {
      client_->OnWatch(std::move(ids), kFakeNfcTagSerialNumber,
                       tag_message_.Clone());
    }
  }

  void set_tag_message(device::mojom::blink::NDEFMessagePtr message) {
    tag_message_ = std::move(message);
  }

  WTF::Vector<uint32_t> GetWatches() {
    WTF::Vector<uint32_t> ids;
    for (auto& pair : watches_) {
      ids.push_back(pair.first);
    }
    return ids;
  }

 private:
  // Override methods from device::mojom::blink::NFC.
  void SetClient(
      mojo::PendingRemote<device::mojom::blink::NFCClient> client) override {
    client_.Bind(std::move(client));
  }
  void Push(device::mojom::blink::NDEFMessagePtr message,
            device::mojom::blink::NDEFPushOptionsPtr options,
            PushCallback callback) override {
    set_tag_message(std::move(message));
    std::move(callback).Run(nullptr);
  }
  void CancelPush(device::mojom::blink::NDEFPushTarget target,
                  CancelPushCallback callback) override {
    std::move(callback).Run(nullptr);
  }
  void Watch(device::mojom::blink::NDEFScanOptionsPtr options,
             uint32_t id,
             WatchCallback callback) override {
    watches_.emplace(id, std::move(options));
    std::move(callback).Run(nullptr);
  }
  void CancelWatch(uint32_t id, CancelWatchCallback callback) override {
    if (watches_.erase(id) < 1) {
      std::move(callback).Run(device::mojom::blink::NDEFError::New(
          device::mojom::blink::NDEFErrorType::NOT_FOUND));
    } else {
      std::move(callback).Run(nullptr);
    }
  }
  void CancelAllWatches(CancelAllWatchesCallback callback) override {
    watches_.clear();
    std::move(callback).Run(nullptr);
  }
  void SuspendNFCOperations() override {}
  void ResumeNFCOperations() override {}

  device::mojom::blink::NDEFMessagePtr tag_message_;
  mojo::Remote<device::mojom::blink::NFCClient> client_;
  std::map<uint32_t, device::mojom::blink::NDEFScanOptionsPtr> watches_;
  mojo::Receiver<device::mojom::blink::NFC> receiver_;
};

// Overrides requests for NFC mojo requests with FakeNfcService instances.
class NFCProxyTest : public PageTestBase {
 public:
  NFCProxyTest() { nfc_service_ = std::make_unique<FakeNfcService>(); }

  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    GetDocument().GetBrowserInterfaceBroker().SetBinderForTesting(
        device::mojom::blink::NFC::Name_,
        WTF::BindRepeating(&FakeNfcService::BindRequest,
                           WTF::Unretained(nfc_service())));
  }

  void TearDown() override {
    GetDocument().GetBrowserInterfaceBroker().SetBinderForTesting(
        device::mojom::blink::NFC::Name_, {});
  }

  FakeNfcService* nfc_service() { return nfc_service_.get(); }

  void DestroyNfcService() { nfc_service_.reset(); }

 private:
  std::unique_ptr<FakeNfcService> nfc_service_;
};

TEST_F(NFCProxyTest, SuccessfulPath) {
  auto& document = GetDocument();
  auto* nfc_proxy = NFCProxy::From(document);
  auto* scan_options = NDEFScanOptions::Create();
  scan_options->setURL(kTestUrl);
  auto* reader = MakeGarbageCollected<MockNDEFReader>(&document);

  nfc_proxy->StartReading(reader, scan_options);
  EXPECT_TRUE(nfc_proxy->IsReading(reader));
  test::RunPendingTasks();
  EXPECT_EQ(nfc_service()->GetWatches().size(), 1u);

  // Construct a NDEFMessagePtr
  auto message = device::mojom::blink::NDEFMessage::New();
  message->url = kTestUrl;
  auto record = device::mojom::blink::NDEFRecord::New();
  WTF::Vector<uint8_t> record_data(
      {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10});
  record->record_type = "opaque";
  record->data = WTF::Vector<uint8_t>(record_data);
  message->data.push_back(std::move(record));

  base::RunLoop loop;
  EXPECT_CALL(*reader, OnReading(String(kFakeNfcTagSerialNumber),
                                 MessageEquals(record_data)))
      .WillOnce(Invoke([&](const String& serial_number,
                           const device::mojom::blink::NDEFMessage& message) {
        loop.Quit();
      }));

  nfc_proxy->Push(
      std::move(message), /*options=*/nullptr,
      base::BindLambdaForTesting([&](device::mojom::blink::NDEFErrorPtr error) {
        nfc_service()->TriggerWatchEvent();
      }));
  loop.Run();

  nfc_proxy->StopReading(reader);
  EXPECT_FALSE(nfc_proxy->IsReading(reader));
  test::RunPendingTasks();
  EXPECT_EQ(nfc_service()->GetWatches().size(), 0u);
}

TEST_F(NFCProxyTest, ErrorPath) {
  auto& document = GetDocument();
  auto* nfc_proxy = NFCProxy::From(document);
  auto* scan_options = NDEFScanOptions::Create();
  scan_options->setURL(kTestUrl);
  auto* reader = MakeGarbageCollected<MockNDEFReader>(&document);

  nfc_proxy->StartReading(reader, scan_options);
  EXPECT_TRUE(nfc_proxy->IsReading(reader));
  test::RunPendingTasks();

  base::RunLoop loop;
  EXPECT_CALL(*reader, OnError(_))
      .WillOnce(
          Invoke([&](device::mojom::blink::NDEFErrorType) { loop.Quit(); }));
  DestroyNfcService();
  loop.Run();
  EXPECT_FALSE(nfc_proxy->IsReading(reader));
}

}  // namespace
}  // namespace blink
