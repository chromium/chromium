// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_device_win.h"

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>
#include <ks.h>
#include <ksmedia.h>
#include <stddef.h>
#include <stdint.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/threading/simple_thread.h"
#include "base/win/scoped_com_initializer.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "media/capture/video/win/sink_filter_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::Microsoft::WRL::ChainInterfaces;
using ::Microsoft::WRL::ClassicCom;
using ::Microsoft::WRL::ComPtr;
using ::Microsoft::WRL::Make;
using ::Microsoft::WRL::RuntimeClass;
using ::Microsoft::WRL::RuntimeClassFlags;
using ::testing::NiceMock;

namespace media {

namespace {

constexpr int kTestWidth = 64;
constexpr int kTestHeight = 48;
constexpr float kTestFrameRate = 30.0f;
constexpr size_t kI420FrameBytes = kTestWidth * kTestHeight * 3 / 2;
constexpr REFERENCE_TIME kReferenceTimePerSecond = 10000000;

void PopulateI420MediaType(AM_MEDIA_TYPE* media_type,
                           VIDEOINFOHEADER* video_info) {
  *media_type = {};
  *video_info = {};
  video_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  video_info->bmiHeader.biWidth = kTestWidth;
  video_info->bmiHeader.biHeight = kTestHeight;
  video_info->bmiHeader.biPlanes = 1;
  video_info->bmiHeader.biBitCount = 12;
  video_info->bmiHeader.biCompression = MAKEFOURCC('I', '4', '2', '0');
  video_info->bmiHeader.biSizeImage = static_cast<DWORD>(kI420FrameBytes);
  video_info->AvgTimePerFrame =
      static_cast<REFERENCE_TIME>(kReferenceTimePerSecond / kTestFrameRate);

  media_type->majortype = MEDIATYPE_Video;
  media_type->subtype = kMediaSubTypeI420;
  media_type->bFixedSizeSamples = TRUE;
  media_type->bTemporalCompression = FALSE;
  media_type->lSampleSize = static_cast<ULONG>(kI420FrameBytes);
  media_type->formattype = FORMAT_VideoInfo;
  media_type->cbFormat = sizeof(VIDEOINFOHEADER);
  media_type->pbFormat = reinterpret_cast<BYTE*>(video_info);
}

class FakeMediaSample final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMediaSample> {
 public:
  explicit FakeMediaSample(size_t size) : buffer_(size, 0x5A) {}

  // IMediaSample
  IFACEMETHODIMP GetPointer(BYTE** buffer) override {
    *buffer = buffer_.data();
    return S_OK;
  }
  IFACEMETHODIMP_(long) GetSize() override {
    return static_cast<long>(buffer_.size());
  }
  IFACEMETHODIMP GetTime(REFERENCE_TIME* start, REFERENCE_TIME* end) override {
    return VFW_E_SAMPLE_TIME_NOT_SET;
  }
  IFACEMETHODIMP SetTime(REFERENCE_TIME* start, REFERENCE_TIME* end) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP IsSyncPoint() override { return S_OK; }
  IFACEMETHODIMP SetSyncPoint(BOOL is_sync_point) override { return E_NOTIMPL; }
  IFACEMETHODIMP IsPreroll() override { return S_FALSE; }
  IFACEMETHODIMP SetPreroll(BOOL is_preroll) override { return E_NOTIMPL; }
  IFACEMETHODIMP_(long) GetActualDataLength() override {
    return static_cast<long>(buffer_.size());
  }
  IFACEMETHODIMP SetActualDataLength(long length) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetMediaType(AM_MEDIA_TYPE** media_type) override {
    return S_FALSE;
  }
  IFACEMETHODIMP SetMediaType(AM_MEDIA_TYPE* media_type) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP IsDiscontinuity() override { return S_FALSE; }
  IFACEMETHODIMP SetDiscontinuity(BOOL is_discontinuity) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetMediaTime(LONGLONG* start, LONGLONG* end) override {
    return VFW_E_MEDIA_TIME_NOT_SET;
  }
  IFACEMETHODIMP SetMediaTime(LONGLONG* start, LONGLONG* end) override {
    return E_NOTIMPL;
  }

