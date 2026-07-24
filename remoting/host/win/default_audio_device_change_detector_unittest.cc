// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/default_audio_device_change_detector.h"

#include <mmdeviceapi.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class FakeMMDeviceEnumerator final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMMDeviceEnumerator> {
 public:
  FakeMMDeviceEnumerator() = default;

  FakeMMDeviceEnumerator(const FakeMMDeviceEnumerator&) = delete;
  FakeMMDeviceEnumerator& operator=(const FakeMMDeviceEnumerator&) = delete;

  ~FakeMMDeviceEnumerator() override = default;

  Microsoft::WRL::ComPtr<IMMNotificationClient> client() const {
    return client_;
  }

  // IMMDeviceEnumerator implementation.
  HRESULT __stdcall EnumAudioEndpoints(
      EDataFlow dataFlow,
      DWORD dwStateMask,
      IMMDeviceCollection** ppDevices) override;
  HRESULT __stdcall GetDefaultAudioEndpoint(EDataFlow dataFlow,
                                            ERole role,
                                            IMMDevice** ppEndpoint) override;
  HRESULT __stdcall GetDevice(LPCWSTR pwstrId, IMMDevice** ppDevice) override;
  HRESULT __stdcall RegisterEndpointNotificationCallback(
      IMMNotificationClient* pClient) override;
  HRESULT __stdcall UnregisterEndpointNotificationCallback(
      IMMNotificationClient* pClient) override;

 private:
  Microsoft::WRL::ComPtr<IMMNotificationClient> client_;
};

HRESULT FakeMMDeviceEnumerator::EnumAudioEndpoints(
    EDataFlow dataFlow,
    DWORD dwStateMask,
    IMMDeviceCollection** ppDevices) {
  return E_NOTIMPL;
}

HRESULT FakeMMDeviceEnumerator::GetDefaultAudioEndpoint(
    EDataFlow dataFlow,
    ERole role,
    IMMDevice** ppEndpoint) {
  return E_NOTIMPL;
}

HRESULT FakeMMDeviceEnumerator::GetDevice(LPCWSTR pwstrId,
                                          IMMDevice** ppDevice) {
  return E_NOTIMPL;
}

HRESULT FakeMMDeviceEnumerator::RegisterEndpointNotificationCallback(
    IMMNotificationClient* pClient) {
  client_ = pClient;
  return S_OK;
}

HRESULT FakeMMDeviceEnumerator::UnregisterEndpointNotificationCallback(
    IMMNotificationClient* pClient) {
  if (client_.Get() != pClient) {
    return E_INVALIDARG;
  }
  client_.Reset();
  return S_OK;
}

}  // namespace

class DefaultAudioDeviceChangeDetectorTest : public testing::Test {
 public:
  DefaultAudioDeviceChangeDetectorTest() = default;
  ~DefaultAudioDeviceChangeDetectorTest() override = default;

 protected:
  base::win::ScopedCOMInitializer com_init_;
};

TEST_F(DefaultAudioDeviceChangeDetectorTest, RegistersAndUnregisters) {
  auto enumerator = Microsoft::WRL::Make<FakeMMDeviceEnumerator>();
  ASSERT_FALSE(enumerator->client());

  auto detector =
      Microsoft::WRL::Make<DefaultAudioDeviceChangeDetector>(enumerator);
  EXPECT_EQ(enumerator->client().Get(),
            static_cast<IMMNotificationClient*>(detector.Get()));

  detector->Unregister();
  EXPECT_FALSE(enumerator->client());

  // A second call is a no-op.
  detector->Unregister();
}

TEST_F(DefaultAudioDeviceChangeDetectorTest, GetAndReset) {
  auto enumerator = Microsoft::WRL::Make<FakeMMDeviceEnumerator>();
  auto detector =
      Microsoft::WRL::Make<DefaultAudioDeviceChangeDetector>(enumerator);

  EXPECT_FALSE(detector->GetAndReset());

  enumerator->client()->OnDefaultDeviceChanged(eRender, eConsole, nullptr);
  EXPECT_TRUE(detector->GetAndReset());
  EXPECT_FALSE(detector->GetAndReset());

  detector->Unregister();
}

TEST_F(DefaultAudioDeviceChangeDetectorTest,
       OutlivesOwnerWhileSystemHoldsReference) {
  auto enumerator = Microsoft::WRL::Make<FakeMMDeviceEnumerator>();
  auto detector =
      Microsoft::WRL::Make<DefaultAudioDeviceChangeDetector>(enumerator);

  // Simulate the system queuing a notification, which takes an additional
  // reference on the registered client.
  Microsoft::WRL::ComPtr<IMMNotificationClient> pending = enumerator->client();
  ASSERT_TRUE(pending);

  // The owner unregisters the detector and releases its reference, as
  // AudioCapturerWin::Deinitialize() does.
  detector->Unregister();
  detector.Reset();

  // The detector must remain alive while `pending` holds a reference, so a
  // late notification is delivered to a valid object.
  EXPECT_EQ(pending->OnDefaultDeviceChanged(eRender, eConsole, nullptr), S_OK);
  pending.Reset();
}

}  // namespace remoting
