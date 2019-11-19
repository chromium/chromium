// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/787657): Handle hardware key reset and notify the client.
#include "media/gpu/windows/d3d11_cdm_proxy.h"

#include <initguid.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/object_watcher.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/cdm/cdm_proxy_context.h"
#include "media/gpu/windows/d3d11_decryptor.h"

namespace media {

namespace {

// Checks whether there is a hardware protected key exhange method.
// https://msdn.microsoft.com/en-us/library/windows/desktop/dn894125(v=vs.85).aspx
// The key exhange capabilities are checked using these.
// https://msdn.microsoft.com/en-us/library/windows/desktop/hh447640%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396
// https://msdn.microsoft.com/en-us/library/windows/desktop/hh447782(v=vs.85).aspx
bool CanDoHardwareProtectedKeyExchange(ComD3D11VideoDevice video_device,
                                       const GUID& crypto_type) {
  D3D11_VIDEO_CONTENT_PROTECTION_CAPS caps = {};
  HRESULT hresult = video_device->GetContentProtectionCaps(
      &crypto_type, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT, &caps);
  if (FAILED(hresult)) {
    DVLOG(1) << "Failed to get content protection caps.";
    return false;
  }

  for (uint32_t i = 0; i < caps.KeyExchangeTypeCount; ++i) {
    GUID kex_guid = {};
    hresult = video_device->CheckCryptoKeyExchange(
        &crypto_type, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT, i, &kex_guid);
    if (FAILED(hresult)) {
      DVLOG(1) << "Failed to get key exchange GUID";
      return false;
    }

    if (kex_guid == D3D11_KEY_EXCHANGE_HW_PROTECTION)
      return true;
  }

  DVLOG(1) << "Hardware key exchange is not supported.";
  return false;
}

class D3D11CdmProxyContext : public CdmProxyContext {
 public:
  explicit D3D11CdmProxyContext(const GUID& key_info_guid)
      : key_info_guid_(key_info_guid) {}
  ~D3D11CdmProxyContext() override = default;

  // The pointers are owned by the caller.
  void SetKey(ID3D11CryptoSession* crypto_session,
              const std::vector<uint8_t>& key_id,
              CdmProxy::KeyType key_type,
              const std::vector<uint8_t>& key_blob) {
    std::string key_id_str(key_id.begin(), key_id.end());
    KeyInfo key_info(crypto_session, key_blob);
    // Note that this would overwrite an entry but it is completely valid, e.g.
    // updating the keyblob due to a configuration change.
    key_info_map_[key_id_str][key_type] = std::move(key_info);
  }

  void RemoveKey(ID3D11CryptoSession* crypto_session,
                 const std::vector<uint8_t>& key_id) {
    // There's no need for a keytype for Remove() at the moment, because it's
    // used for completely removing keys associated to |key_id|.
    std::string key_id_str(key_id.begin(), key_id.end());
    key_info_map_.erase(key_id_str);
  }

  // Removes all keys from the context.
  void RemoveAllKeys() { key_info_map_.clear(); }

  // CdmProxyContext implementation.
  base::Optional<D3D11DecryptContext> GetD3D11DecryptContext(
      CdmProxy::KeyType key_type,
      const std::string& key_id) override {
    auto key_id_find_it = key_info_map_.find(key_id);
    if (key_id_find_it == key_info_map_.end())
      return base::nullopt;

    auto& key_type_to_key_info = key_id_find_it->second;
    auto key_type_find_it = key_type_to_key_info.find(key_type);
    if (key_type_find_it == key_type_to_key_info.end())
      return base::nullopt;

    auto& key_info = key_type_find_it->second;
    D3D11DecryptContext context = {};
    context.crypto_session = key_info.crypto_session;
    context.key_blob = key_info.key_blob.data();
    context.key_blob_size = key_info.key_blob.size();
    context.key_info_guid = key_info_guid_;
    return context;
  }

