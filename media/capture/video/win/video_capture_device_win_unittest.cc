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
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "base/win/scoped_com_initializer.h"
#include "media/base/video_types.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "media/capture/video/win/pin_base_win.h"
#include "media/capture/video/win/sink_filter_observer_win.h"
#include "media/capture/video/win/sink_filter_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

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
constexpr size_t kRgb32FrameBytes = kTestWidth * kTestHeight * 4;
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

void PopulateRgb32MediaType(AM_MEDIA_TYPE* media_type,
                            VIDEOINFOHEADER* video_info) {
  PopulateI420MediaType(media_type, video_info);
  video_info->bmiHeader.biBitCount = 32;
  video_info->bmiHeader.biCompression = BI_RGB;
  video_info->bmiHeader.biSizeImage = static_cast<DWORD>(kRgb32FrameBytes);
  media_type->subtype = MEDIASUBTYPE_RGB32;
  media_type->lSampleSize = static_cast<ULONG>(kRgb32FrameBytes);
}

class FakeMediaSample final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMediaSample> {
 public:
  FakeMediaSample(size_t size, uint8_t fill, base::OnceClosure on_get_time)
      : buffer_(size, fill), on_get_time_(std::move(on_get_time)) {}

  // IMediaSample
  IFACEMETHODIMP GetPointer(BYTE** buffer) override {
    *buffer = buffer_.data();
    return S_OK;
  }
  IFACEMETHODIMP_(long) GetSize() override {
    return static_cast<long>(buffer_.size());
  }
  IFACEMETHODIMP GetTime(REFERENCE_TIME* start, REFERENCE_TIME* end) override {
    if (on_get_time_) {
      std::move(on_get_time_).Run();
    }
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
  base::OnceClosure on_get_time_;
};

// Delivers a sample to |pin|. |on_get_time| runs when the pin queries the
// sample's time, i.e. while the pin is inside Receive().
HRESULT DeliverSampleTo(IPin* pin,
                        size_t size = kI420FrameBytes,
                        uint8_t fill = 0x5A,
                        base::OnceClosure on_get_time = {}) {
  ComPtr<IMemInputPin> mem_input_pin;
  HRESULT hr = pin->QueryInterface(IID_PPV_ARGS(&mem_input_pin));
  if (FAILED(hr)) {
    return hr;
  }
  ComPtr<FakeMediaSample> sample =
      Make<FakeMediaSample>(size, fill, std::move(on_get_time));
  return mem_input_pin->Receive(sample.Get());
}

class FakeCaptureFilter;

// Output pin of FakeCaptureFilter. Offers I420 and RGB32.
class FakeOutputPin final : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
                                                IPin,
                                                IAMStreamConfig,
                                                IKsPropertySet> {
 public:
  explicit FakeOutputPin(FakeCaptureFilter* filter) : filter_(filter) {
    base::AutoLock lock(lock_);
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
    AM_MEDIA_TYPE type;
    VIDEOINFOHEADER video_info;
    {
      base::AutoLock lock(lock_);
      type = media_type_;
      video_info = video_info_;
    }
    type.pbFormat = reinterpret_cast<BYTE*>(&video_info);
    HRESULT hr = receive_pin->ReceiveConnection(this, &type);
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
  IFACEMETHODIMP SetFormat(AM_MEDIA_TYPE* media_type) override {
    if (media_type->formattype != FORMAT_VideoInfo ||
        media_type->cbFormat < sizeof(VIDEOINFOHEADER)) {
      return E_INVALIDARG;
    }
    base::AutoLock lock(lock_);
    video_info_ = *reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
    media_type_ = *media_type;
    media_type_.cbFormat = sizeof(VIDEOINFOHEADER);
    media_type_.pbFormat = reinterpret_cast<BYTE*>(&video_info_);
    return S_OK;
  }
  IFACEMETHODIMP GetFormat(AM_MEDIA_TYPE** media_type) override {
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetNumberOfCapabilities(int* count, int* size) override {
    *count = 2;
    *size = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
  }
  IFACEMETHODIMP GetStreamCaps(int index,
                               AM_MEDIA_TYPE** media_type,
                               BYTE* stream_caps) override {
    if (index != 0 && index != 1) {
      return S_FALSE;
    }
    auto* type =
        static_cast<AM_MEDIA_TYPE*>(::CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE)));
    auto* video_info = static_cast<VIDEOINFOHEADER*>(
        ::CoTaskMemAlloc(sizeof(VIDEOINFOHEADER)));
    if (index == 0) {
      PopulateI420MediaType(type, video_info);
    } else {
      PopulateRgb32MediaType(type, video_info);
    }
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
  VIDEOINFOHEADER video_info_ GUARDED_BY(lock_);
  AM_MEDIA_TYPE media_type_ GUARDED_BY(lock_);
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

// A sample delivery that is still inside SinkInputPin::Receive() when the
// device is torn down.
struct InFlightDelivery {
  // CBaseOutputPin::Deliver() isn't synchronized with BreakConnect(), which
  // releases the output pin's reference to the input pin. Model a delivery
  // that has lost that race: the streaming thread uses the input pin without
  // holding a reference to it.
  raw_ptr<IMemInputPin, DisableDanglingPtrDetection> input_pin;
  // The streaming thread's own reference to the capture filter, which keeps
  // the device's teardown from joining it.
  ComPtr<IBaseFilter> capture_filter;
  // Signaled once the delivery is inside Receive().
  base::WaitableEvent in_receive;
  // The delivery stays inside Receive() until this is signaled, or for at most
  // a short timeout.
  base::WaitableEvent resume;
  // Set by the test once the device is destroyed.
  std::atomic<bool> device_destroyed{false};
  // Whether the device was destroyed while the delivery was inside Receive().
  bool device_destroyed_during_receive = false;
  HRESULT hr = E_UNEXPECTED;
};

void RunInFlightDelivery(InFlightDelivery* delivery) {
  ComPtr<IBaseFilter> capture_filter = std::move(delivery->capture_filter);
  ComPtr<FakeMediaSample> sample = Make<FakeMediaSample>(
      kI420FrameBytes, 0x5A,
      base::BindOnce(
          [](InFlightDelivery* delivery) {
            delivery->in_receive.Signal();
            delivery->resume.TimedWait(TestTimeouts::tiny_timeout());
            delivery->device_destroyed_during_receive =
                delivery->device_destroyed.load();
          },
          base::Unretained(delivery)));
  delivery->hr = delivery->input_pin->Receive(sample.Get());
}

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
  HRESULT pin_info_hr = E_UNEXPECTED;
  HRESULT filter_state_hr = E_UNEXPECTED;
  HRESULT delivery_hr = E_UNEXPECTED;
};

// What a streaming thread might do with an input pin it still points to: query
// the pin and its filter, then deliver a sample.
void UseInputPinLate(IPin* pin, LateDeliveryResults* results) {
  PIN_INFO pin_info = {};
  results->pin_info_hr = pin->QueryPinInfo(&pin_info);
  if (SUCCEEDED(results->pin_info_hr) && pin_info.pFilter) {
    FILTER_STATE state;
    results->filter_state_hr = pin_info.pFilter->GetState(0, &state);
    pin_info.pFilter->Release();
  }
  results->delivery_hr = DeliverSampleTo(pin);
}

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
        [](base::WaitableEvent* start, IPin* pin,
           LateDeliveryResults* results) {
          start->Wait();
          UseInputPinLate(pin, results);
        },
        base::Unretained(&start_late_delivery_),
        base::Unretained(raw_input_pin), base::Unretained(results_.get())));
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
  void set_stop_result(HRESULT hr) { stop_result_ = hr; }
  void set_disconnect_on_remove(bool disconnect) {
    disconnect_on_remove_ = disconnect;
  }

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
    if (disconnect_on_remove_ && SUCCEEDED(filter->EnumPins(&enum_pins))) {
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
    if (FAILED(stop_result_)) {
      return stop_result_;
    }
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
  HRESULT stop_result_ = S_OK;
  bool disconnect_on_remove_ = true;
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

VideoCaptureParams CreateCaptureParams(VideoPixelFormat pixel_format) {
  VideoCaptureParams params;
  params.requested_format = VideoCaptureFormat(
      gfx::Size(kTestWidth, kTestHeight), kTestFrameRate, pixel_format);
  return params;
}

class CountingSinkObserver final : public SinkFilterObserver {
 public:
  void FrameReceived(base::span<const uint8_t> buffer,
                     const VideoCaptureFormat& format,
                     base::TimeDelta timestamp,
                     bool flip_y) override {
    ++frames_received_;
    last_format_ = format;
    last_flip_y_ = flip_y;
  }
  void FrameDropped(VideoCaptureFrameDropReason reason) override {}

  int frames_received() const { return frames_received_; }
  const VideoCaptureFormat& last_format() const { return last_format_; }
  bool last_flip_y() const { return last_flip_y_; }

 private:
  int frames_received_ = 0;
  VideoCaptureFormat last_format_;
  bool last_flip_y_ = false;
};

}  // namespace

