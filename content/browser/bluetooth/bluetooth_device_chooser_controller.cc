// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"

#include <set>
#include <string>
#include <unordered_set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/bluetooth/bluetooth_blocklist.h"
#include "content/browser/bluetooth/bluetooth_metrics.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"

using device::BluetoothUUID;
using UUIDSet = device::BluetoothDevice::UUIDSet;

namespace {

// Signal Strength Display Notes:
//
// RSSI values are displayed by the chooser to empower a user to differentiate
// between multiple devices with the same name, comparing devices with different
// names is a secondary goal. It is important that a user is able to move closer
// and farther away from a device and have it transition between two different
// signal strength levels, thus we want to spread RSSI values out evenly accross
// displayed levels.
//
// RSSI values from UMA in RecordRSSISignalStrength are charted here:
// https://photos.app.goo.gl/6R0ksxWzBsfvrbXH2 (2017-10-18)
// with a copy-paste of table data at every 5dBm:
//  dBm   CDF* Histogram Bucket Quantity (hand drawn estimate)
// -100 00.26%
//  -95 01.22% ---
//  -90 04.14% -----------
//  -85 11.24% ----------------------------
//  -80 18.29% ----------------------------
//  -75 27.13% -----------------------------------
//  -70 37.72% ------------------------------------------
//  -65 49.42% ----------------------------------------------
//  -60 62.91% -----------------------------------------------------
//  -55 74.35% ---------------------------------------------
//  -50 83.07% ----------------------------------
//  -45 88.43% ---------------------
//  -40 92.41% ---------------
//  -35 94.84% ---------
//  -30 95.96% ----
//  -25 96.60% --
//  -20 96.90% -
//  -15 97.07%
//  -10 97.21%
//   -5 97.34%
//    0 97.47%
//
// CDF: Cumulative Distribution Function:
// https://en.wikipedia.org/wiki/Cumulative_distribution_function
//
// Conversion to signal strengths is done by selecting 4 threshold points
// equally spaced through the CDF.
const int k20thPercentileRSSI = -79;
const int k40thPercentileRSSI = -69;
const int k60thPercentileRSSI = -61;
const int k80thPercentileRSSI = -52;

}  // namespace

