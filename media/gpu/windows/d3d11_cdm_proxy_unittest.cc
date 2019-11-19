// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_cdm_proxy.h"

#include <d3d11.h>
#include <d3d11_1.h>
#include <initguid.h>

#include "base/bind.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/callback_registry.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/cdm/cdm_proxy_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using Microsoft::WRL::ComPtr;

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Lt;
using ::testing::Mock;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::WithArgs;

namespace media {

namespace {

// TODO(rkuroiwa): Although inheriting from different classes, there are several
// mock CdmProxy clients already. They all have NotifyHardwareReset(), so share
// a single mock class that inherits from all the CdmProxy client classes.
class MockProxyClient : public CdmProxy::Client {
 public:
  MOCK_METHOD0(NotifyHardwareReset, void());
};

class MockPowerMonitorSource : public base::PowerMonitorSource {
 public:
  // Use this method to send a power resume event.
  void Resume() {
    // Due to how ProcessPowerEvent() works, it has to be suspended first to
    // resume.
    ProcessPowerEvent(SUSPEND_EVENT);
    ProcessPowerEvent(RESUME_EVENT);
  }

  MOCK_METHOD0(IsOnBatteryPowerImpl, bool());
};

// The values doesn't matter as long as this is consistently used thruout the
// test.
const CdmProxy::Protocol kTestProtocol = CdmProxy::Protocol::kIntel;
const CdmProxy::Function kTestFunction =
    CdmProxy::Function::kIntelNegotiateCryptoSessionKeyExchange;
// TODO(rkuroiwa): Add test cases for KeyType.
const CdmProxy::KeyType kTestKeyType = CdmProxy::KeyType::kDecryptOnly;
const uint32_t kTestFunctionId = 123;
// clang-format off
DEFINE_GUID(CRYPTO_TYPE_GUID,
            0x01020304, 0xffee, 0xefba,
            0x93, 0xaa, 0x47, 0x77, 0x43, 0xb1, 0x22, 0x98);
// clang-format on

}  // namespace

// Class for mocking the callbacks that get passed to the proxy methods.
class CallbackMock {
 public:
  MOCK_METHOD3(InitializeCallback, CdmProxy::InitializeCB::RunType);
  MOCK_METHOD2(ProcessCallback, CdmProxy::ProcessCB::RunType);
  MOCK_METHOD3(CreateMediaCryptoSessionCallback,
               CdmProxy::CreateMediaCryptoSessionCB::RunType);
  MOCK_METHOD1(SetKeyCallback, CdmProxy::SetKeyCB::RunType);
  MOCK_METHOD1(RemoveKeyCallback, CdmProxy::RemoveKeyCB::RunType);
};

class D3D11CdmProxyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::map<CdmProxy::Function, uint32_t> function_id_map;
    function_id_map[kTestFunction] = kTestFunctionId;

    // Use NiceMock because we don't care about base::PowerMonitorSource events
    // other than calling Resume() directly.
    auto mock_power_monitor_source =
        std::make_unique<NiceMock<MockPowerMonitorSource>>();
    mock_power_monitor_source_ = mock_power_monitor_source.get();
    base::PowerMonitor::Initialize(std::move(mock_power_monitor_source));

    proxy_ = std::make_unique<D3D11CdmProxy>(CRYPTO_TYPE_GUID, kTestProtocol,
                                             function_id_map);

    device_mock_ = CreateD3D11Mock<D3D11DeviceMock>();
    video_device_mock_ = CreateD3D11Mock<D3D11VideoDeviceMock>();
    video_device1_mock_ = CreateD3D11Mock<D3D11VideoDevice1Mock>();
    crypto_session_mock_ = CreateD3D11Mock<D3D11CryptoSessionMock>();
    device_context_mock_ = CreateD3D11Mock<D3D11DeviceContextMock>();
    video_context_mock_ = CreateD3D11Mock<D3D11VideoContextMock>();
    video_context1_mock_ = CreateD3D11Mock<D3D11VideoContext1Mock>();
    dxgi_device_ = CreateD3D11Mock<DXGIDevice2Mock>();
    dxgi_adapter_ = CreateD3D11Mock<NiceMock<DXGIAdapter3Mock>>();

    // These flags are a reasonable subset of flags to get HARDWARE protected
    // playback.
    content_protection_caps_.Caps =
        D3D11_CONTENT_PROTECTION_CAPS_HARDWARE |
        D3D11_CONTENT_PROTECTION_CAPS_HARDWARE_PROTECT_UNCOMPRESSED |
        D3D11_CONTENT_PROTECTION_CAPS_HARDWARE_PROTECTED_MEMORY_PAGEABLE |
        D3D11_CONTENT_PROTECTION_CAPS_HARDWARE_TEARDOWN |
        D3D11_CONTENT_PROTECTION_CAPS_HARDWARE_DRM_COMMUNICATION;
    // 1 for the mock behavior below for CheckCryptoKeyExchange().
    content_protection_caps_.KeyExchangeTypeCount = 1;
    // This is arbitrary but 1 is reasonable, meaning doesn't need to be
    // aligned.
    content_protection_caps_.BlockAlignmentSize = 1;
    // This value is arbitrary.
    content_protection_caps_.ProtectedMemorySize = 10000000;

