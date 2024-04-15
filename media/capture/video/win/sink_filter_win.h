// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implement a DirectShow sink filter used for receiving captured frames from
// a DirectShow Capture filter.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_SINK_FILTER_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_SINK_FILTER_WIN_H_

#include <windows.h>

#include <stddef.h>

#include "base/memory/scoped_refptr.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/win/filter_base_win.h"
#include "media/capture/video/win/sink_filter_observer_win.h"
#include "media/capture/video_capture_types.h"

namespace media {

// Define GUID for I420. This is the color format we would like to support but
// it is not defined in the DirectShow SDK.
// http://msdn.microsoft.com/en-us/library/dd757532.aspx
// 30323449-0000-0010-8000-00AA00389B71.
const GUID kMediaSubTypeI420 = {
    0x30323449,
    0x0000,
    0x0010,
    {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

// UYVY synonym with BT709 color components, used in HD video. This variation
// might appear in non-USB capture cards and it's implemented as a normal YUV
// pixel format with the characters HDYC encoded in the first array word.
const GUID kMediaSubTypeHDYC = {
    0x43594448,
    0x0000,
    0x0010,
    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

// 16-bit grey-scale single plane formats provided by some depth cameras.
const GUID kMediaSubTypeZ16 = {
    0x2036315a,
    0x0000,
    0x0010,
    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID kMediaSubTypeINVZ = {
    0x5a564e49,
    0x2d90,
    0x4a58,
    {0x92, 0x0b, 0x77, 0x3f, 0x1f, 0x2c, 0x55, 0x6b}};
const GUID kMediaSubTypeY16 = {
    0x20363159,
    0x0000,
    0x0010,
    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

class SinkInputPin;

class __declspec(uuid("88cdbbdc-a73b-4afa-acbf-15d5e2ce12c3")) SinkFilter
    : public FilterBase {
 public:
  SinkFilter() = delete;

  explicit SinkFilter(SinkFilterObserver* observer);

  SinkFilter(const SinkFilter&) = delete;
  SinkFilter& operator=(const SinkFilter&) = delete;

  void SetRequestedMediaFormat(VideoPixelFormat pixel_format,
                               float frame_rate,
                               const BITMAPINFOHEADER& info_header);

  // Implement FilterBase.
  size_t NoOfPins() override;
  IPin* GetPin(int index) override;

  IFACEMETHODIMP GetClassID(CLSID* clsid) override;

 private:
  ~SinkFilter() override;

  scoped_refptr<SinkInputPin> input_pin_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_SINK_FILTER_WIN_H_