 private:
  // A structure to keep the data passed to SetKey(). See documentation for
  // SetKey() for what the fields mean.
  struct KeyInfo {
    KeyInfo() = default;
    KeyInfo(ID3D11CryptoSession* crypto_session, std::vector<uint8_t> key_blob)
        : crypto_session(crypto_session), key_blob(std::move(key_blob)) {}
    KeyInfo(const KeyInfo&) = default;
    ~KeyInfo() = default;

    ID3D11CryptoSession* crypto_session;
    std::vector<uint8_t> key_blob;
  };

  // Maps key ID -> key type -> KeyInfo.
  // The key ID's type is string, which is converted from |key_id| in
  // SetKey(). It's better to use string here rather than convert
  // vector<uint8_t> to string every time in GetD3D11DecryptContext() because
  // in most cases it would be called more often than SetKey() and RemoveKey()
  // combined.
  std::map<std::string, std::map<CdmProxy::KeyType, KeyInfo>> key_info_map_;

  const GUID key_info_guid_;

  DISALLOW_COPY_AND_ASSIGN(D3D11CdmProxyContext);
};

}  // namespace

// Watches for any content protection teardown events.
// If the instance has been started for watching, the destructor will
// automatically stop watching.
class D3D11CdmProxy::HardwareEventWatcher
    : public base::win::ObjectWatcher::Delegate,
      public base::PowerObserver {
 public:
  ~HardwareEventWatcher() override;

  // |teardown_callback| is called on the current sequence.
  // Returns an instance if it starts watching for events, otherwise returns
  // nullptr.
  static std::unique_ptr<HardwareEventWatcher> Create(
      ComD3D11Device device,
      base::RepeatingClosure teardown_callback);

 private:
  HardwareEventWatcher(ComD3D11Device device,
                       base::RepeatingClosure teardown_callback);

  // Start watching for events.
  bool StartWatching();

  // Registers for hardware content protection teardown events.
  // Return true on success.
  bool RegisterHardwareContentProtectionTeardown(ComD3D11Device device);

  // Regiesters for power events, specifically power resume event.
  // Returns true on success.
  bool RegisterPowerEvents();

  // base::win::ObjectWatcher::Delegate implementation.
  void OnObjectSignaled(HANDLE object) override;

  // base::PowerObserver implementation. Other power events are not relevant to
  // this class.
  void OnResume() override;

  // Stops watching for events. Good for clean up.
  void StopWatching();

  // IDXGIAdapter3::RegisterHardwareContentProtectionTeardownStatusEvent
  // allows watching for teardown events. It is queried thru the following
  // Devices.
  ComD3D11Device device_;
  ComDXGIDevice2 dxgi_device_;
  ComDXGIAdapter3 dxgi_adapter_;

  // Cookie, event, and watcher used for watching events from
  // RegisterHardwareContentProtectionTeardownStatusEvent.
  DWORD teardown_event_cookie_ = 0u;
  base::WaitableEvent content_protection_teardown_event_;
  base::RepeatingClosure teardown_callback_;
  base::win::ObjectWatcher teardown_status_watcher_;
};

class D3D11CdmContext : public CdmContext {
 public:
  explicit D3D11CdmContext(const GUID& key_info_guid)
      : cdm_proxy_context_(key_info_guid) {}
  ~D3D11CdmContext() override = default;

  // The pointers are owned by the caller.
  void SetKey(ID3D11CryptoSession* crypto_session,
              const std::vector<uint8_t>& key_id,
              CdmProxy::KeyType key_type,
              const std::vector<uint8_t>& key_blob) {
    cdm_proxy_context_.SetKey(crypto_session, key_id, key_type, key_blob);
    event_callbacks_.Notify(Event::kHasAdditionalUsableKey);
  }
  void RemoveKey(ID3D11CryptoSession* crypto_session,
                 const std::vector<uint8_t>& key_id) {
    cdm_proxy_context_.RemoveKey(crypto_session, key_id);
  }

