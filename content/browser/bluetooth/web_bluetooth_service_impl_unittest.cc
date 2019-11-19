// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

#include <utility>

#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "device/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

using testing::Return;

namespace content {

namespace {

const char kBatteryServiceUUIDString[] = "0000180f-0000-1000-8000-00805f9b34fb";

class FakeBluetoothScanningPrompt : public BluetoothScanningPrompt {
 public:
  FakeBluetoothScanningPrompt(const EventHandler& event_handler,
                              content::BluetoothScanningPrompt::Event event) {
    event_handler.Run(event);
  }

  ~FakeBluetoothScanningPrompt() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothScanningPrompt);
};

class FakeBluetoothAdapter : public device::MockBluetoothAdapter {
 public:
  FakeBluetoothAdapter() = default;

  // device::BluetoothAdapter:
  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override {
    std::move(callback).Run(
        /*is_error=*/false,
        device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
  }

 protected:
  ~FakeBluetoothAdapter() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothAdapter);
};

class FakeWebContentsDelegate : public content::WebContentsDelegate {
 public:
  FakeWebContentsDelegate() = default;
  ~FakeWebContentsDelegate() override = default;

  std::unique_ptr<BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      RenderFrameHost* frame,
      const BluetoothScanningPrompt::EventHandler& event_handler) override {
    return std::make_unique<FakeBluetoothScanningPrompt>(event_handler, event_);
  }

  void set_event(content::BluetoothScanningPrompt::Event event) {
    event_ = event;
  }

 private:
  content::BluetoothScanningPrompt::Event event_ =
      content::BluetoothScanningPrompt::Event::kAllow;

  DISALLOW_COPY_AND_ASSIGN(FakeWebContentsDelegate);
};

class FakeWebBluetoothScanClientImpl : blink::mojom::WebBluetoothScanClient {
 public:
  FakeWebBluetoothScanClientImpl() = default;
  ~FakeWebBluetoothScanClientImpl() override = default;

  // blink::mojom::WebBluetoothScanClient:
  void ScanEvent(blink::mojom::WebBluetoothScanResultPtr result) override {}

  void BindReceiver(
      mojo::PendingAssociatedReceiver<blink::mojom::WebBluetoothScanClient>
          receiver) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(
        base::BindOnce(&FakeWebBluetoothScanClientImpl::OnConnectionError,
                       base::Unretained(this)));
  }

  void OnConnectionError() { on_connection_error_called_ = true; }

  bool on_connection_error_called() { return on_connection_error_called_; }

 private:
  mojo::AssociatedReceiver<blink::mojom::WebBluetoothScanClient> receiver_{
      this};
  bool on_connection_error_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeWebBluetoothScanClientImpl);
};

}  // namespace

class WebBluetoothServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  WebBluetoothServiceImplTest() = default;
  ~WebBluetoothServiceImplTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    // Set up an adapter.
    scoped_refptr<FakeBluetoothAdapter> adapter(new FakeBluetoothAdapter());
    EXPECT_CALL(*adapter, IsPresent()).WillRepeatedly(Return(true));
    device::BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterForTesting(
        adapter);

    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    contents()->SetDelegate(&delegate_);

    // Simulate a frame connected to a bluetooth service.
    service_ =
        contents()->GetMainFrame()->CreateWebBluetoothServiceForTesting();
  }

  void TearDown() override {
    service_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  blink::mojom::WebBluetoothLeScanFilterPtr CreateScanFilter(
      const std::string& name,
      const std::string& name_prefix) {
    base::Optional<std::vector<device::BluetoothUUID>> services;
    services.emplace();
    services->push_back(device::BluetoothUUID(kBatteryServiceUUIDString));
    return blink::mojom::WebBluetoothLeScanFilter::New(services, name,
                                                       name_prefix);
  }

  blink::mojom::RequestScanningStartResultPtr RequestScanningStart(
      const blink::mojom::WebBluetoothLeScanFilter& filter,
      FakeWebBluetoothScanClientImpl* client_impl) {
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothScanClient> client;
    client_impl->BindReceiver(client.InitWithNewEndpointAndPassReceiver());
    auto options = blink::mojom::WebBluetoothRequestLEScanOptions::New();
    options->filters.emplace();
    auto filter_ptr = blink::mojom::WebBluetoothLeScanFilter::New(filter);
    options->filters->push_back(std::move(filter_ptr));
    base::RunLoop loop;
    blink::mojom::RequestScanningStartResultPtr result;
    service_->RequestScanningStart(
        std::move(client), std::move(options),
        base::BindLambdaForTesting(
            [&](blink::mojom::RequestScanningStartResultPtr p) {
              result = std::move(p);
              loop.Quit();
            }));
    loop.Run();
    return result;
  }

  WebBluetoothServiceImpl* service_;
  FakeWebContentsDelegate delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebBluetoothServiceImplTest);
};