 private:
  std::vector<uint8_t> buffer_;
};

HRESULT DeliverSampleTo(IPin* pin) {
  ComPtr<IMemInputPin> mem_input_pin;
  HRESULT hr = pin->QueryInterface(IID_PPV_ARGS(&mem_input_pin));
  if (FAILED(hr)) {
    return hr;
  }
  ComPtr<FakeMediaSample> sample = Make<FakeMediaSample>(kI420FrameBytes);
  return mem_input_pin->Receive(sample.Get());
}

class FakeCaptureFilter;

// Output pin of FakeCaptureFilter. Offers one I420 format.
class FakeOutputPin final : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
                                                IPin,
                                                IAMStreamConfig,
                                                IKsPropertySet> {
 public:
  explicit FakeOutputPin(FakeCaptureFilter* filter) : filter_(filter) {
    PopulateI420MediaType(&media_type_, &video_info_);
  }

  ComPtr<IPin> connected_pin() {
    base::AutoLock lock(lock_);
    return connected_pin_;
  }

  void ClearFilter() { filter_ = nullptr; }

  // IPin
  IFACEMETHODIMP Connect(IPin* receive_pin,
                         const AM_MEDIA_TYPE* media_type) override {
    HRESULT hr = receive_pin->ReceiveConnection(this, &media_type_);
    if (SUCCEEDED(hr)) {
      base::AutoLock lock(lock_);
      connected_pin_ = receive_pin;
    }
    return hr;
  }
  IFACEMETHODIMP ReceiveConnection(IPin* connector,
                                   const AM_MEDIA_TYPE* media_type) override {
    return E_UNEXPECTED;
  }
  IFACEMETHODIMP Disconnect() override;
  IFACEMETHODIMP ConnectedTo(IPin** pin) override {
    base::AutoLock lock(lock_);
    if (!connected_pin_) {
      *pin = nullptr;
      return VFW_E_NOT_CONNECTED;
    }
    return connected_pin_.CopyTo(pin);
  }
  IFACEMETHODIMP ConnectionMediaType(AM_MEDIA_TYPE* media_type) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP QueryPinInfo(PIN_INFO* info) override { return E_NOTIMPL; }
  IFACEMETHODIMP QueryDirection(PIN_DIRECTION* pin_dir) override {
    *pin_dir = PINDIR_OUTPUT;
    return S_OK;
  }
  IFACEMETHODIMP QueryId(LPWSTR* id) override { return E_NOTIMPL; }
  IFACEMETHODIMP QueryAccept(const AM_MEDIA_TYPE* media_type) override {
    return S_OK;
  }
  IFACEMETHODIMP EnumMediaTypes(IEnumMediaTypes** types) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP QueryInternalConnections(IPin** pins,
                                          ULONG* no_pins) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP EndOfStream() override { return S_OK; }
  IFACEMETHODIMP BeginFlush() override { return S_OK; }
  IFACEMETHODIMP EndFlush() override { return S_OK; }
  IFACEMETHODIMP NewSegment(REFERENCE_TIME start,
                            REFERENCE_TIME stop,
                            double rate) override {
    return S_OK;
  }

  // IAMStreamConfig
  IFACEMETHODIMP SetFormat(AM_MEDIA_TYPE* media_type) override { return S_OK; }
  IFACEMETHODIMP GetFormat(AM_MEDIA_TYPE** media_type) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetNumberOfCapabilities(int* count, int* size) override {
    *count = 1;
    *size = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
  }
  IFACEMETHODIMP GetStreamCaps(int index,
                               AM_MEDIA_TYPE** media_type,
                               BYTE* stream_caps) override {
    if (index != 0) {
      return S_FALSE;
    }
    auto* type =
        static_cast<AM_MEDIA_TYPE*>(::CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
    auto* video_info = static_cast<VIDEOINFOHEADER*>(
        ::CoTaskMemAlloc(sizeof(VIDEOINFOHEADER)));
    PopulateI420MediaType(type, video_info);
    *reinterpret_cast<VIDEO_STREAM_CONFIG_CAPS*>(stream_caps) = {};
    *media_type = type;
    return S_OK;
  }

  // IKsPropertySet
  IFACEMETHODIMP Set(REFGUID guid_prop_set,
                     DWORD prop_id,
                     LPVOID instance_data,
                     DWORD cb_instance_data,
                     LPVOID prop_data,
                     DWORD cb_prop_data) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP Get(REFGUID guid_prop_set,
                     DWORD prop_id,
                     LPVOID instance_data,
                     DWORD cb_instance_data,
                     LPVOID prop_data,
                     DWORD cb_prop_data,
                     DWORD* cb_returned) override {
    if (guid_prop_set != AMPROPSETID_Pin ||
        prop_id != AMPROPERTY_PIN_CATEGORY || cb_prop_data < sizeof(GUID)) {
      return E_PROP_ID_UNSUPPORTED;
    }
    *static_cast<GUID*>(prop_data) = PIN_CATEGORY_CAPTURE;
    *cb_returned = sizeof(GUID);
    return S_OK;
  }
  IFACEMETHODIMP QuerySupported(REFGUID guid_prop_set,
                                DWORD prop_id,
                                DWORD* type_support) override {
    return E_NOTIMPL;
  }

 private:
  // Owns |this|.
  raw_ptr<FakeCaptureFilter> filter_;
  base::Lock lock_;
  ComPtr<IPin> connected_pin_ GUARDED_BY(lock_);
  VIDEOINFOHEADER video_info_;
  AM_MEDIA_TYPE media_type_;
};

class FakeEnumPins final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IEnumPins> {
 public:
  explicit FakeEnumPins(ComPtr<IPin> pin) : pin_(std::move(pin)) {}

  IFACEMETHODIMP Next(ULONG count, IPin** pins, ULONG* fetched) override {
    ULONG copied = 0;
    if (count > 0 && !done_) {
      pin_.CopyTo(&pins[0]);
      done_ = true;
      copied = 1;
    }
    if (fetched) {
      *fetched = copied;
    }
    return copied == count ? S_OK : S_FALSE;
  }
  IFACEMETHODIMP Skip(ULONG count) override { return E_NOTIMPL; }
  IFACEMETHODIMP Reset() override {
    done_ = false;
    return S_OK;
  }
  IFACEMETHODIMP Clone(IEnumPins** clone) override { return E_NOTIMPL; }

 private:
  ComPtr<IPin> pin_;
  bool done_ = false;
};

class ClosureSimpleThread final : public base::SimpleThread {
 public:
  explicit ClosureSimpleThread(base::OnceClosure body)
      : base::SimpleThread("FakeDirectShowStreamingThread"),
        body_(std::move(body)) {}

  void Run() override { std::move(body_).Run(); }

 private:
  base::OnceClosure body_;
};

struct LateDeliveryResults {
  bool filter_destroyed = false;
  HRESULT delivery_hr = E_UNEXPECTED;
};

// Models a DirectShow source filter whose Stop() returns without joining its
// streaming thread, and which only joins that thread from its destructor, i.e.
// when its last COM reference is released.
class FakeCaptureFilter final
    : public RuntimeClass<
          RuntimeClassFlags<ClassicCom>,
          ChainInterfaces<IBaseFilter, IMediaFilter, IPersist>> {
 public:
  explicit FakeCaptureFilter(LateDeliveryResults* results)
      : results_(results),
        output_pin_(Make<FakeOutputPin>(this)),
        start_late_delivery_(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  ~FakeCaptureFilter() override {
    if (streaming_thread_) {
      start_late_delivery_.Signal();
      streaming_thread_->Join();
    }
    output_pin_->ClearFilter();
    results_->filter_destroyed = true;
  }

  FakeOutputPin* output_pin() { return output_pin_.Get(); }

  bool is_stopped() const { return state_ == State_Stopped; }

  // Starts a streaming thread that grabs a raw pointer to the connected input
  // pin now, but only delivers a sample to it once the destructor runs, i.e.
  // after Stop() and Disconnect().
  void StartLateDeliveryThread() {
    IPin* raw_input_pin = output_pin_->connected_pin().Get();
    ASSERT_TRUE(raw_input_pin);
    streaming_thread_ = std::make_unique<ClosureSimpleThread>(base::BindOnce(
        [](base::WaitableEvent* start, IPin* pin, HRESULT* delivery_hr) {
          start->Wait();
          *delivery_hr = DeliverSampleTo(pin);
        },
        base::Unretained(&start_late_delivery_),
        base::Unretained(raw_input_pin),
        base::Unretained(&results_->delivery_hr)));
    streaming_thread_->Start();
  }

  // IPersist
  IFACEMETHODIMP GetClassID(CLSID* class_id) override { return E_NOTIMPL; }

  // IMediaFilter
  IFACEMETHODIMP Stop() override {
    state_ = State_Stopped;
    return S_OK;
  }
  IFACEMETHODIMP Pause() override {
    state_ = State_Paused;
    return S_OK;
  }
  IFACEMETHODIMP Run(REFERENCE_TIME start) override {
    state_ = State_Running;
    return S_OK;
  }
  IFACEMETHODIMP GetState(DWORD timeout_ms, FILTER_STATE* state) override {
    *state = state_;
    return S_OK;
  }
  IFACEMETHODIMP SetSyncSource(IReferenceClock* clock) override { return S_OK; }
  IFACEMETHODIMP GetSyncSource(IReferenceClock** clock) override {
    return E_NOTIMPL;
  }

  // IBaseFilter
  IFACEMETHODIMP EnumPins(IEnumPins** enum_pins) override {
    return Make<FakeEnumPins>(output_pin_).CopyTo(enum_pins);
  }
  IFACEMETHODIMP FindPin(LPCWSTR id, IPin** pin) override { return E_NOTIMPL; }
  IFACEMETHODIMP QueryFilterInfo(FILTER_INFO* info) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP JoinFilterGraph(IFilterGraph* graph, LPCWSTR name) override {
    return S_OK;
  }
  IFACEMETHODIMP QueryVendorInfo(LPWSTR* vendor_info) override {
    return E_NOTIMPL;
  }

 private:
  const raw_ptr<LateDeliveryResults> results_;
  ComPtr<FakeOutputPin> output_pin_;
  std::atomic<FILTER_STATE> state_ = State_Stopped;
  base::WaitableEvent start_late_delivery_;
  std::unique_ptr<ClosureSimpleThread> streaming_thread_;
};

STDMETHODIMP FakeOutputPin::Disconnect() {
  // Like real DirectShow pins, refuse to disconnect unless stopped.
  if (filter_ && !filter_->is_stopped()) {
    return VFW_E_NOT_STOPPED;
  }
  ComPtr<IPin> old_pin;
  base::AutoLock lock(lock_);
  if (!connected_pin_) {
    return S_FALSE;
  }
  old_pin = std::move(connected_pin_);
  return S_OK;
}

// Minimal filter graph manager. Like the real one, it holds references to the
// filters it contains and implements IMediaControl on the same object.
class FakeFilterGraph final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
                          ChainInterfaces<IGraphBuilder, IFilterGraph>,
                          ChainInterfaces<IMediaControl, IDispatch>> {
 public:
  // IFilterGraph
  IFACEMETHODIMP AddFilter(IBaseFilter* filter, LPCWSTR name) override {
    filters_.emplace_back(filter);
    return filter->JoinFilterGraph(this, name);
  }
  IFACEMETHODIMP RemoveFilter(IBaseFilter* filter) override {
    auto it = std::ranges::find(filters_, filter, &ComPtr<IBaseFilter>::Get);
    if (it == filters_.end()) {
      return E_FAIL;
    }
    // Like the real graph, disconnect the filter's pins before removing it.
    ComPtr<IEnumPins> enum_pins;
    if (SUCCEEDED(filter->EnumPins(&enum_pins))) {
      ComPtr<IPin> pin;
      while (enum_pins->Next(1, &pin, nullptr) == S_OK) {
        ComPtr<IPin> peer;
        if (SUCCEEDED(pin->ConnectedTo(&peer))) {
          peer->Disconnect();
          pin->Disconnect();
        }
        pin.Reset();
      }
    }
    filter->JoinFilterGraph(nullptr, nullptr);
    filters_.erase(it);
    return S_OK;
  }
  IFACEMETHODIMP EnumFilters(IEnumFilters** enum_filters) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP FindFilterByName(LPCWSTR name, IBaseFilter** filter) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP ConnectDirect(IPin* pin_out,
                               IPin* pin_in,
                               const AM_MEDIA_TYPE* media_type) override {
    return pin_out->Connect(pin_in, media_type);
  }
  IFACEMETHODIMP Reconnect(IPin* pin) override { return E_NOTIMPL; }
  IFACEMETHODIMP Disconnect(IPin* pin) override { return pin->Disconnect(); }
  IFACEMETHODIMP SetDefaultSyncSource() override { return S_OK; }

  // IGraphBuilder
  IFACEMETHODIMP Connect(IPin* pin_out, IPin* pin_in) override {
    return ConnectDirect(pin_out, pin_in, nullptr);
  }
  IFACEMETHODIMP Render(IPin* pin_out) override { return E_NOTIMPL; }
  IFACEMETHODIMP RenderFile(LPCWSTR file, LPCWSTR play_list) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP AddSourceFilter(LPCWSTR file_name,
                                 LPCWSTR filter_name,
                                 IBaseFilter** filter) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP SetLogFile(DWORD_PTR file) override { return E_NOTIMPL; }
  IFACEMETHODIMP Abort() override { return E_NOTIMPL; }
  IFACEMETHODIMP ShouldOperationContinue() override { return S_OK; }

  // IDispatch
  IFACEMETHODIMP GetTypeInfoCount(UINT* count) override { return E_NOTIMPL; }
  IFACEMETHODIMP GetTypeInfo(UINT index, LCID lcid, ITypeInfo** info) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetIDsOfNames(REFIID riid,
                               LPOLESTR* names,
                               UINT count,
                               LCID lcid,
                               DISPID* ids) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP Invoke(DISPID id,
                        REFIID riid,
                        LCID lcid,
                        WORD flags,
                        DISPPARAMS* params,
                        VARIANT* result,
                        EXCEPINFO* exception_info,
                        UINT* arg_error) override {
    return E_NOTIMPL;
  }

  // IMediaControl
  IFACEMETHODIMP Run() override {
    for (auto& filter : filters_) {
      filter->Run(0);
    }
    return S_OK;
  }
  IFACEMETHODIMP Pause() override {
    for (auto& filter : filters_) {
      filter->Pause();
    }
    return S_OK;
  }
  IFACEMETHODIMP Stop() override {
    for (auto& filter : filters_) {
      filter->Stop();
    }
    return S_OK;
  }
  IFACEMETHODIMP GetState(LONG timeout_ms, OAFilterState* state) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP RenderFile(BSTR file_name) override { return E_NOTIMPL; }
  IFACEMETHODIMP AddSourceFilter(BSTR file_name, IDispatch** filter) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_FilterCollection(IDispatch** collection) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_RegFilterCollection(IDispatch** collection) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP StopWhenReady() override { return Stop(); }

 private:
  std::vector<ComPtr<IBaseFilter>> filters_;
};

class FakeCaptureGraphBuilder final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
                          ICaptureGraphBuilder2> {
 public:
  IFACEMETHODIMP SetFiltergraph(IGraphBuilder* graph) override {
    graph_ = graph;
    return S_OK;
  }
  IFACEMETHODIMP GetFiltergraph(IGraphBuilder** graph) override {
    return graph_.CopyTo(graph);
  }
  IFACEMETHODIMP SetOutputFileName(const GUID* type,
                                   LPCOLESTR file,
                                   IBaseFilter** filter,
                                   IFileSinkFilter** sink) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP FindInterface(const GUID* category,
                               const GUID* type,
                               IBaseFilter* filter,
                               REFIID riid,
                               void** interface_ptr) override {
    return E_NOINTERFACE;
  }
  IFACEMETHODIMP RenderStream(const GUID* category,
                              const GUID* type,
                              IUnknown* source,
                              IBaseFilter* compressor,
                              IBaseFilter* renderer) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP ControlStream(const GUID* category,
                               const GUID* type,
                               IBaseFilter* filter,
                               REFERENCE_TIME* start,
                               REFERENCE_TIME* stop,
                               WORD start_cookie,
                               WORD stop_cookie) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP AllocCapFile(LPCOLESTR file, DWORDLONG size) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP CopyCaptureFile(
      LPOLESTR old_file,
      LPOLESTR new_file,
      int allow_esc_abort,
      IAMCopyCaptureFileProgress* callback) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP FindPin(IUnknown* source,
                         PIN_DIRECTION pin_dir,
                         const GUID* category,
                         const GUID* type,
                         BOOL unconnected,
                         int index,
                         IPin** pin) override {
    return E_NOTIMPL;
  }

 private:
  ComPtr<IGraphBuilder> graph_;
};

VideoCaptureParams CreateI420CaptureParams() {
  VideoCaptureParams params;
  params.requested_format = VideoCaptureFormat(
      gfx::Size(kTestWidth, kTestHeight), kTestFrameRate, PIXEL_FORMAT_I420);
  return params;
}

}  // namespace

class VideoCaptureDeviceWinTest : public ::testing::Test {
 protected:
  base::win::ScopedCOMInitializer com_initializer_;
  base::test::TaskEnvironment task_environment_;
};

// Regression test for a source filter that keeps delivering samples from its
// streaming thread after Stop() and Disconnect() return, and only joins that
// thread when its last reference is released. The input pin must stay alive
// until then.
TEST_F(VideoCaptureDeviceWinTest,
       LateDeliveryFromStreamingThreadDuringTeardown) {
  LateDeliveryResults results;
  ComPtr<FakeCaptureFilter> capture_filter = Make<FakeCaptureFilter>(&results);
  std::unique_ptr<VideoCaptureDeviceWin> device =
      VideoCaptureDeviceWin::CreateForTesting(
          VideoCaptureDeviceDescriptor("Fake DirectShow camera", "fake_id",
                                       VideoCaptureApi::WIN_DIRECT_SHOW),
          capture_filter, Make<FakeFilterGraph>(),
          Make<FakeCaptureGraphBuilder>());
  ASSERT_TRUE(device->Init());

  auto client = std::make_unique<NiceMock<MockVideoCaptureDeviceClient>>();
  EXPECT_CALL(*client, OnStarted);
  EXPECT_CALL(*client, OnIncomingCapturedData);
  device->AllocateAndStart(CreateI420CaptureParams(), std::move(client));

  ComPtr<IPin> input_pin = capture_filter->output_pin()->connected_pin();
  ASSERT_TRUE(input_pin);
  EXPECT_EQ(DeliverSampleTo(input_pin.Get()), S_OK);
  input_pin.Reset();

  capture_filter->StartLateDeliveryThread();
  FakeCaptureFilter* raw_capture_filter = capture_filter.Get();
  capture_filter.Reset();

  device->StopAndDeAllocate();
  EXPECT_FALSE(raw_capture_filter->output_pin()->connected_pin());
  EXPECT_FALSE(results.filter_destroyed);

  // Releases the last reference to the capture filter, which then delivers a
  // sample to the disconnected input pin and joins its streaming thread.
  device.reset();
  EXPECT_TRUE(results.filter_destroyed);
  EXPECT_TRUE(SUCCEEDED(results.delivery_hr));
}

}  // namespace media