    OnCallsForInitialize();

    proxy_->SetCreateDeviceCallbackForTesting(
        base::BindRepeating(&D3D11CreateDeviceMock::Create,
                            base::Unretained(&create_device_mock_)));
  }

  void TearDown() override {
    proxy_.reset();
    base::PowerMonitor::ShutdownForTesting();
  }

  // Sets up ON_CALLs for the mock objects. These can be overriden with
  // EXPECT_CALLs.
  // |content_protection_caps_| should be set.
  void OnCallsForInitialize() {
    ON_CALL(create_device_mock_,
            Create(_, D3D_DRIVER_TYPE_HARDWARE, _, _, _, _, _, _, _, _))
        .WillByDefault(
            DoAll(SetComPointee<7>(device_mock_.Get()),
                  SetComPointeeAndReturnOk<9>(device_context_mock_.Get())));

    COM_ON_CALL(device_mock_, QueryInterface(IID_ID3D11VideoDevice, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(video_device_mock_.Get()));

    COM_ON_CALL(device_mock_, QueryInterface(IID_ID3D11VideoDevice1, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(video_device1_mock_.Get()));

    COM_ON_CALL(device_mock_, QueryInterface(IID_IDXGIDevice2, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(dxgi_device_.Get()));

    COM_ON_CALL(dxgi_device_, GetParent(IID_IDXGIAdapter3, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(dxgi_adapter_.Get()));

    COM_ON_CALL(dxgi_adapter_,
                RegisterHardwareContentProtectionTeardownStatusEvent(_, _))
        .WillByDefault(DoAll(SaveArg<0>(&teardown_event_), Return(S_OK)));

    COM_ON_CALL(device_context_mock_, QueryInterface(IID_ID3D11VideoContext, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(video_context_mock_.Get()));

    COM_ON_CALL(device_context_mock_,
                QueryInterface(IID_ID3D11VideoContext1, _))
        .WillByDefault(SetComPointeeAndReturnOk<1>(video_context1_mock_.Get()));

    COM_ON_CALL(
        video_device_mock_,
        CreateCryptoSession(Pointee(CRYPTO_TYPE_GUID), _,
                            Pointee(D3D11_KEY_EXCHANGE_HW_PROTECTION), _))
        .WillByDefault(SetComPointeeAndReturnOk<3>(crypto_session_mock_.Get()));

    COM_ON_CALL(video_device1_mock_, GetCryptoSessionPrivateDataSize(
                                         Pointee(CRYPTO_TYPE_GUID), _, _, _, _))
        .WillByDefault(DoAll(SetArgPointee<3>(kPrivateInputSize),
                             SetArgPointee<4>(kPrivateOutputSize),
                             Return(S_OK)));

    COM_ON_CALL(video_device_mock_, GetContentProtectionCaps(_, _, _))
        .WillByDefault(
            DoAll(SetArgPointee<2>(content_protection_caps_), Return(S_OK)));

    COM_ON_CALL(video_device_mock_, CheckCryptoKeyExchange(_, _, Lt(1u), _))
        .WillByDefault(DoAll(SetArgPointee<3>(D3D11_KEY_EXCHANGE_HW_PROTECTION),
                             Return(S_OK)));
  }

  // Helper method to do Initialize(). The returned mock objects are accessible
  // thru member variables.
  void Initialize(CdmProxy::Client* client, CdmProxy::InitializeCB callback) {
    EXPECT_CALL(create_device_mock_,
                Create(_, D3D_DRIVER_TYPE_HARDWARE, _, _, _, _, _, _, _, _));
    COM_EXPECT_CALL(device_mock_, QueryInterface(IID_ID3D11VideoDevice, _))
        .Times(AtLeast(1));
    COM_EXPECT_CALL(device_mock_, QueryInterface(IID_IDXGIDevice2, _))
        .Times(AtLeast(1));
    COM_EXPECT_CALL(dxgi_device_, GetParent(IID_IDXGIAdapter3, _))
        .Times(AtLeast(1));
    COM_EXPECT_CALL(dxgi_adapter_,
                    RegisterHardwareContentProtectionTeardownStatusEvent(_, _))
        .Times(AtLeast(1));
    COM_EXPECT_CALL(device_mock_, QueryInterface(IID_ID3D11VideoDevice1, _))
        .Times(AtLeast(1));
    COM_EXPECT_CALL(device_context_mock_,
                    QueryInterface(IID_ID3D11VideoContext, _))
        .Times(AtLeast(1));
    COM_EXPECT_CALL(device_context_mock_,
                    QueryInterface(IID_ID3D11VideoContext1, _))
        .Times(AtLeast(1));
    COM_EXPECT_CALL(
        video_device_mock_,
        CreateCryptoSession(Pointee(CRYPTO_TYPE_GUID), _,
                            Pointee(D3D11_KEY_EXCHANGE_HW_PROTECTION), _));
    COM_EXPECT_CALL(
        video_device1_mock_,
        GetCryptoSessionPrivateDataSize(Pointee(CRYPTO_TYPE_GUID), _, _, _, _));

    COM_EXPECT_CALL(video_device_mock_, GetContentProtectionCaps(_, _, _));

    COM_EXPECT_CALL(video_device_mock_,
                    CheckCryptoKeyExchange(_, _, Lt(1u), _));

    proxy_->Initialize(client, std::move(callback));

    Mock::VerifyAndClearExpectations(device_mock_.Get());
    Mock::VerifyAndClearExpectations(video_device_mock_.Get());
    Mock::VerifyAndClearExpectations(video_device1_mock_.Get());
    Mock::VerifyAndClearExpectations(crypto_session_mock_.Get());
    Mock::VerifyAndClearExpectations(device_context_mock_.Get());
    Mock::VerifyAndClearExpectations(video_context_mock_.Get());
    Mock::VerifyAndClearExpectations(video_context1_mock_.Get());
  }

  // Test case where the proxy is initialized and then hardware content
  // protection teardown is notified.
  void HardwareContentProtectionTeardown() {
    base::RunLoop run_loop;

    EXPECT_CALL(callback_mock_,
                InitializeCallback(CdmProxy::Status::kOk, _, _));
    ASSERT_NO_FATAL_FAILURE(Initialize(
        &client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                 base::Unretained(&callback_mock_))));

    EXPECT_CALL(client_, NotifyHardwareReset());

    base::MockCallback<CdmContext::EventCB> event_cb;
    auto callback_registration =
        proxy_->GetCdmContext()->RegisterEventCB(event_cb.Get());
    EXPECT_CALL(event_cb, Run(CdmContext::Event::kHardwareContextLost))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

    SetEvent(teardown_event_);
    run_loop.Run();
  }

  MockProxyClient client_;
  std::unique_ptr<D3D11CdmProxy> proxy_;
  // Owned by PowerMonitor. Use this to simulate a power-resume.
  MockPowerMonitorSource* mock_power_monitor_source_;

  D3D11CreateDeviceMock create_device_mock_;
  CallbackMock callback_mock_;

  ComPtr<D3D11DeviceMock> device_mock_;
  ComPtr<D3D11VideoDeviceMock> video_device_mock_;
  ComPtr<D3D11VideoDevice1Mock> video_device1_mock_;
  ComPtr<D3D11CryptoSessionMock> crypto_session_mock_;
  ComPtr<D3D11DeviceContextMock> device_context_mock_;
  ComPtr<D3D11VideoContextMock> video_context_mock_;
  ComPtr<D3D11VideoContext1Mock> video_context1_mock_;
  ComPtr<DXGIDevice2Mock> dxgi_device_;
  ComPtr<NiceMock<DXGIAdapter3Mock>> dxgi_adapter_;

  D3D11_VIDEO_CONTENT_PROTECTION_CAPS content_protection_caps_ = {};

  // Event captured in Initialize(). Used in tests to notify hardware content
  // protection teardown.
  HANDLE teardown_event_;

  // These size values are arbitrary. Used for mocking
  // GetCryptoSessionPrivateDataSize().
  const UINT kPrivateInputSize = 10;
  const UINT kPrivateOutputSize = 40;

  // ObjectWatcher uses SequencedTaskRunnerHandle.
  base::test::TaskEnvironment task_environment_;
};

// Verifies that if device creation fails, then the call fails.
TEST_F(D3D11CdmProxyTest, FailedToCreateDevice) {
  EXPECT_CALL(create_device_mock_, Create(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(E_FAIL));
  EXPECT_CALL(callback_mock_,
              InitializeCallback(CdmProxy::Status::kFail, _, _));
  proxy_->Initialize(&client_,
                     base::BindOnce(&CallbackMock::InitializeCallback,
                                    base::Unretained(&callback_mock_)));
}

// Initialize() success case.
TEST_F(D3D11CdmProxyTest, Initialize) {
  EXPECT_CALL(callback_mock_, InitializeCallback(CdmProxy::Status::kOk, _, _));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));
}

// Hardware content protection teardown is notified to the proxy.
// Verify that the client is notified.
TEST_F(D3D11CdmProxyTest, HardwareContentProtectionTeardown) {
  EXPECT_NO_FATAL_FAILURE(HardwareContentProtectionTeardown());
}

// Verify that initialization after hardware content protection teardown works..
TEST_F(D3D11CdmProxyTest, HardwareContentProtectionTeardownThenInitialize) {
  ASSERT_NO_FATAL_FAILURE(HardwareContentProtectionTeardown());
  EXPECT_CALL(callback_mock_, InitializeCallback(CdmProxy::Status::kOk, _, _));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));
}