  // Notifies of hardware reset.
  void OnHardwareReset() {
    cdm_proxy_context_.RemoveAllKeys();
    event_callbacks_.Notify(Event::kHardwareContextLost);
  }

  base::WeakPtr<D3D11CdmContext> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // CdmContext implementation.
  std::unique_ptr<CallbackRegistration> RegisterEventCB(
      EventCB event_cb) override {
    return event_callbacks_.Register(std::move(event_cb));
  }
  CdmProxyContext* GetCdmProxyContext() override { return &cdm_proxy_context_; }

  Decryptor* GetDecryptor() override {
    if (!decryptor_)
      decryptor_.reset(new D3D11Decryptor(&cdm_proxy_context_));

    return decryptor_.get();
  }

 private:
  D3D11CdmProxyContext cdm_proxy_context_;

  std::unique_ptr<D3D11Decryptor> decryptor_;

  CallbackRegistry<EventCB::RunType> event_callbacks_;

  base::WeakPtrFactory<D3D11CdmContext> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(D3D11CdmContext);
};

D3D11CdmProxy::D3D11CdmProxy(const GUID& crypto_type,
                             CdmProxy::Protocol protocol,
                             const FunctionIdMap& function_id_map)
    : crypto_type_(crypto_type),
      protocol_(protocol),
      function_id_map_(function_id_map),
      cdm_context_(std::make_unique<D3D11CdmContext>(crypto_type)),
      create_device_func_(base::BindRepeating(D3D11CreateDevice)) {}

D3D11CdmProxy::~D3D11CdmProxy() {}

base::WeakPtr<CdmContext> D3D11CdmProxy::GetCdmContext() {
  return cdm_context_->GetWeakPtr();
}

void D3D11CdmProxy::Initialize(Client* client, InitializeCB init_cb) {
  DCHECK(client);

  auto failed = [this, &init_cb]() {
    // The value doesn't matter as it shouldn't be used on a failure.
    const uint32_t kFailedCryptoSessionId = 0xFF;
    std::move(init_cb).Run(Status::kFail, protocol_, kFailedCryptoSessionId);
  };

  if (initialized_) {
    failed();
    NOTREACHED() << "CdmProxy should not be initialized more than once.";
    return;
  }

  client_ = client;

  const D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_1};

  HRESULT hresult = create_device_func_.Run(
      nullptr,                            // No adapter.
      D3D_DRIVER_TYPE_HARDWARE, nullptr,  // No software rasterizer.
      0,                                  // flags, none.
      feature_levels, base::size(feature_levels), D3D11_SDK_VERSION,
      device_.GetAddressOf(), nullptr, device_context_.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to create the D3D11Device:" << hresult;
    failed();
    return;
  }

  // TODO(rkuroiwa): This should be registered iff
  // D3D11_CONTENT_PROTECTION_CAPS_HARDWARE_TEARDOWN is set in the capabilities.
  hardware_event_watcher_ = HardwareEventWatcher::Create(
      device_, base::BindRepeating(
                   &D3D11CdmProxy::NotifyHardwareContentProtectionTeardown,
                   weak_factory_.GetWeakPtr()));
  if (!hardware_event_watcher_) {
    DLOG(ERROR)
        << "Failed to start waching for content protection teardown events.";
    failed();
    return;
  }

  hresult = device_.CopyTo(video_device_.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to get ID3D11VideoDevice: " << hresult;
    failed();
    return;
  }

  if (!CanDoHardwareProtectedKeyExchange(video_device_, crypto_type_)) {
    DLOG(ERROR) << "Cannot do hardware protected key exchange.";
    failed();
    return;
  }

  hresult = device_context_.CopyTo(video_context_.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to get ID3D11VideoContext: " << hresult;
    failed();
    return;
  }

  hresult = device_.CopyTo(video_device1_.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to get ID3D11VideoDevice1: " << hresult;
    failed();
    return;
  }

  hresult = device_context_.CopyTo(video_context1_.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to get ID3D11VideoContext1: " << hresult;
    failed();
    return;
  }