namespace content {

// Sets the default duration for a Bluetooth scan to 60 seconds.
int64_t BluetoothDeviceChooserController::scan_duration_ = 60;

namespace {
// Max length of device name in filter. Bluetooth 5.0 3.C.3.2.2.3 states that
// the maximum device name length is 248 bytes (UTF-8 encoded).
constexpr size_t kMaxLengthForDeviceName = 248;

void LogRequestDeviceOptions(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  DVLOG(1) << "requestDevice called with the following filters: ";
  DVLOG(1) << "acceptAllDevices: " << options->accept_all_devices;

  if (!options->filters)
    return;

  int i = 0;
  for (const auto& filter : options->filters.value()) {
    DVLOG(1) << "Filter #" << ++i;
    if (filter->name)
      DVLOG(1) << "Name: " << filter->name.value();

    if (filter->name_prefix)
      DVLOG(1) << "Name Prefix: " << filter->name_prefix.value();

    if (filter->services) {
      DVLOG(1) << "Services: ";
      DVLOG(1) << "\t[";
      for (const auto& service : filter->services.value())
        DVLOG(1) << "\t\t" << service.canonical_value();
      DVLOG(1) << "\t]";
    }
  }
}

bool IsEmptyOrInvalidFilter(
    const blink::mojom::WebBluetoothLeScanFilterPtr& filter) {
  // At least one member needs to be present.
  if (!filter->name && !filter->name_prefix && !filter->services)
    return true;

  // The renderer will never send a name or a name_prefix longer than
  // kMaxLengthForDeviceName.
  if (filter->name && filter->name->size() > kMaxLengthForDeviceName)
    return true;
  if (filter->name_prefix && filter->name_prefix->size() == 0)
    return true;
  if (filter->name_prefix &&
      filter->name_prefix->size() > kMaxLengthForDeviceName)
    return true;

  return false;
}

bool HasEmptyOrInvalidFilter(
    const base::Optional<
        std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>>& filters) {
  if (!filters) {
    return true;
  }

  return filters->empty()
             ? true
             : filters->end() != std::find_if(filters->begin(), filters->end(),
                                              IsEmptyOrInvalidFilter);
}

bool IsOptionsInvalid(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  if (options->accept_all_devices) {
    return options->filters.has_value();
  } else {
    return HasEmptyOrInvalidFilter(options->filters);
  }
}

bool MatchesFilter(const std::string* device_name,
                   const UUIDSet& device_uuids,
                   const blink::mojom::WebBluetoothLeScanFilterPtr& filter) {
  if (filter->name) {
    if (device_name == nullptr)
      return false;
    if (filter->name.value() != *device_name)
      return false;
  }

  if (filter->name_prefix && filter->name_prefix->size()) {
    if (device_name == nullptr)
      return false;
    if (!base::StartsWith(*device_name, filter->name_prefix.value(),
                          base::CompareCase::SENSITIVE))
      return false;
  }

  if (filter->services) {
    for (const auto& service : filter->services.value()) {
      if (!base::ContainsKey(device_uuids, service)) {
        return false;
      }
    }
  }

  return true;
}

bool MatchesFilters(
    const std::string* device_name,
    const UUIDSet& device_uuids,
    const base::Optional<
        std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>>& filters) {
  DCHECK(!HasEmptyOrInvalidFilter(filters));
  for (const auto& filter : filters.value()) {
    if (MatchesFilter(device_name, device_uuids, filter)) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<device::BluetoothDiscoveryFilter> ComputeScanFilter(
    const base::Optional<
        std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>>& filters) {
  std::unordered_set<BluetoothUUID, device::BluetoothUUIDHash> services;

  if (filters) {
    for (const auto& filter : filters.value()) {
      if (!filter->services) {
        continue;
      }
      for (const auto& service : filter->services.value()) {
        services.insert(service);
      }
    }
  }

  // There isn't much support for GATT over BR/EDR from neither platforms nor
  // devices so performing a Dual scan will find devices that the API is not
  // able to interact with. To avoid wasting power and confusing users with
  // devices they are not able to interact with, we only perform an LE Scan.
  auto discovery_filter = std::make_unique<device::BluetoothDiscoveryFilter>(
      device::BLUETOOTH_TRANSPORT_LE);
  for (const BluetoothUUID& service : services) {
    discovery_filter->AddUUID(service);
  }
  return discovery_filter;
}

void StopDiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  // Nothing goes wrong if the discovery session fails to stop, and we don't
  // need to wait for it before letting the user's script proceed, so we ignore
  // the results here.
  discovery_session->Stop(base::DoNothing(), base::DoNothing());
}

UMARequestDeviceOutcome OutcomeFromChooserEvent(BluetoothChooser::Event event) {
  switch (event) {
    case BluetoothChooser::Event::DENIED_PERMISSION:
      return UMARequestDeviceOutcome::BLUETOOTH_CHOOSER_DENIED_PERMISSION;
    case BluetoothChooser::Event::CANCELLED:
      return UMARequestDeviceOutcome::BLUETOOTH_CHOOSER_CANCELLED;
    case BluetoothChooser::Event::SHOW_OVERVIEW_HELP:
      return UMARequestDeviceOutcome::BLUETOOTH_OVERVIEW_HELP_LINK_PRESSED;
    case BluetoothChooser::Event::SHOW_ADAPTER_OFF_HELP:
      return UMARequestDeviceOutcome::ADAPTER_OFF_HELP_LINK_PRESSED;
    case BluetoothChooser::Event::SHOW_NEED_LOCATION_HELP:
      return UMARequestDeviceOutcome::NEED_LOCATION_HELP_LINK_PRESSED;
    case BluetoothChooser::Event::SELECTED:
      // We can't know if we are going to send a success message yet because
      // the device could have vanished. This event should be histogramed
      // manually after checking if the device is still around.
      NOTREACHED();
      return UMARequestDeviceOutcome::SUCCESS;
    case BluetoothChooser::Event::RESCAN:
      return UMARequestDeviceOutcome::BLUETOOTH_CHOOSER_RESCAN;
  }
  NOTREACHED();
  return UMARequestDeviceOutcome::SUCCESS;
}

void RecordScanningDuration(const base::TimeDelta& duration) {
  UMA_HISTOGRAM_LONG_TIMES("Bluetooth.Web.RequestDevice.ScanningDuration",
                           duration);
}

}  // namespace

BluetoothDeviceChooserController::BluetoothDeviceChooserController(
    WebBluetoothServiceImpl* web_bluetooth_service,
    RenderFrameHost* render_frame_host,
    device::BluetoothAdapter* adapter)
    : adapter_(adapter),
      web_bluetooth_service_(web_bluetooth_service),
      render_frame_host_(render_frame_host),
      web_contents_(WebContents::FromRenderFrameHost(render_frame_host_)),
      discovery_session_timer_(
          FROM_HERE,
          base::TimeDelta::FromSeconds(scan_duration_),
          base::Bind(&BluetoothDeviceChooserController::StopDeviceDiscovery,
                     // base::Timer guarantees it won't call back after its
                     // destructor starts.
                     base::Unretained(this))),
      weak_ptr_factory_(this) {
  CHECK(adapter_);
}

BluetoothDeviceChooserController::~BluetoothDeviceChooserController() {
  if (scanning_start_time_) {
    RecordScanningDuration(base::TimeTicks::Now() -
                           scanning_start_time_.value());
  }

  if (chooser_) {
    DCHECK(!error_callback_.is_null());
    error_callback_.Run(blink::mojom::WebBluetoothResult::CHOOSER_CANCELLED);
  }
}

void BluetoothDeviceChooserController::GetDevice(
    blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
    const SuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // GetDevice should only be called once.
  DCHECK(success_callback_.is_null());
  DCHECK(error_callback_.is_null());

  success_callback_ = success_callback;
  error_callback_ = error_callback;

  // The renderer should never send invalid options.
  if (IsOptionsInvalid(options)) {
    web_bluetooth_service_->CrashRendererAndClosePipe(
        bad_message::BDH_INVALID_OPTIONS);
    return;
  }
  options_ = std::move(options);
  LogRequestDeviceOptions(options_);

  // Check blocklist to reject invalid filters and adjust optional_services.
  if (options_->filters &&
      BluetoothBlocklist::Get().IsExcluded(options_->filters.value())) {
    RecordRequestDeviceOutcome(
        UMARequestDeviceOutcome::BLOCKLISTED_SERVICE_IN_FILTER);
    PostErrorCallback(
        blink::mojom::WebBluetoothResult::REQUEST_DEVICE_WITH_BLOCKLISTED_UUID);
    return;
  }
  BluetoothBlocklist::Get().RemoveExcludedUUIDs(options_.get());

  const url::Origin requesting_origin =
      render_frame_host_->GetLastCommittedOrigin();
  const url::Origin embedding_origin =
      web_contents_->GetMainFrame()->GetLastCommittedOrigin();

  // TODO(crbug.com/518042): Enforce correctly-delegated permissions instead of
  // matching origins. When relaxing this, take care to handle non-sandboxed
  // unique origins.
  if (!embedding_origin.IsSameOriginWith(requesting_origin)) {
    PostErrorCallback(blink::mojom::WebBluetoothResult::
                          REQUEST_DEVICE_FROM_CROSS_ORIGIN_IFRAME);
    return;
  }
  // The above also excludes opaque origins, which are not even same-origin with
  // themselves.
  DCHECK(!requesting_origin.opaque());

  if (!adapter_->IsPresent()) {
    DVLOG(1) << "Bluetooth Adapter not present. Can't serve requestDevice.";
    RecordRequestDeviceOutcome(
        UMARequestDeviceOutcome::BLUETOOTH_ADAPTER_NOT_PRESENT);
    PostErrorCallback(blink::mojom::WebBluetoothResult::NO_BLUETOOTH_ADAPTER);
    return;
  }

  switch (GetContentClient()->browser()->AllowWebBluetooth(
      web_contents_->GetBrowserContext(), requesting_origin,
      embedding_origin)) {
    case ContentBrowserClient::AllowWebBluetoothResult::BLOCK_POLICY: {
      RecordRequestDeviceOutcome(
          UMARequestDeviceOutcome::BLUETOOTH_CHOOSER_POLICY_DISABLED);
      PostErrorCallback(blink::mojom::WebBluetoothResult::
                            CHOOSER_NOT_SHOWN_API_LOCALLY_DISABLED);
      return;
    }
    case ContentBrowserClient::AllowWebBluetoothResult::
        BLOCK_GLOBALLY_DISABLED: {
      // Log to the developer console.
      web_contents_->GetMainFrame()->AddMessageToConsole(
          content::CONSOLE_MESSAGE_LEVEL_INFO,
          "Bluetooth permission has been blocked.");
      // Block requests.
      RecordRequestDeviceOutcome(
          UMARequestDeviceOutcome::BLUETOOTH_GLOBALLY_DISABLED);
      PostErrorCallback(blink::mojom::WebBluetoothResult::
                            CHOOSER_NOT_SHOWN_API_GLOBALLY_DISABLED);
      return;
    }
    case ContentBrowserClient::AllowWebBluetoothResult::ALLOW:
      break;
  }

  BluetoothChooser::EventHandler chooser_event_handler =
      base::Bind(&BluetoothDeviceChooserController::OnBluetoothChooserEvent,
                 base::Unretained(this));

  if (WebContentsDelegate* delegate = web_contents_->GetDelegate()) {
    chooser_ = delegate->RunBluetoothChooser(render_frame_host_,
                                             std::move(chooser_event_handler));
  }

  if (!chooser_.get()) {
    PostErrorCallback(
        blink::mojom::WebBluetoothResult::WEB_BLUETOOTH_NOT_SUPPORTED);
    return;
  }

  if (!chooser_->CanAskForScanningPermission()) {
    DVLOG(1) << "Closing immediately because Chooser cannot obtain permission.";
    OnBluetoothChooserEvent(BluetoothChooser::Event::DENIED_PERMISSION,
                            "" /* device_address */);
    return;
  }

  device_ids_.clear();
  PopulateConnectedDevices();
  if (!chooser_.get()) {
    // If the dialog's closing, no need to do any of the rest of this.
    return;
  }

  if (!adapter_->IsPowered()) {
    chooser_->SetAdapterPresence(
        BluetoothChooser::AdapterPresence::POWERED_OFF);
    return;
  }

  StartDeviceDiscovery();
}

void BluetoothDeviceChooserController::AddFilteredDevice(
    const device::BluetoothDevice& device) {
  base::Optional<std::string> device_name = device.GetName();
  if (chooser_.get()) {
    if (options_->accept_all_devices ||
        MatchesFilters(device_name ? &device_name.value() : nullptr,
                       device.GetUUIDs(), options_->filters)) {
      base::Optional<int8_t> rssi = device.GetInquiryRSSI();
      std::string device_id = device.GetAddress();
      device_ids_.insert(device_id);
      chooser_->AddOrUpdateDevice(
          device_id, !!device.GetName() /* should_update_name */,
          device.GetNameForDisplay(), device.IsGattConnected(),
          web_bluetooth_service_->IsDevicePaired(device.GetAddress()),
          rssi ? CalculateSignalStrengthLevel(rssi.value()) : -1);
    }
  }
}

void BluetoothDeviceChooserController::AdapterPoweredChanged(bool powered) {
  if (!powered && discovery_session_.get()) {
    StopDiscoverySession(std::move(discovery_session_));
  }

  if (chooser_.get()) {
    chooser_->SetAdapterPresence(
        powered ? BluetoothChooser::AdapterPresence::POWERED_ON
                : BluetoothChooser::AdapterPresence::POWERED_OFF);
    if (powered) {
      OnBluetoothChooserEvent(BluetoothChooser::Event::RESCAN,
                              "" /* device_address */);
    }
  }

  if (!powered) {
    discovery_session_timer_.Stop();
  }
}

int BluetoothDeviceChooserController::CalculateSignalStrengthLevel(
    int8_t rssi) {
  RecordRSSISignalStrength(rssi);

  if (rssi < k20thPercentileRSSI) {
    RecordRSSISignalStrengthLevel(content::UMARSSISignalStrengthLevel::LEVEL_0);
    return 0;
  } else if (rssi < k40thPercentileRSSI) {
    RecordRSSISignalStrengthLevel(content::UMARSSISignalStrengthLevel::LEVEL_1);
    return 1;
  } else if (rssi < k60thPercentileRSSI) {
    RecordRSSISignalStrengthLevel(content::UMARSSISignalStrengthLevel::LEVEL_2);
    return 2;
  } else if (rssi < k80thPercentileRSSI) {
    RecordRSSISignalStrengthLevel(content::UMARSSISignalStrengthLevel::LEVEL_3);
    return 3;
  } else {
    RecordRSSISignalStrengthLevel(content::UMARSSISignalStrengthLevel::LEVEL_4);
    return 4;
  }
}

void BluetoothDeviceChooserController::SetTestScanDurationForTesting(
    TestScanDurationSetting setting) {
  switch (setting) {
    case TestScanDurationSetting::IMMEDIATE_TIMEOUT:
      scan_duration_ = 0;
      break;
    case TestScanDurationSetting::NEVER_TIMEOUT:
      scan_duration_ = base::TimeDelta::Max().InSeconds();
      break;
  }
}

void BluetoothDeviceChooserController::PopulateConnectedDevices() {
  // TODO(crbug.com/728897): Use RetrieveGattConnectedDevices once implemented.
  for (const device::BluetoothDevice* device : adapter_->GetDevices()) {
    if (device->IsGattConnected()) {
      AddFilteredDevice(*device);
    }
  }
}

void BluetoothDeviceChooserController::StartDeviceDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (discovery_session_.get() && discovery_session_->IsActive()) {
    // Already running; just increase the timeout.
    discovery_session_timer_.Reset();
    return;
  }

