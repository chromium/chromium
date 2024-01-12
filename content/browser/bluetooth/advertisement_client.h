// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_ADVERTISEMENT_CLIENT_H_
#define CONTENT_BROWSER_BLUETOOTH_ADVERTISEMENT_CLIENT_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/bluetooth/bluetooth_device_scanning_prompt_controller.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace content {

namespace {

using RequestCallback =
    base::OnceCallback<void(blink::mojom::WebBluetoothResult)>;

}

class WebBluetoothServiceImpl::AdvertisementClient {
 public:
  virtual void SendEvent(
      const blink::mojom::WebBluetoothAdvertisingEvent& event) = 0;

  bool is_connected() { return client_remote_.is_connected(); }

  void RunCallback(blink::mojom::WebBluetoothResult result) {
    std::move(callback_).Run(result);
  }

 protected:
  explicit AdvertisementClient(
      WebBluetoothServiceImpl* service,
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_info,
      RequestCallback callback);
  virtual ~AdvertisementClient();

  mojo::AssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
      client_remote_;
  raw_ptr<WebContentsImpl> web_contents_;
  raw_ptr<WebBluetoothServiceImpl> service_;

 private:
  RequestCallback callback_;
};

class WebBluetoothServiceImpl::WatchAdvertisementsClient
    : public WebBluetoothServiceImpl::AdvertisementClient {
 public:
  WatchAdvertisementsClient(
      WebBluetoothServiceImpl* service,
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_info,
      blink::WebBluetoothDeviceId device_id,
      RequestCallback callback);

  ~WatchAdvertisementsClient() override;

  // AdvertisementClient implementation:
  void SendEvent(
      const blink::mojom::WebBluetoothAdvertisingEvent& event) override;

  blink::WebBluetoothDeviceId device_id() const { return device_id_; }

 private:
  blink::WebBluetoothDeviceId device_id_;
};

class WebBluetoothServiceImpl::ScanningClient
    : public WebBluetoothServiceImpl::AdvertisementClient {
 public:
  ScanningClient(WebBluetoothServiceImpl* service,
                 mojo::PendingAssociatedRemote<
                     blink::mojom::WebBluetoothAdvertisementClient> client_info,
                 blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
                 RequestCallback callback);

  ~ScanningClient() override;

  void SetPromptController(
      BluetoothDeviceScanningPromptController* prompt_controller) {
    prompt_controller_ = prompt_controller;
  }

  // AdvertisingClient implementation:
  void SendEvent(
      const blink::mojom::WebBluetoothAdvertisingEvent& event) override;

  void set_prompt_controller(
      BluetoothDeviceScanningPromptController* prompt_controller) {
    prompt_controller_ = prompt_controller;
  }

  BluetoothDeviceScanningPromptController* prompt_controller() {
    return prompt_controller_;
  }

  void set_allow_send_event(bool allow_send_event) {
    allow_send_event_ = allow_send_event;
  }

  const blink::mojom::WebBluetoothRequestLEScanOptions& scan_options() {
    return *options_;
  }

 private:
  void AddFilteredDeviceToPrompt(
      const std::string& device_id,
      const std::optional<std::string>& device_name) {
    bool should_update_name = device_name.has_value();
    std::u16string device_name_for_display =
        base::UTF8ToUTF16(device_name.value_or(""));
    prompt_controller_->AddFilteredDevice(device_id, should_update_name,
                                          device_name_for_display);
  }

  bool allow_send_event_ = false;
  blink::mojom::WebBluetoothRequestLEScanOptionsPtr options_;
  raw_ptr<BluetoothDeviceScanningPromptController> prompt_controller_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_ADVERTISEMENT_CLIENT_H_