  ComD3D11CryptoSession csme_crypto_session;
  hresult = video_device_->CreateCryptoSession(
      &crypto_type_, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
      &D3D11_KEY_EXCHANGE_HW_PROTECTION, csme_crypto_session.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to Create CryptoSession: " << hresult;
    failed();
    return;
  }

  hresult = video_device1_->GetCryptoSessionPrivateDataSize(
      &crypto_type_, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
      &D3D11_KEY_EXCHANGE_HW_PROTECTION, &private_input_size_,
      &private_output_size_);
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to get private data sizes: " << hresult;
    failed();
    return;
  }

  const uint32_t crypto_session_id = next_crypto_session_id_++;
  crypto_session_map_[crypto_session_id] = std::move(csme_crypto_session);
  initialized_ = true;
  std::move(init_cb).Run(Status::kOk, protocol_, crypto_session_id);
}

void D3D11CdmProxy::Process(Function function,
                            uint32_t crypto_session_id,
                            const std::vector<uint8_t>& input_data_vec,
                            uint32_t expected_output_data_size,
                            ProcessCB process_cb) {
  auto failed = [&process_cb]() {
    std::move(process_cb).Run(Status::kFail, std::vector<uint8_t>());
  };

  if (!initialized_) {
    DLOG(ERROR) << "Not initialied.";
    failed();
    return;
  }

  auto function_id_it = function_id_map_.find(function);
  if (function_id_it == function_id_map_.end()) {
    DLOG(ERROR) << "Unrecognized function: " << static_cast<int>(function);
    failed();
    return;
  }

  auto crypto_session_it = crypto_session_map_.find(crypto_session_id);
  if (crypto_session_it == crypto_session_map_.end()) {
    DLOG(ERROR) << "Cannot find crypto session with ID " << crypto_session_id;
    failed();
    return;
  }

  ComD3D11CryptoSession& crypto_session = crypto_session_it->second;

  D3D11_KEY_EXCHANGE_HW_PROTECTION_DATA key_exchange_data = {};
  key_exchange_data.HWProtectionFunctionID = function_id_it->second;

  // Because D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA and
  // D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA are variable size structures,
  // uint8 array are allocated and casted to each type.
  // -4 for the "BYTE pbInput[4]" field.
  std::unique_ptr<uint8_t[]> input_data_raw(
      new uint8_t[sizeof(D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA) - 4 +
                  input_data_vec.size()]);
  std::unique_ptr<uint8_t[]> output_data_raw(
      new uint8_t[sizeof(D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA) - 4 +
                  expected_output_data_size]);

  D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA* input_data =
      reinterpret_cast<D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA*>(
          input_data_raw.get());
  D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA* output_data =
      reinterpret_cast<D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA*>(
          output_data_raw.get());

  key_exchange_data.pInputData = input_data;
  key_exchange_data.pOutputData = output_data;
  input_data->PrivateDataSize = private_input_size_;
  input_data->HWProtectionDataSize = 0;
  memcpy(input_data->pbInput, input_data_vec.data(), input_data_vec.size());

  output_data->PrivateDataSize = private_output_size_;
  output_data->HWProtectionDataSize = 0;
  output_data->TransportTime = 0;
  output_data->ExecutionTime = 0;
  output_data->MaxHWProtectionDataSize = expected_output_data_size;

  HRESULT hresult = video_context_->NegotiateCryptoSessionKeyExchange(
      crypto_session.Get(), sizeof(key_exchange_data), &key_exchange_data);
  if (FAILED(hresult)) {
    failed();
    return;
  }

  std::move(process_cb)
      .Run(Status::kOk, std::vector<uint8_t>(
                            output_data->pbOutput,
                            output_data->pbOutput + expected_output_data_size));
  return;
}