class VideoCaptureDeviceWinTest : public ::testing::Test {
 protected:
  std::unique_ptr<VideoCaptureDeviceWin> CreateDevice(
      ComPtr<IBaseFilter> capture_filter) {
    return VideoCaptureDeviceWin::CreateForTesting(
        VideoCaptureDeviceDescriptor("Fake DirectShow camera", "fake_id",
                                     VideoCaptureApi::WIN_DIRECT_SHOW),
        std::move(capture_filter), graph_, Make<FakeCaptureGraphBuilder>());
  }

  base::win::ScopedCOMInitializer com_initializer_;
  base::test::TaskEnvironment task_environment_;
  ComPtr<FakeFilterGraph> graph_ = Make<FakeFilterGraph>();
};

// Regression test for a source filter that keeps delivering samples from its
// streaming thread after Stop() and Disconnect() return, and only joins that
// thread when its last reference is released. The input pin and sink filter
// must stay alive until then, and late samples must not reach the device.
TEST_F(VideoCaptureDeviceWinTest,
       LateDeliveryFromStreamingThreadDuringTeardown) {
  LateDeliveryResults results;
  ComPtr<FakeCaptureFilter> capture_filter = Make<FakeCaptureFilter>(&results);
  std::unique_ptr<VideoCaptureDeviceWin> device = CreateDevice(capture_filter);
  ASSERT_TRUE(device->Init());

  auto client = std::make_unique<NiceMock<MockVideoCaptureDeviceClient>>();
  EXPECT_CALL(*client, OnStarted);
  EXPECT_CALL(*client, OnIncomingCapturedData);
  device->AllocateAndStart(CreateCaptureParams(PIXEL_FORMAT_I420),
                           std::move(client));

  ComPtr<IPin> input_pin = capture_filter->output_pin()->connected_pin();
  ASSERT_TRUE(input_pin);
  EXPECT_EQ(DeliverSampleTo(input_pin.Get()), S_OK);
  input_pin.Reset();

  capture_filter->StartLateDeliveryThread();
  FakeCaptureFilter* raw_capture_filter = capture_filter.Get();
  capture_filter.Reset();
  graph_.Reset();

  device->StopAndDeAllocate();
  EXPECT_FALSE(raw_capture_filter->output_pin()->connected_pin());
  EXPECT_FALSE(results.filter_destroyed);

  // Releases the last reference to the capture filter, which then uses the
  // disconnected input pin and joins its streaming thread.
  device.reset();
  EXPECT_TRUE(results.filter_destroyed);
  EXPECT_EQ(results.pin_info_hr, S_OK);
  EXPECT_EQ(results.filter_state_hr, S_OK);
  EXPECT_EQ(results.delivery_hr, S_FALSE);
}