// Verify that failing to register to hardware content protection teardown
// status event results in initialization failure.
TEST_F(D3D11CdmProxyTest, FailedToRegisterForContentProtectionTeardown) {
  EXPECT_CALL(callback_mock_,
              InitializeCallback(CdmProxy::Status::kFail, _, _));

  COM_EXPECT_CALL(dxgi_adapter_,
                  RegisterHardwareContentProtectionTeardownStatusEvent(_, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(E_FAIL));

  proxy_->Initialize(&client_,
                     base::BindOnce(&CallbackMock::InitializeCallback,
                                    base::Unretained(&callback_mock_)));
}

// Verify that the client is notified on power suspend.
TEST_F(D3D11CdmProxyTest, PowerResume) {
  base::RunLoop run_loop;

  EXPECT_CALL(callback_mock_, InitializeCallback(CdmProxy::Status::kOk, _, _));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));

  EXPECT_CALL(client_, NotifyHardwareReset()).WillOnce(Invoke([&run_loop]() {
    run_loop.Quit();
  }));

  mock_power_monitor_source_->Resume();
  run_loop.Run();
}

// IRL power resume is notified and then hardware content protection teardown
// is notified. Make sure that the two notifications don't signal the clients
// more than once (without being reinitialized in between the notifications).
// Note that this test uses QuitWhenIdle(). If both notifications are processed
// this test will run forever.
TEST_F(D3D11CdmProxyTest, PowerResumeAndHardwareContentProtectionTeardown) {
  base::RunLoop run_loop;

  EXPECT_CALL(callback_mock_, InitializeCallback(CdmProxy::Status::kOk, _, _));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));

  EXPECT_CALL(client_, NotifyHardwareReset())
      .Times(1)
      .WillOnce(Invoke([&run_loop]() { run_loop.QuitWhenIdle(); }));

  mock_power_monitor_source_->Resume();
  SetEvent(teardown_event_);
  run_loop.Run();
}