void D3D11CdmProxy::CreateMediaCryptoSession(
    const std::vector<uint8_t>& input_data,
    CreateMediaCryptoSessionCB create_media_crypto_session_cb) {
  auto failed = [&create_media_crypto_session_cb]() {
    const uint32_t kInvalidSessionId = 0;
    const uint64_t kNoOutputData = 0;
    std::move(create_media_crypto_session_cb)
        .Run(Status::kFail, kInvalidSessionId, kNoOutputData);
  };
  if (!initialized_) {
    DLOG(ERROR) << "Not initialized.";
    failed();
    return;
  }

  ComD3D11CryptoSession media_crypto_session;
  HRESULT hresult = video_device_->CreateCryptoSession(
      &crypto_type_, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT, &crypto_type_,
      media_crypto_session.GetAddressOf());
  if (FAILED(hresult)) {
    DLOG(ERROR) << "Failed to create a crypto session: " << hresult;
    failed();
    return;
  }

  // Don't do CheckCryptoSessionStatus() yet. The status may be something like
  // CONTEXT_LOST because GetDataForNewHardwareKey() is not called yet.
  uint64_t output_data = 0;
  if (!input_data.empty()) {
    hresult = video_context1_->GetDataForNewHardwareKey(
        media_crypto_session.Get(), input_data.size(), input_data.data(),
        &output_data);
    if (FAILED(hresult)) {
      DLOG(ERROR) << "Failed to establish hardware session: " << hresult;
      failed();
      return;
    }
  }

  D3D11_CRYPTO_SESSION_STATUS crypto_session_status = {};
  hresult = video_context1_->CheckCryptoSessionStatus(
      media_crypto_session.Get(), &crypto_session_status);
  if (FAILED(hresult) ||
      crypto_session_status != D3D11_CRYPTO_SESSION_STATUS_OK) {
    DLOG(ERROR) << "Crypto session is not OK. Crypto session status "
                << crypto_session_status << ". HRESULT " << hresult;
    failed();
    return;
  }

  const uint32_t media_crypto_session_id = next_crypto_session_id_++;
  crypto_session_map_[media_crypto_session_id] =
      std::move(media_crypto_session);
  std::move(create_media_crypto_session_cb)
      .Run(Status::kOk, media_crypto_session_id, output_data);
}

void D3D11CdmProxy::SetKey(uint32_t crypto_session_id,
                           const std::vector<uint8_t>& key_id,
                           KeyType key_type,
                           const std::vector<uint8_t>& key_blob,
                           SetKeyCB set_key_cb) {
  auto crypto_session_it = crypto_session_map_.find(crypto_session_id);
  if (crypto_session_it == crypto_session_map_.end()) {
    DLOG(WARNING) << crypto_session_id
                  << " did not map to a crypto session instance.";
    std::move(set_key_cb).Run(Status::kFail);
    return;
  }

  cdm_context_->SetKey(crypto_session_it->second.Get(), key_id, key_type,
                       key_blob);
  std::move(set_key_cb).Run(Status::kOk);
}

void D3D11CdmProxy::RemoveKey(uint32_t crypto_session_id,
                              const std::vector<uint8_t>& key_id,
                              RemoveKeyCB remove_key_cb) {
  auto crypto_session_it = crypto_session_map_.find(crypto_session_id);
  if (crypto_session_it == crypto_session_map_.end()) {
    DLOG(WARNING) << crypto_session_id
                  << " did not map to a crypto session instance.";
    std::move(remove_key_cb).Run(Status::kFail);
    return;
  }

  cdm_context_->RemoveKey(crypto_session_it->second.Get(), key_id);
  std::move(remove_key_cb).Run(Status::kOk);
}

void D3D11CdmProxy::SetCreateDeviceCallbackForTesting(
    D3D11CreateDeviceCB callback) {
  create_device_func_ = std::move(callback);
}

void D3D11CdmProxy::NotifyHardwareContentProtectionTeardown() {
  cdm_context_->OnHardwareReset();
  client_->NotifyHardwareReset();
  Reset();
}