// If something else still references the capture filter after teardown, its
// streaming thread may outlive the device, so the sink filter and pin must be
// kept alive.
TEST_F(VideoCaptureDeviceWinTest,
       SinkOutlivesExternallyReferencedCaptureFilter) {
  LateDeliveryResults results;
  ComPtr<FakeCaptureFilter> capture_filter = Make<FakeCaptureFilter>(&results);
  std::unique_ptr<VideoCaptureDeviceWin> device = CreateDevice(capture_filter);
  ASSERT_TRUE(device->Init());
  device->AllocateAndStart(
      CreateCaptureParams(PIXEL_FORMAT_I420),
      std::make_unique<NiceMock<MockVideoCaptureDeviceClient>>());
  capture_filter->StartLateDeliveryThread();
  graph_.Reset();

  device->StopAndDeAllocate();
  device.reset();
  EXPECT_FALSE(results.filter_destroyed);

  capture_filter.Reset();
  EXPECT_TRUE(results.filter_destroyed);
  EXPECT_EQ(results.pin_info_hr, S_OK);
  EXPECT_EQ(results.filter_state_hr, S_OK);
  EXPECT_EQ(results.delivery_hr, S_FALSE);
}

// Samples delivered to a pin that is still connected after the device is gone
// must not reach the device.
TEST_F(VideoCaptureDeviceWinTest, DeliveryToStillConnectedPinAfterDestruction) {
  LateDeliveryResults results;
  ComPtr<FakeCaptureFilter> capture_filter = Make<FakeCaptureFilter>(&results);
  std::unique_ptr<VideoCaptureDeviceWin> device = CreateDevice(capture_filter);
  ASSERT_TRUE(device->Init());
  device->AllocateAndStart(
      CreateCaptureParams(PIXEL_FORMAT_I420),
      std::make_unique<NiceMock<MockVideoCaptureDeviceClient>>());
  ComPtr<IPin> input_pin = capture_filter->output_pin()->connected_pin();
  ASSERT_TRUE(input_pin);

  // Destroy the device without StopAndDeAllocate(), with a graph that doesn't
  // disconnect pins on removal. The test's reference to the capture filter
  // keeps the sink filter alive past the device, so only the device's own
  // teardown detaches it.
  graph_->set_disconnect_on_remove(false);
  graph_.Reset();
  device.reset();
  EXPECT_FALSE(results.filter_destroyed);

  // The device disconnected the input pin itself.
  ComPtr<IPin> connected;
  EXPECT_EQ(input_pin->ConnectedTo(&connected), VFW_E_NOT_CONNECTED);

  // Even once connected again, samples don't reach the destroyed device.
  AM_MEDIA_TYPE media_type;
  VIDEOINFOHEADER video_info;
  PopulateI420MediaType(&media_type, &video_info);
  ASSERT_EQ(
      input_pin->ReceiveConnection(capture_filter->output_pin(), &media_type),
      S_OK);
  EXPECT_EQ(DeliverSampleTo(input_pin.Get()), S_FALSE);
  capture_filter.Reset();
  EXPECT_TRUE(results.filter_destroyed);
}