// Verify that if there isn't a power monitor, initialization fails.
TEST_F(D3D11CdmProxyTest, NoPowerMonitor) {
  base::PowerMonitor::ShutdownForTesting();
  EXPECT_CALL(callback_mock_,
              InitializeCallback(CdmProxy::Status::kFail, _, _));

  proxy_->Initialize(&client_,
                     base::BindOnce(&CallbackMock::InitializeCallback,
                                    base::Unretained(&callback_mock_)));
}

// Initialization failure because HW key exchange is not available.
TEST_F(D3D11CdmProxyTest, NoHwKeyExchange) {
  EXPECT_CALL(callback_mock_,
              InitializeCallback(CdmProxy::Status::kFail, _, _));
  // GUID is set to non-D3D11_KEY_EXCHANGE_HW_PROTECTION, which means no HW key
  // exchange.
  COM_EXPECT_CALL(video_device_mock_, CheckCryptoKeyExchange(_, _, Lt(1u), _))
      .WillOnce(
          DoAll(SetArgPointee<3>(D3D11_CRYPTO_TYPE_AES128_CTR), Return(S_OK)));

  proxy_->Initialize(&client_,
                     base::BindOnce(&CallbackMock::InitializeCallback,
                                    base::Unretained(&callback_mock_)));
}

// Verifies that Process() won't work if not initialized.
TEST_F(D3D11CdmProxyTest, ProcessUninitialized) {
  // The size nor value here matter, so making non empty non zero vector.
  const std::vector<uint8_t> kAnyInput(16, 0xFF);
  // Output size is also arbitrary, just has to match with the mock.
  const uint32_t kExpectedOutputDataSize = 20;
  EXPECT_CALL(callback_mock_, ProcessCallback(CdmProxy::Status::kFail, _));
  proxy_->Process(kTestFunction, 0, kAnyInput, kExpectedOutputDataSize,
                  base::BindOnce(&CallbackMock::ProcessCallback,
                                 base::Unretained(&callback_mock_)));
}

// Verifies that using a crypto session that is not reported will fail.
TEST_F(D3D11CdmProxyTest, ProcessInvalidCryptoSessionID) {
  uint32_t crypto_session_id = 0;
  EXPECT_CALL(callback_mock_, InitializeCallback(CdmProxy::Status::kOk, _, _))
      .WillOnce(SaveArg<2>(&crypto_session_id));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));
  Mock::VerifyAndClearExpectations(&callback_mock_);

  // The size nor value here matter, so making non empty non zero vector.
  const std::vector<uint8_t> kAnyInput(16, 0xFF);
  // Output size is also arbitrary, just has to match with the mock.
  const uint32_t kExpectedOutputDataSize = 20;
  EXPECT_CALL(callback_mock_, ProcessCallback(CdmProxy::Status::kFail, _));

  // Use a crypto session ID that hasn't been reported.
  proxy_->Process(kTestFunction, crypto_session_id + 1, kAnyInput,
                  kExpectedOutputDataSize,
                  base::BindOnce(&CallbackMock::ProcessCallback,
                                 base::Unretained(&callback_mock_)));
}