void D3D11CdmProxy::Reset() {
  client_ = nullptr;
  initialized_ = false;
  crypto_session_map_.clear();
  device_.Reset();
  device_context_.Reset();
  video_device_.Reset();
  video_device1_.Reset();
  video_context_.Reset();
  video_context1_.Reset();
  // Note that this deregisters hardware reset event watcher. It shouldn't
  // notify the clients until this is reinitialized. Also the client is set to
  // null in this method.
  hardware_event_watcher_ = nullptr;
  crypto_session_map_.clear();
  private_input_size_ = 0;
  private_output_size_ = 0;
}

D3D11CdmProxy::HardwareEventWatcher::~HardwareEventWatcher() {
  StopWatching();
}

std::unique_ptr<D3D11CdmProxy::HardwareEventWatcher>
D3D11CdmProxy::HardwareEventWatcher::Create(
    ComD3D11Device device,
    base::RepeatingClosure teardown_callback) {
  std::unique_ptr<HardwareEventWatcher> event_watcher = base::WrapUnique(
      new HardwareEventWatcher(device, std::move(teardown_callback)));
  if (!event_watcher->StartWatching())
    return nullptr;
  return event_watcher;
}

D3D11CdmProxy::HardwareEventWatcher::HardwareEventWatcher(
    ComD3D11Device device,
    base::RepeatingClosure teardown_callback)
    : device_(device), teardown_callback_(std::move(teardown_callback)) {}

bool D3D11CdmProxy::HardwareEventWatcher::StartWatching() {
  if (!RegisterPowerEvents() ||
      !RegisterHardwareContentProtectionTeardown(device_)) {
    StopWatching();
    return false;
  }

  return true;
}

bool D3D11CdmProxy::HardwareEventWatcher::
    RegisterHardwareContentProtectionTeardown(ComD3D11Device device) {
  device_ = device;
  HRESULT hresult = device_.CopyTo(dxgi_device_.ReleaseAndGetAddressOf());
  if (FAILED(hresult)) {
    DVLOG(1) << "Failed to get dxgi device from device: "
             << logging::SystemErrorCodeToString(hresult);
    return false;
  }

  hresult = dxgi_device_->GetParent(
      IID_PPV_ARGS(dxgi_adapter_.ReleaseAndGetAddressOf()));
  if (FAILED(hresult)) {
    DVLOG(1) << "Failed to get dxgi adapter from dxgi device: "
             << logging::SystemErrorCodeToString(hresult);
    return false;
  }

  if (!teardown_status_watcher_.StartWatchingOnce(
          content_protection_teardown_event_.handle(), this)) {
    DVLOG(1) << "Failed to watch tear down event.";
    return false;
  }

  hresult = dxgi_adapter_->RegisterHardwareContentProtectionTeardownStatusEvent(
      content_protection_teardown_event_.handle(), &teardown_event_cookie_);
  if (FAILED(hresult)) {
    DVLOG(1)
        << "Failed to register for HardwareContentProtectionTeardownStatus: "
        << logging::SystemErrorCodeToString(hresult);
    return false;
  }

  return true;
}

bool D3D11CdmProxy::HardwareEventWatcher::RegisterPowerEvents() {
  if (!base::PowerMonitor::AddObserver(this)) {
    DVLOG(1) << "Power monitor not available.";
    return false;
  }
  return true;
}

void D3D11CdmProxy::HardwareEventWatcher::OnObjectSignaled(HANDLE object) {
  DCHECK_EQ(object, content_protection_teardown_event_.handle());
  teardown_callback_.Run();
}

void D3D11CdmProxy::HardwareEventWatcher::OnResume() {
  teardown_callback_.Run();
}

void D3D11CdmProxy::HardwareEventWatcher::StopWatching() {
  if (dxgi_adapter_) {
    dxgi_adapter_->UnregisterHardwareContentProtectionTeardownStatus(
        teardown_event_cookie_);
  }
  teardown_status_watcher_.StopWatching();
  base::PowerMonitor::RemoveObserver(this);
}

}  // namespace media