TEST_F(VideoCaptureDeviceWinTest, AllocateDeliverAndRestartWithOtherFormat) {
  LateDeliveryResults results;
  ComPtr<FakeCaptureFilter> capture_filter = Make<FakeCaptureFilter>(&results);
  std::unique_ptr<VideoCaptureDeviceWin> device = CreateDevice(capture_filter);
  ASSERT_TRUE(device->Init());

  struct Session {
    VideoPixelFormat pixel_format;
    size_t frame_bytes;
    bool flip_y;
  };
  for (const Session& session :
       {Session{PIXEL_FORMAT_ARGB, kRgb32FrameBytes, true},
        Session{PIXEL_FORMAT_I420, kI420FrameBytes, false}}) {
    SCOPED_TRACE(VideoPixelFormatToString(session.pixel_format));
    auto client = std::make_unique<NiceMock<MockVideoCaptureDeviceClient>>();
    EXPECT_CALL(*client, OnStarted);
    EXPECT_CALL(*client, OnIncomingCapturedData)
        .WillOnce([&](base::span<const uint8_t> data,
                      const VideoCaptureFormat& format, const gfx::ColorSpace&,
                      int, bool flip_y, base::TimeTicks, base::TimeDelta,
                      std::optional<base::TimeTicks>,
                      const std::optional<VideoFrameMetadata>&, int) {
          EXPECT_EQ(data.size(), session.frame_bytes);
          EXPECT_EQ(data[0], 0x21);
          EXPECT_EQ(format.frame_size, gfx::Size(kTestWidth, kTestHeight));
          EXPECT_EQ(format.pixel_format, session.pixel_format);
          EXPECT_EQ(flip_y, session.flip_y);
        });
    device->AllocateAndStart(CreateCaptureParams(session.pixel_format),
                             std::move(client));

    ComPtr<IPin> input_pin = capture_filter->output_pin()->connected_pin();
    ASSERT_TRUE(input_pin);
    EXPECT_EQ(DeliverSampleTo(input_pin.Get(), session.frame_bytes, 0x21),
              S_OK);

    device->StopAndDeAllocate();
    EXPECT_FALSE(capture_filter->output_pin()->connected_pin());
    // Samples arriving after disconnection are dropped.
    EXPECT_EQ(DeliverSampleTo(input_pin.Get(), session.frame_bytes), S_FALSE);
  }
}