// Matcher for checking whether the structure passed to
// NegotiateCryptoSessionKeyExchange has the expected values.
MATCHER_P2(MatchesKeyExchangeStructure, expected, input_struct_size, "") {
  D3D11_KEY_EXCHANGE_HW_PROTECTION_DATA* actual =
      static_cast<D3D11_KEY_EXCHANGE_HW_PROTECTION_DATA*>(arg);
  if (expected->HWProtectionFunctionID != actual->HWProtectionFunctionID) {
    *result_listener << "function IDs mismatch. Expected "
                     << expected->HWProtectionFunctionID << " actual "
                     << actual->HWProtectionFunctionID;
    return false;
  }
  D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA* expected_input_data =
      expected->pInputData;
  D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA* actual_input_data =
      actual->pInputData;
  if (memcmp(expected_input_data, actual_input_data, input_struct_size) != 0) {
    *result_listener
        << "D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA don't match.";
    return false;
  }
  D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA* expected_output_data =
      expected->pOutputData;
  D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA* actual_output_data =
      actual->pOutputData;
  // Don't check that pbOutput field. It's filled by the callee.
  if (expected_output_data->PrivateDataSize !=
      actual_output_data->PrivateDataSize) {
    *result_listener << "D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA::"
                        "PrivateDataSize don't match. Expected "
                     << expected_output_data->PrivateDataSize << " actual "
                     << actual_output_data->PrivateDataSize;
    return false;
  }
  if (expected_output_data->HWProtectionDataSize !=
      actual_output_data->HWProtectionDataSize) {
    *result_listener << "D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA::"
                        "HWProtectionDataSize don't match. Expected "
                     << expected_output_data->HWProtectionDataSize << " actual "
                     << actual_output_data->HWProtectionDataSize;
    return false;
  }
  if (expected_output_data->TransportTime !=
      actual_output_data->TransportTime) {
    *result_listener << "D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA::"
                        "TransportTime don't match. Expected "
                     << expected_output_data->TransportTime << " actual "
                     << actual_output_data->TransportTime;
    return false;
  }
  if (expected_output_data->ExecutionTime !=
      actual_output_data->ExecutionTime) {
    *result_listener << "D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA::"
                        "ExecutionTime don't match. Expected "
                     << expected_output_data->ExecutionTime << " actual "
                     << actual_output_data->ExecutionTime;
    return false;
  }
  if (expected_output_data->MaxHWProtectionDataSize !=
      actual_output_data->MaxHWProtectionDataSize) {
    *result_listener << "D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA::"
                        "MaxHWProtectionDataSize don't match. Expected "
                     << expected_output_data->MaxHWProtectionDataSize
                     << " actual "
                     << actual_output_data->MaxHWProtectionDataSize;
    return false;
  }
  return true;
}

// Verifies that Process() works.
TEST_F(D3D11CdmProxyTest, Process) {
  uint32_t crypto_session_id = 0;
  EXPECT_CALL(callback_mock_,
              InitializeCallback(CdmProxy::Status::kOk, kTestProtocol, _))
      .WillOnce(SaveArg<2>(&crypto_session_id));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));
  Mock::VerifyAndClearExpectations(&callback_mock_);

  // The size nor value here matter, so making non empty non zero vector.
  const std::vector<uint8_t> kAnyInput(16, 0xFF);
  // Output size is also arbitrary, just has to match with the mock.
  const uint32_t kExpectedOutputDataSize = 20;

  const uint32_t input_structure_size =
      sizeof(D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA) - 4 +
      kAnyInput.size();
  const uint32_t output_structure_size =
      sizeof(D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA) - 4 +
      kExpectedOutputDataSize;
  std::unique_ptr<uint8_t[]> input_data_raw(new uint8_t[input_structure_size]);
  std::unique_ptr<uint8_t[]> output_data_raw(
      new uint8_t[output_structure_size]);

  D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA* input_data =
      reinterpret_cast<D3D11_KEY_EXCHANGE_HW_PROTECTION_INPUT_DATA*>(
          input_data_raw.get());
  D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA* output_data =
      reinterpret_cast<D3D11_KEY_EXCHANGE_HW_PROTECTION_OUTPUT_DATA*>(
          output_data_raw.get());

  D3D11_KEY_EXCHANGE_HW_PROTECTION_DATA expected_key_exchange_data = {};
  expected_key_exchange_data.HWProtectionFunctionID = kTestFunctionId;
  expected_key_exchange_data.pInputData = input_data;
  expected_key_exchange_data.pOutputData = output_data;
  input_data->PrivateDataSize = kPrivateInputSize;
  input_data->HWProtectionDataSize = 0;
  memcpy(input_data->pbInput, kAnyInput.data(), kAnyInput.size());

  output_data->PrivateDataSize = kPrivateOutputSize;
  output_data->HWProtectionDataSize = 0;
  output_data->TransportTime = 0;
  output_data->ExecutionTime = 0;
  output_data->MaxHWProtectionDataSize = kExpectedOutputDataSize;

  // The value does not matter, so making non zero vector.
  std::vector<uint8_t> test_output_data(kExpectedOutputDataSize, 0xAA);
  EXPECT_CALL(callback_mock_,
              ProcessCallback(CdmProxy::Status::kOk, test_output_data));

  auto set_test_output_data = [&test_output_data](void* output) {
    D3D11_KEY_EXCHANGE_HW_PROTECTION_DATA* kex_struct =
        static_cast<D3D11_KEY_EXCHANGE_HW_PROTECTION_DATA*>(output);
    memcpy(kex_struct->pOutputData->pbOutput, test_output_data.data(),
           test_output_data.size());
  };

  COM_EXPECT_CALL(video_context_mock_,
                  NegotiateCryptoSessionKeyExchange(
                      _, sizeof(expected_key_exchange_data),
                      MatchesKeyExchangeStructure(&expected_key_exchange_data,
                                                  input_structure_size)))
      .WillOnce(DoAll(WithArgs<2>(Invoke(set_test_output_data)), Return(S_OK)));

  proxy_->Process(kTestFunction, crypto_session_id, kAnyInput,
                  kExpectedOutputDataSize,
                  base::BindOnce(&CallbackMock::ProcessCallback,
                                 base::Unretained(&callback_mock_)));
}