TEST_F(WebBluetoothServiceImplTest, ClearStateDuringRequestDevice) {
  auto options = blink::mojom::WebBluetoothRequestDeviceOptions::New();
  options->accept_all_devices = true;

  base::RunLoop loop;
  service_->RequestDevice(
      std::move(options),
      base::BindLambdaForTesting(
          [&loop](blink::mojom::WebBluetoothResult,
                  blink::mojom::WebBluetoothDevicePtr) { loop.Quit(); }));
  service_->ClearState();
  loop.Run();
}

TEST_F(WebBluetoothServiceImplTest, PermissionAllowed) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  base::Optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters));

  FakeWebBluetoothScanClientImpl client_impl;
  blink::mojom::RequestScanningStartResultPtr result =
      RequestScanningStart(*filter, &client_impl);
  EXPECT_FALSE(result->is_error_result());
  // |filters| should be allowed.
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest, PermissionPromptCanceled) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  base::Optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters));

  // Set |event_| to kCanceled in the FakeWebContentsDelegate so that the call
  // to WebBluetoothServiceImpl::RequestScanningStart() will not grant the
  // permission.
  delegate_.set_event(content::BluetoothScanningPrompt::Event::kCanceled);

  FakeWebBluetoothScanClientImpl client_impl;
  blink::mojom::RequestScanningStartResultPtr result =
      RequestScanningStart(*filter, &client_impl);
  EXPECT_TRUE(result->is_error_result());
  EXPECT_EQ(blink::mojom::WebBluetoothResult::PROMPT_CANCELED,
            result->get_error_result());
  // |filters| should still not be allowed.
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenTabHidden) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  base::Optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  FakeWebBluetoothScanClientImpl client_impl;
  blink::mojom::RequestScanningStartResultPtr result =
      RequestScanningStart(*filter, &client_impl);
  EXPECT_FALSE(result->is_error_result());
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters));

  contents()->SetVisibilityAndNotifyObservers(content::Visibility::HIDDEN);

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenTabOccluded) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter = CreateScanFilter("a", "b");
  base::Optional<WebBluetoothServiceImpl::ScanFilters> filters;
  filters.emplace();
  filters->push_back(filter.Clone());
  FakeWebBluetoothScanClientImpl client_impl;
  RequestScanningStart(*filter, &client_impl);
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters));

  contents()->SetVisibilityAndNotifyObservers(content::Visibility::OCCLUDED);

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters));
}

TEST_F(WebBluetoothServiceImplTest,
       BluetoothScanningPermissionRevokedWhenBlocked) {
  blink::mojom::WebBluetoothLeScanFilterPtr filter_1 =
      CreateScanFilter("a", "b");
  base::Optional<WebBluetoothServiceImpl::ScanFilters> filters_1;
  filters_1.emplace();
  filters_1->push_back(filter_1.Clone());
  FakeWebBluetoothScanClientImpl client_impl_1;
  blink::mojom::RequestScanningStartResultPtr result_1 =
      RequestScanningStart(*filter_1, &client_impl_1);
  EXPECT_FALSE(result_1->is_error_result());
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters_1));
  EXPECT_FALSE(client_impl_1.on_connection_error_called());

  blink::mojom::WebBluetoothLeScanFilterPtr filter_2 =
      CreateScanFilter("c", "d");
  base::Optional<WebBluetoothServiceImpl::ScanFilters> filters_2;
  filters_2.emplace();
  filters_2->push_back(filter_2.Clone());
  FakeWebBluetoothScanClientImpl client_impl_2;
  blink::mojom::RequestScanningStartResultPtr result_2 =
      RequestScanningStart(*filter_2, &client_impl_2);
  EXPECT_FALSE(result_2->is_error_result());
  EXPECT_TRUE(service_->AreScanFiltersAllowed(filters_2));
  EXPECT_FALSE(client_impl_2.on_connection_error_called());

  // Set |event_| to kBlock in the FakeWebContentsDelegate so that the next call
  // to WebBluetoothServiceImpl::RequestScanningStart() will block the
  // permission.
  delegate_.set_event(content::BluetoothScanningPrompt::Event::kBlock);

  blink::mojom::WebBluetoothLeScanFilterPtr filter_3 =
      CreateScanFilter("e", "f");
  base::Optional<WebBluetoothServiceImpl::ScanFilters> filters_3;
  filters_3.emplace();
  filters_3->push_back(filter_3.Clone());
  FakeWebBluetoothScanClientImpl client_impl_3;
  blink::mojom::RequestScanningStartResultPtr result_3 =
      RequestScanningStart(*filter_3, &client_impl_3);
  EXPECT_TRUE(result_3->is_error_result());
  EXPECT_EQ(blink::mojom::WebBluetoothResult::SCANNING_BLOCKED,
            result_3->get_error_result());
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters_3));

  // The previously granted Bluetooth scanning permission should be revoked.
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters_1));
  EXPECT_FALSE(service_->AreScanFiltersAllowed(filters_2));

  base::RunLoop().RunUntilIdle();

  // All existing scanning clients are disconnected.
  EXPECT_TRUE(client_impl_1.on_connection_error_called());
  EXPECT_TRUE(client_impl_2.on_connection_error_called());
  EXPECT_TRUE(client_impl_3.on_connection_error_called());
}

}  // namespace content