// A source filter whose Stop() doesn't wait for an in-flight Receive(), and
// whose streaming thread holds its own reference to the filter, can still be
// inside SinkInputPin::Receive() while the device is stopped and destroyed.
// Teardown must wait for that call instead of freeing the input pin and the
// device under it. Covers both a successful and a failing Stop().
TEST_F(VideoCaptureDeviceWinTest, InFlightReceiveDuringStopAndDestruction) {
  for (const bool stop_fails : {false, true}) {
    SCOPED_TRACE(stop_fails ? "Stop() fails" : "Stop() succeeds");
    graph_ = Make<FakeFilterGraph>();
    LateDeliveryResults results;
    ComPtr<FakeCaptureFilter> capture_filter =
        Make<FakeCaptureFilter>(&results);
    std::unique_ptr<VideoCaptureDeviceWin> device =
        CreateDevice(capture_filter);
    ASSERT_TRUE(device->Init());
    device->AllocateAndStart(
        CreateCaptureParams(PIXEL_FORMAT_I420),
        std::make_unique<NiceMock<MockVideoCaptureDeviceClient>>());

    InFlightDelivery delivery;
    {
      ComPtr<IPin> input_pin = capture_filter->output_pin()->connected_pin();
      ASSERT_TRUE(input_pin);
      ComPtr<IMemInputPin> mem_input_pin;
      ASSERT_HRESULT_SUCCEEDED(input_pin.As(&mem_input_pin));
      delivery.input_pin = mem_input_pin.Get();
    }
    delivery.capture_filter = capture_filter;
    ClosureSimpleThread streaming_thread(
        base::BindOnce(&RunInFlightDelivery, base::Unretained(&delivery)));
    streaming_thread.Start();
    ASSERT_TRUE(delivery.in_receive.TimedWait(TestTimeouts::action_timeout()));

    graph_->set_stop_result(stop_fails ? E_FAIL : S_OK);
    device->StopAndDeAllocate();
    capture_filter.Reset();
    graph_.Reset();
    // With the fix, this blocks until the delivery stops waiting for |resume|
    // (tiny_timeout()). Without it, a regression is only caught if teardown
    // finishes within that timeout, which may not hold on a very slow bot.
    device.reset();
    delivery.device_destroyed = true;
    delivery.resume.Signal();
    streaming_thread.Join();
    EXPECT_FALSE(delivery.device_destroyed_during_receive);
    EXPECT_HRESULT_SUCCEEDED(delivery.hr);
    EXPECT_TRUE(results.filter_destroyed);
  }
}

TEST_F(VideoCaptureDeviceWinTest, RetainedInputPinAfterDeviceDestruction) {
  LateDeliveryResults results;
  ComPtr<FakeCaptureFilter> capture_filter = Make<FakeCaptureFilter>(&results);
  std::unique_ptr<VideoCaptureDeviceWin> device = CreateDevice(capture_filter);
  ASSERT_TRUE(device->Init());
  device->AllocateAndStart(
      CreateCaptureParams(PIXEL_FORMAT_I420),
      std::make_unique<NiceMock<MockVideoCaptureDeviceClient>>());

  ComPtr<IPin> retained_input_pin =
      capture_filter->output_pin()->connected_pin();
  ASSERT_TRUE(retained_input_pin);
  capture_filter.Reset();
  // Keep the sink filter alive past the device, so that the pin's pointer to it
  // can only have been cleared by the device's teardown.
  PIN_INFO pin_info = {};
  ASSERT_EQ(retained_input_pin->QueryPinInfo(&pin_info), S_OK);
  ComPtr<IBaseFilter> sink_filter;
  sink_filter.Attach(pin_info.pFilter);
  ASSERT_TRUE(sink_filter);

  device->StopAndDeAllocate();
  device.reset();
  EXPECT_TRUE(results.filter_destroyed);

  // A component that still holds the pin must not reach the destroyed device
  // or the sink filter through it.
  EXPECT_EQ(DeliverSampleTo(retained_input_pin.Get()), S_FALSE);
  pin_info = {};
  EXPECT_EQ(retained_input_pin->QueryPinInfo(&pin_info), S_OK);
  EXPECT_EQ(pin_info.pFilter, nullptr);
}