TEST_F(D3D11CdmProxyTest, CreateMediaCryptoSessionUninitialized) {
  // The size nor value here matter, so making non empty non zero vector.
  const std::vector<uint8_t> kAnyInput(16, 0xFF);
  EXPECT_CALL(callback_mock_,
              CreateMediaCryptoSessionCallback(CdmProxy::Status::kFail, _, _));
  proxy_->CreateMediaCryptoSession(
      kAnyInput, base::BindOnce(&CallbackMock::CreateMediaCryptoSessionCallback,
                                base::Unretained(&callback_mock_)));
}

// Tests the case where no extra data is specified. This is a success case.
TEST_F(D3D11CdmProxyTest, CreateMediaCryptoSessionNoExtraData) {
  uint32_t crypto_session_id_from_initialize = 0;
  EXPECT_CALL(callback_mock_,
              InitializeCallback(CdmProxy::Status::kOk, kTestProtocol, _))
      .WillOnce(SaveArg<2>(&crypto_session_id_from_initialize));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));
  Mock::VerifyAndClearExpectations(&callback_mock_);

  // Expect a new crypto session.
  EXPECT_CALL(callback_mock_, CreateMediaCryptoSessionCallback(
                                  CdmProxy::Status::kOk,
                                  Ne(crypto_session_id_from_initialize), _));
  auto media_crypto_session_mock = CreateD3D11Mock<D3D11CryptoSessionMock>();
  COM_EXPECT_CALL(video_device_mock_,
                  CreateCryptoSession(Pointee(CRYPTO_TYPE_GUID), _,
                                      Pointee(CRYPTO_TYPE_GUID), _))
      .WillOnce(SetComPointeeAndReturnOk<3>(media_crypto_session_mock.Get()));

  COM_EXPECT_CALL(video_context1_mock_, GetDataForNewHardwareKey(_, _, _, _))
      .Times(0);

  COM_EXPECT_CALL(video_context1_mock_,
                  CheckCryptoSessionStatus(media_crypto_session_mock.Get(), _))
      .WillOnce(DoAll(SetArgPointee<1>(D3D11_CRYPTO_SESSION_STATUS_OK),
                      Return(S_OK)));
  proxy_->CreateMediaCryptoSession(
      std::vector<uint8_t>(),
      base::BindOnce(&CallbackMock::CreateMediaCryptoSessionCallback,
                     base::Unretained(&callback_mock_)));
}

// |arg| is void*. This casts the pointer to uint8_t* and checks whether they
// match.
MATCHER_P(CastedToUint8Are, expected, "") {
  const uint8_t* actual = static_cast<const uint8_t*>(arg);
  for (size_t i = 0; i < expected.size(); ++i) {
    if (actual[i] != expected[i]) {
      *result_listener << "Mismatch at element " << i;
      return false;
    }
  }
  return true;
}

// Verifies that extra data is used when creating a media crypto session.
TEST_F(D3D11CdmProxyTest, CreateMediaCryptoSessionWithExtraData) {
  uint32_t crypto_session_id_from_initialize = 0;
  EXPECT_CALL(callback_mock_,
              InitializeCallback(CdmProxy::Status::kOk, kTestProtocol, _))
      .WillOnce(SaveArg<2>(&crypto_session_id_from_initialize));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));
  Mock::VerifyAndClearExpectations(&callback_mock_);

  // Expect a new crypto session.
  EXPECT_CALL(callback_mock_, CreateMediaCryptoSessionCallback(
                                  CdmProxy::Status::kOk,
                                  Ne(crypto_session_id_from_initialize), _));

  auto media_crypto_session_mock = CreateD3D11Mock<D3D11CryptoSessionMock>();
  COM_EXPECT_CALL(video_device_mock_,
                  CreateCryptoSession(Pointee(CRYPTO_TYPE_GUID), _,
                                      Pointee(CRYPTO_TYPE_GUID), _))
      .WillOnce(SetComPointeeAndReturnOk<3>(media_crypto_session_mock.Get()));
  // The size nor value here matter, so making non empty non zero vector.
  const std::vector<uint8_t> kAnyInput(16, 0xFF);
  const uint64_t kAnyOutputData = 23298u;
  COM_EXPECT_CALL(video_context1_mock_,
                  GetDataForNewHardwareKey(media_crypto_session_mock.Get(),
                                           kAnyInput.size(),
                                           CastedToUint8Are(kAnyInput), _))
      .WillOnce(DoAll(SetArgPointee<3>(kAnyOutputData), Return(S_OK)));

  COM_EXPECT_CALL(video_context1_mock_,
                  CheckCryptoSessionStatus(media_crypto_session_mock.Get(), _))
      .WillOnce(DoAll(SetArgPointee<1>(D3D11_CRYPTO_SESSION_STATUS_OK),
                      Return(S_OK)));
  proxy_->CreateMediaCryptoSession(
      kAnyInput, base::BindOnce(&CallbackMock::CreateMediaCryptoSessionCallback,
                                base::Unretained(&callback_mock_)));
}