  scanning_start_time_ = base::TimeTicks::Now();

  chooser_->ShowDiscoveryState(BluetoothChooser::DiscoveryState::DISCOVERING);
  adapter_->StartDiscoverySessionWithFilter(
      ComputeScanFilter(options_->filters),
      base::Bind(
          &BluetoothDeviceChooserController::OnStartDiscoverySessionSuccess,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &BluetoothDeviceChooserController::OnStartDiscoverySessionFailed,
          weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothDeviceChooserController::StopDeviceDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (scanning_start_time_) {
    RecordScanningDuration(base::TimeTicks::Now() -
                           scanning_start_time_.value());
    scanning_start_time_.reset();
  }

  StopDiscoverySession(std::move(discovery_session_));
  if (chooser_) {
    chooser_->ShowDiscoveryState(BluetoothChooser::DiscoveryState::IDLE);
  }
}

void BluetoothDeviceChooserController::OnStartDiscoverySessionSuccess(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(1) << "Started discovery session.";
  if (chooser_.get()) {
    discovery_session_ = std::move(discovery_session);
    discovery_session_timer_.Reset();
  } else {
    StopDiscoverySession(std::move(discovery_session));
  }
}

void BluetoothDeviceChooserController::OnStartDiscoverySessionFailed() {
  if (chooser_.get()) {
    chooser_->ShowDiscoveryState(
        BluetoothChooser::DiscoveryState::FAILED_TO_START);
  }
}

void BluetoothDeviceChooserController::OnBluetoothChooserEvent(
    BluetoothChooser::Event event,
    const std::string& device_address) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Shouldn't recieve an event from a closed chooser.
  DCHECK(chooser_.get());

  switch (event) {
    case BluetoothChooser::Event::RESCAN:
      RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
      device_ids_.clear();
      PopulateConnectedDevices();
      DCHECK(chooser_);
      StartDeviceDiscovery();
      // No need to close the chooser so we return.
      return;
    case BluetoothChooser::Event::DENIED_PERMISSION:
      RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
      PostErrorCallback(blink::mojom::WebBluetoothResult::
                            CHOOSER_NOT_SHOWN_USER_DENIED_PERMISSION_TO_SCAN);
      break;
    case BluetoothChooser::Event::CANCELLED:
      RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
      PostErrorCallback(blink::mojom::WebBluetoothResult::CHOOSER_CANCELLED);
      break;
    case BluetoothChooser::Event::SHOW_OVERVIEW_HELP:
      DVLOG(1) << "Overview Help link pressed.";
      RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
      PostErrorCallback(blink::mojom::WebBluetoothResult::CHOOSER_CANCELLED);
      break;
    case BluetoothChooser::Event::SHOW_ADAPTER_OFF_HELP:
      DVLOG(1) << "Adapter Off Help link pressed.";
      RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
      PostErrorCallback(blink::mojom::WebBluetoothResult::CHOOSER_CANCELLED);
      break;
    case BluetoothChooser::Event::SHOW_NEED_LOCATION_HELP:
      DVLOG(1) << "Need Location Help link pressed.";
      RecordRequestDeviceOutcome(OutcomeFromChooserEvent(event));
      PostErrorCallback(blink::mojom::WebBluetoothResult::CHOOSER_CANCELLED);
      break;
    case BluetoothChooser::Event::SELECTED:
      RecordNumOfDevices(options_->accept_all_devices, device_ids_.size());
      // RecordRequestDeviceOutcome is called in the callback, because the
      // device may have vanished.
      PostSuccessCallback(device_address);
      break;
  }
  // Close chooser.
  chooser_.reset();
}

void BluetoothDeviceChooserController::PostSuccessCallback(
    const std::string& device_address) {
  if (!base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(success_callback_, std::move(options_),
                                    device_address))) {
    LOG(WARNING) << "No TaskRunner.";
  }
}

void BluetoothDeviceChooserController::PostErrorCallback(
    blink::mojom::WebBluetoothResult error) {
  if (!base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(error_callback_, error))) {
    LOG(WARNING) << "No TaskRunner.";
  }
}

}  // namespace content