TEST_F(VideoCaptureDeviceWinTest, SinkFilterShutdownDetachesObserver) {
  CountingSinkObserver observer;
  auto sink_filter = base::MakeRefCounted<SinkFilter>(&observer);
  ComPtr<IPin> input_pin = sink_filter->GetPin(0);
  ComPtr<FakeOutputPin> upstream_pin = Make<FakeOutputPin>(nullptr);
  ASSERT_EQ(upstream_pin->Connect(input_pin.Get(), nullptr), S_OK);

  EXPECT_EQ(DeliverSampleTo(input_pin.Get()), S_OK);
  EXPECT_EQ(observer.frames_received(), 1);

  sink_filter->Shutdown();
  EXPECT_EQ(DeliverSampleTo(input_pin.Get()), S_FALSE);
  EXPECT_EQ(observer.frames_received(), 1);

  EXPECT_EQ(upstream_pin->Disconnect(), S_OK);
  EXPECT_EQ(input_pin->Disconnect(), S_OK);
  PIN_INFO pin_info = {};
  EXPECT_EQ(input_pin->QueryPinInfo(&pin_info), S_OK);
  EXPECT_EQ(pin_info.pFilter, sink_filter.get());
  pin_info.pFilter->Release();

  sink_filter->DetachPinOwner();
  pin_info = {};
  EXPECT_EQ(input_pin->QueryPinInfo(&pin_info), S_OK);
  EXPECT_EQ(pin_info.pFilter, nullptr);
}

// Validating a media type doesn't change the negotiated format. While
// connected, further connection attempts are refused, whether or not their
// media type is acceptable, and don't change it either.
TEST_F(VideoCaptureDeviceWinTest, ReconnectionAttemptKeepsFormat) {
  CountingSinkObserver observer;
  auto sink_filter = base::MakeRefCounted<SinkFilter>(&observer);
  ComPtr<IPin> input_pin = sink_filter->GetPin(0);
  PinBase* const pin_base = static_cast<PinBase*>(input_pin.Get());
  ComPtr<FakeOutputPin> upstream_pin = Make<FakeOutputPin>(nullptr);

  AM_MEDIA_TYPE rgb32_type;
  VIDEOINFOHEADER rgb32_info;
  PopulateRgb32MediaType(&rgb32_type, &rgb32_info);

  // Samples are dropped before the first connection, and Connect(), which is
  // only for output pins, doesn't connect.
  EXPECT_EQ(DeliverSampleTo(input_pin.Get(), kRgb32FrameBytes), S_FALSE);
  EXPECT_EQ(input_pin->Connect(upstream_pin.Get(), &rgb32_type), E_UNEXPECTED);
  ComPtr<IPin> connected;
  EXPECT_EQ(input_pin->ConnectedTo(&connected), VFW_E_NOT_CONNECTED);
  EXPECT_EQ(observer.frames_received(), 0);

  // Neither a valid nor a rejected media type is taken before connecting.
  EXPECT_TRUE(pin_base->IsMediaTypeValid(&rgb32_type));
  AM_MEDIA_TYPE unsupported_type;
  VIDEOINFOHEADER unsupported_info;
  PopulateRgb32MediaType(&unsupported_type, &unsupported_info);
  unsupported_type.subtype = GUID_NULL;
  EXPECT_EQ(input_pin->ReceiveConnection(upstream_pin.Get(), &unsupported_type),
            VFW_E_TYPE_NOT_ACCEPTED);
  EXPECT_EQ(DeliverSampleTo(input_pin.Get(), kRgb32FrameBytes), S_FALSE);
  EXPECT_EQ(observer.frames_received(), 0);

  // Null pointers are rejected without connecting.
  EXPECT_EQ(input_pin->ReceiveConnection(nullptr, &rgb32_type), E_POINTER);
  EXPECT_EQ(input_pin->ReceiveConnection(upstream_pin.Get(), nullptr),
            E_POINTER);
  EXPECT_EQ(input_pin->ConnectedTo(&connected), VFW_E_NOT_CONNECTED);
  EXPECT_EQ(DeliverSampleTo(input_pin.Get(), kRgb32FrameBytes), S_FALSE);
  EXPECT_EQ(observer.frames_received(), 0);

  ASSERT_EQ(input_pin->ReceiveConnection(upstream_pin.Get(), &rgb32_type),
            S_OK);

  AM_MEDIA_TYPE i420_type;
  VIDEOINFOHEADER i420_info;
  PopulateI420MediaType(&i420_type, &i420_info);
  i420_info.bmiHeader.biWidth = kTestWidth / 2;
  EXPECT_TRUE(pin_base->IsMediaTypeValid(&i420_type));
  EXPECT_EQ(input_pin->ReceiveConnection(upstream_pin.Get(), &i420_type),
            VFW_E_ALREADY_CONNECTED);
  i420_type.majortype = MEDIATYPE_Audio;
  EXPECT_EQ(input_pin->ReceiveConnection(upstream_pin.Get(), &i420_type),
            VFW_E_ALREADY_CONNECTED);

  EXPECT_EQ(DeliverSampleTo(input_pin.Get(), kRgb32FrameBytes), S_OK);
  EXPECT_EQ(observer.frames_received(), 1);
  EXPECT_EQ(observer.last_format().frame_size,
            gfx::Size(kTestWidth, kTestHeight));
  EXPECT_EQ(observer.last_format().pixel_format, PIXEL_FORMAT_ARGB);
  EXPECT_TRUE(observer.last_flip_y());
  EXPECT_EQ(input_pin->Disconnect(), S_OK);
}