// Verify that GetCdmContext() is implemented and does not return null.
TEST_F(D3D11CdmProxyTest, GetCdmContext) {
  base::WeakPtr<CdmContext> context = proxy_->GetCdmContext();
  ASSERT_TRUE(context);
}

TEST_F(D3D11CdmProxyTest, GetCdmProxyContext) {
  base::WeakPtr<CdmContext> context = proxy_->GetCdmContext();
  ASSERT_TRUE(context);
  ASSERT_TRUE(context->GetCdmProxyContext());
}

// No keys are set.
TEST_F(D3D11CdmProxyTest, GetD3D11DecryptContextNoKey) {
  base::WeakPtr<CdmContext> context = proxy_->GetCdmContext();
  ASSERT_TRUE(context);
  CdmProxyContext* proxy_context = context->GetCdmProxyContext();
  auto decrypt_context =
      proxy_context->GetD3D11DecryptContext(kTestKeyType, "");
  EXPECT_FALSE(decrypt_context);
}

// A key is set but no keys for the key type requested.
TEST_F(D3D11CdmProxyTest, GetD3D11DecryptContextNoKeyForKeyType) {
  uint32_t crypto_session_id_from_initialize = 0;
  EXPECT_CALL(callback_mock_,
              InitializeCallback(CdmProxy::Status::kOk, kTestProtocol, _))
      .WillOnce(SaveArg<2>(&crypto_session_id_from_initialize));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));
  Mock::VerifyAndClearExpectations(&callback_mock_);

  const std::vector<uint8_t> kAnyBlob = {0x01, 0x4f, 0x83};

  EXPECT_CALL(callback_mock_, SetKeyCallback(CdmProxy::Status::kOk));
  proxy_->SetKey(crypto_session_id_from_initialize, kAnyBlob,
                 CdmProxy::KeyType::kDecryptAndDecode, kAnyBlob,
                 base::BindOnce(&CallbackMock::SetKeyCallback,
                                base::Unretained(&callback_mock_)));

  base::WeakPtr<CdmContext> context = proxy_->GetCdmContext();
  CdmProxyContext* proxy_context = context->GetCdmProxyContext();
  auto decrypt_context = proxy_context->GetD3D11DecryptContext(
      CdmProxy::KeyType::kDecryptOnly,
      std::string(kAnyBlob.begin(), kAnyBlob.end()));
  EXPECT_FALSE(decrypt_context);
}

// Verifies that keys are set and is accessible with a getter.
TEST_F(D3D11CdmProxyTest, SetKeyAndGetDecryptContext) {
  base::WeakPtr<CdmContext> context = proxy_->GetCdmContext();
  ASSERT_TRUE(context);
  CdmProxyContext* proxy_context = context->GetCdmProxyContext();

  uint32_t crypto_session_id_from_initialize = 0;
  EXPECT_CALL(callback_mock_,
              InitializeCallback(CdmProxy::Status::kOk, kTestProtocol, _))
      .WillOnce(SaveArg<2>(&crypto_session_id_from_initialize));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));
  Mock::VerifyAndClearExpectations(&callback_mock_);

  std::vector<uint8_t> kKeyId = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  };
  std::vector<uint8_t> kKeyBlob = {
      0xab, 0x01, 0x20, 0xd3, 0xee, 0x05, 0x99, 0x87,
      0xff, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x7F,
  };

  base::MockCallback<CdmContext::EventCB> event_cb;
  auto callback_registration = context->RegisterEventCB(event_cb.Get());
  EXPECT_CALL(event_cb, Run(CdmContext::Event::kHasAdditionalUsableKey));

  EXPECT_CALL(callback_mock_, SetKeyCallback(CdmProxy::Status::kOk));
  proxy_->SetKey(crypto_session_id_from_initialize, kKeyId, kTestKeyType,
                 kKeyBlob,
                 base::BindOnce(&CallbackMock::SetKeyCallback,
                                base::Unretained(&callback_mock_)));

  // |event_cb| is posted. Run the loop to make sure it's fired.
  base::RunLoop().RunUntilIdle();

  std::string key_id_str(kKeyId.begin(), kKeyId.end());
  auto decrypt_context =
      proxy_context->GetD3D11DecryptContext(kTestKeyType, key_id_str);
  ASSERT_TRUE(decrypt_context);

  EXPECT_TRUE(decrypt_context->crypto_session)
      << "Crypto session should not be null.";
  const uint8_t* key_blob =
      reinterpret_cast<const uint8_t*>(decrypt_context->key_blob);
  EXPECT_EQ(kKeyBlob, std::vector<uint8_t>(
                          key_blob, key_blob + decrypt_context->key_blob_size));
  EXPECT_EQ(CRYPTO_TYPE_GUID, decrypt_context->key_info_guid);
}

