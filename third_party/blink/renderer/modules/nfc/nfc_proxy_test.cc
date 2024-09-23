// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"

#include <map>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ndef_scan_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/nfc/ndef_reader.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace {

using ::testing::_;
using ::testing::Invoke;

static const char kFakeRecordId[] =
    "https://w3c.github.io/web-nfc/dummy-record-id";
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
};

class FakeNfcService : public device::mojom::blink::NFC {
 public:
  FakeNfcService() : receiver_(this) {}
  ~FakeNfcService() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(
        mojo::PendingReceiver<device::mojom::blink::NFC>(std::move(handle)));
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &FakeNfcService::OnConnectionError, WTF::Unretained(this)));
  }

  void OnConnectionError() {
    receiver_.reset();
    client_.reset();
  }

  void TriggerWatchEvent() {
    if (!client_ || !tag_message_)
      return;

    client_->OnWatch(std::move(watchIDs_), kFakeNfcTagSerialNumber,
                     tag_message_.Clone());
  }

  void set_tag_message(device::mojom::blink::NDEFMessagePtr message) {
    tag_message_ = std::move(message);
  }

  void set_watch_error(device::mojom::blink::NDEFErrorPtr error) {
    watch_error_ = std::move(error);
  }

  WTF::Vector<uint32_t> GetWatches() { return watchIDs_; }

 private:
  // Override methods from device::mojom::blink::NFC.
  void SetClient(
      mojo::PendingRemote<device::mojom::blink::NFCClient> client) override {
    client_.Bind(std::move(client));
  }
  void Push(device::mojom::blink::NDEFMessagePtr message,
            device::mojom::blink::NDEFWriteOptionsPtr options,
            PushCallback callback) override {
    set_tag_message(std::move(message));
    std::move(callback).Run(nullptr);
  }
  void CancelPush() override {}
  void MakeReadOnly(MakeReadOnlyCallback callback) override {}
  void CancelMakeReadOnly() override {}
  void Watch(uint32_t id, WatchCallback callback) override {
    if (watch_error_) {
      std::move(callback).Run(watch_error_.Clone());
      return;
    }
    if (watchIDs_.Find(id) == kNotFound)
      watchIDs_.push_back(id);
    std::move(callback).Run(nullptr);
  }
  void CancelWatch(uint32_t id) override {
    wtf_size_t index = watchIDs_.Find(id);
    if (index != kNotFound)
      watchIDs_.EraseAt(index);
  }

  device::mojom::blink::NDEFErrorPtr watch_error_;
  device::mojom::blink::NDEFMessagePtr tag_message_;
  mojo::Remote<device::mojom::blink::NFCClient> client_;
  WTF::Vector<uint32_t> watchIDs_;
  mojo::Receiver<device::mojom::blink::NFC> receiver_;
};

// Overrides requests for NFC mojo requests with FakeNfcService instances.
class NFCProxyTest : public PageTestBase {
 public:
  NFCProxyTest() { nfc_service_ = std::make_unique<FakeNfcService>(); }

  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    GetFrame().DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
        device::mojom::blink::NFC::Name_,
        WTF::BindRepeating(&FakeNfcService::BindRequest,
                           WTF::Unretained(nfc_service())));
  }

  void TearDown() override {
    GetFrame().DomWindow()->GetBrowserInterfaceBroker().SetBinderForTesting(
        device::mojom::blink::NFC::Name_, {});
  }

  FakeNfcService* nfc_service() { return nfc_service_.get(); }

 private:
  std::unique_ptr<FakeNfcService> nfc_service_;
};

TEST_F(NFCProxyTest, SuccessfulPath) {
  auto* window = GetFrame().DomWindow();
  auto* nfc_proxy = NFCProxy::From(*window);
  auto* reader = MakeGarbageCollected<MockNDEFReader>(window);

  {
    base::RunLoop loop;
    nfc_proxy->StartReading(reader,
                            base::BindLambdaForTesting(
                                [&](device::mojom::blink::NDEFErrorPtr error) {
                                  EXPECT_TRUE(error.is_null());
                                  loop.Quit();
                                }));
    EXPECT_TRUE(nfc_proxy->IsReading(reader));
    loop.Run();
    EXPECT_EQ(nfc_service()->GetWatches().size(), 1u);
  }

  // Construct a NDEFMessagePtr
  auto message = device::mojom::blink::NDEFMessage::New();
  auto record = device::mojom::blink::NDEFRecord::New();
  WTF::Vector<uint8_t> record_data(
      {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10});
  record->record_type = "mime";
  record->id = kFakeRecordId;
  record->data = WTF::Vector<uint8_t>(record_data);
  message->data.push_back(std::move(record));

  {
    base::RunLoop loop;
    EXPECT_CALL(*reader, OnReading(String(kFakeNfcTagSerialNumber),
                                   MessageEquals(record_data)))
        .WillOnce(Invoke([&](const String& serial_number,
                             const device::mojom::blink::NDEFMessage& message) {
          loop.Quit();
        }));

    nfc_proxy->Push(std::move(message), /*options=*/nullptr,
                    base::BindLambdaForTesting(
                        [&](device::mojom::blink::NDEFErrorPtr error) {
                          nfc_service()->TriggerWatchEvent();
                        }));
    loop.Run();
  }

  nfc_proxy->StopReading(reader);
  EXPECT_FALSE(nfc_proxy->IsReading(reader));
  test::RunPendingTasks();
  EXPECT_EQ(nfc_service()->GetWatches().size(), 0u);
}

TEST_F(NFCProxyTest, ErrorPath) {
  auto* window = GetFrame().DomWindow();
  auto* nfc_proxy = NFCProxy::From(*window);
  auto* reader = MakeGarbageCollected<MockNDEFReader>(window);

  // Make the fake NFC service return an error for the incoming watch request.
  nfc_service()->set_watch_error(device::mojom::blink::NDEFError::New(
      device::mojom::blink::NDEFErrorType::NOT_READABLE, ""));
  base::RunLoop loop;
  nfc_proxy->StartReading(
      reader,
      base::BindLambdaForTesting([&](device::mojom::blink::NDEFErrorPtr error) {
        // We got the error prepared before.
        EXPECT_FALSE(error.is_null());
        EXPECT_EQ(error->error_type,
                  device::mojom::blink::NDEFErrorType::NOT_READABLE);
        loop.Quit();
      }));
  EXPECT_TRUE(nfc_proxy->IsReading(reader));
  loop.Run();

  EXPECT_EQ(nfc_service()->GetWatches().size(), 0u);
  EXPECT_FALSE(nfc_proxy->IsReading(reader));
}

}  // namespace
}  // namespace blink