TEST_F(VideoCaptureDeviceWinTest, ConnectionMediaTypeReturnsDeepCopy) {
  CountingSinkObserver observer;
  auto sink_filter = base::MakeRefCounted<SinkFilter>(&observer);
  ComPtr<IPin> input_pin = sink_filter->GetPin(0);
  ComPtr<FakeOutputPin> upstream_pin = Make<FakeOutputPin>(nullptr);

  AM_MEDIA_TYPE media_type;
  VIDEOINFOHEADER video_info;
  PopulateI420MediaType(&media_type, &video_info);
  ASSERT_EQ(input_pin->ReceiveConnection(upstream_pin.Get(), &media_type),
            S_OK);
  // The caller owns |video_info| and may reuse it after the call.
  video_info.bmiHeader.biWidth = 1;

  // The caller owns the returned format block and may modify and free it.
  AM_MEDIA_TYPE connection_type = {};
  ASSERT_EQ(input_pin->ConnectionMediaType(&connection_type), S_OK);
  ASSERT_EQ(connection_type.cbFormat, sizeof(VIDEOINFOHEADER));
  ASSERT_NE(connection_type.pbFormat, reinterpret_cast<BYTE*>(&video_info));
  auto* returned_info =
      reinterpret_cast<VIDEOINFOHEADER*>(connection_type.pbFormat);
  EXPECT_EQ(returned_info->bmiHeader.biWidth, kTestWidth);
  returned_info->bmiHeader.biWidth = 2;

  // Each call returns a new copy of the pin's own format block.
  AM_MEDIA_TYPE second_type = {};
  ASSERT_EQ(input_pin->ConnectionMediaType(&second_type), S_OK);
  ASSERT_NE(second_type.pbFormat, connection_type.pbFormat);
  EXPECT_EQ(reinterpret_cast<VIDEOINFOHEADER*>(second_type.pbFormat)
                ->bmiHeader.biWidth,
            kTestWidth);
  ::CoTaskMemFree(second_type.pbFormat);
  ::CoTaskMemFree(connection_type.pbFormat);

  // While disconnected, the returned media type is cleared.
  EXPECT_EQ(input_pin->Disconnect(), S_OK);
  connection_type.cbFormat = 1;
  EXPECT_EQ(input_pin->ConnectionMediaType(&connection_type),
            VFW_E_NOT_CONNECTED);
  EXPECT_EQ(connection_type.cbFormat, 0u);
  EXPECT_EQ(connection_type.pbFormat, nullptr);
}

}  // namespace media