// Verify that the keys are not accessible via CdmProxyContext, after a
// teardown..
TEST_F(D3D11CdmProxyTest, ClearKeysAfterHardwareContentProtectionTeardown) {
  base::RunLoop run_loop;

  uint32_t crypto_session_id_from_initialize = 0;
  EXPECT_CALL(callback_mock_,
              InitializeCallback(CdmProxy::Status::kOk, kTestProtocol, _))
      .WillOnce(SaveArg<2>(&crypto_session_id_from_initialize));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));
  Mock::VerifyAndClearExpectations(&callback_mock_);

  std::vector<uint8_t> kKeyId = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  };
  std::vector<uint8_t> kKeyBlob = {
      0xab, 0x01, 0x20, 0xd3, 0xee, 0x05, 0x99, 0x87,
      0xff, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x7F,
  };
  EXPECT_CALL(callback_mock_, SetKeyCallback(CdmProxy::Status::kOk));
  proxy_->SetKey(crypto_session_id_from_initialize, kKeyId, kTestKeyType,
                 kKeyBlob,
                 base::BindOnce(&CallbackMock::SetKeyCallback,
                                base::Unretained(&callback_mock_)));

  EXPECT_CALL(client_, NotifyHardwareReset()).WillOnce(Invoke([&run_loop]() {
    run_loop.Quit();
  }));

  SetEvent(teardown_event_);
  run_loop.Run();

  base::WeakPtr<CdmContext> context = proxy_->GetCdmContext();
  ASSERT_TRUE(context);
  CdmProxyContext* proxy_context = context->GetCdmProxyContext();

  std::string key_id_str(kKeyId.begin(), kKeyId.end());
  auto decrypt_context =
      proxy_context->GetD3D11DecryptContext(kTestKeyType, key_id_str);
  ASSERT_FALSE(decrypt_context);
}

// Verify that removing a key works.
TEST_F(D3D11CdmProxyTest, RemoveKey) {
  base::WeakPtr<CdmContext> context = proxy_->GetCdmContext();
  ASSERT_TRUE(context);
  CdmProxyContext* proxy_context = context->GetCdmProxyContext();

  uint32_t crypto_session_id_from_initialize = 0;
  EXPECT_CALL(callback_mock_,
              InitializeCallback(CdmProxy::Status::kOk, kTestProtocol, _))
      .WillOnce(SaveArg<2>(&crypto_session_id_from_initialize));
  ASSERT_NO_FATAL_FAILURE(
      Initialize(&client_, base::BindOnce(&CallbackMock::InitializeCallback,
                                          base::Unretained(&callback_mock_))));
  Mock::VerifyAndClearExpectations(&callback_mock_);

  std::vector<uint8_t> kKeyId = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  };
  std::vector<uint8_t> kKeyBlob = {
      0xab, 0x01, 0x20, 0xd3, 0xee, 0x05, 0x99, 0x87,
      0xff, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x7F,
  };
  EXPECT_CALL(callback_mock_, SetKeyCallback(CdmProxy::Status::kOk));
  EXPECT_CALL(callback_mock_, RemoveKeyCallback(CdmProxy::Status::kOk));
  proxy_->SetKey(crypto_session_id_from_initialize, kKeyId, kTestKeyType,
                 kKeyBlob,
                 base::BindOnce(&CallbackMock::SetKeyCallback,
                                base::Unretained(&callback_mock_)));
  proxy_->RemoveKey(crypto_session_id_from_initialize, kKeyId,
                    base::BindOnce(&CallbackMock::RemoveKeyCallback,
                                   base::Unretained(&callback_mock_)));

  std::string keyblob_str(kKeyId.begin(), kKeyId.end());
  auto decrypt_context =
      proxy_context->GetD3D11DecryptContext(kTestKeyType, keyblob_str);
  EXPECT_FALSE(decrypt_context);
}

// Calling SetKey() and RemoveKey() for non-existent crypto session should
// fail but not crash.
TEST_F(D3D11CdmProxyTest, SetRemoveKeyWrongCryptoSessionId) {
  const uint32_t kAnyCryptoSessionId = 0x9238;
  const std::vector<uint8_t> kEmpty;
  EXPECT_CALL(callback_mock_, RemoveKeyCallback(CdmProxy::Status::kFail));
  EXPECT_CALL(callback_mock_, SetKeyCallback(CdmProxy::Status::kFail));
  proxy_->RemoveKey(kAnyCryptoSessionId, kEmpty,
                    base::BindOnce(&CallbackMock::RemoveKeyCallback,
                                   base::Unretained(&callback_mock_)));
  proxy_->SetKey(kAnyCryptoSessionId, kEmpty, kTestKeyType, kEmpty,
                 base::BindOnce(&CallbackMock::SetKeyCallback,
                                base::Unretained(&callback_mock_)));
}

TEST_F(D3D11CdmProxyTest, ProxyInvalidationInvalidatesCdmContext) {
  base::WeakPtr<CdmContext> context = proxy_->GetCdmContext();
  EXPECT_TRUE(context);
  proxy_.reset();
  EXPECT_FALSE(context);
}

}  // namespace media
