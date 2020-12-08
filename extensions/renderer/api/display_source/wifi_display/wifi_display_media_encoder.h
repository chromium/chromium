// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_ENCODER_H_
#define EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_ENCODER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_info.h"

namespace extensions {

// This structure represents an encoded media unit such as a video frame or
// a number of audio frames.
struct WiFiDisplayEncodedUnit {
  WiFiDisplayEncodedUnit(std::string data,
                         base::TimeTicks reference_timestamp,
                         bool key_frame);
  WiFiDisplayEncodedUnit(std::string data,
                         base::TimeTicks reference_timestamp,
                         base::TimeTicks encode_timestamp,
                         bool key_frame);

  const uint8_t* bytes() const {
    return reinterpret_cast<const uint8_t*>(data.data());
  }
  size_t size() const { return data.size(); }

  std::string data;
  base::TimeTicks pts;  // Presentation timestamp.
  base::TimeTicks dts;  // Decoder timestamp.
  bool key_frame;

  DISALLOW_ASSIGN(WiFiDisplayEncodedUnit);
  DISALLOW_COPY(WiFiDisplayEncodedUnit);
};

// This interface is a base class for audio and video encoders used by the
// Wi-Fi Display media pipeline.
// Threading: the client code should belong to a single thread.
class WiFiDisplayMediaEncoder
    : public base::RefCountedThreadSafe<WiFiDisplayMediaEncoder> {
 public:
  using EncodedUnitCallback =
      base::OnceCallback<void(std::unique_ptr<WiFiDisplayEncodedUnit>)>;

  // Creates an elementary stream info describing the stream of encoded units
  // which this encoder generates and passes to a callback set using
  // |SetCallbacks|. The created elementary stream info should be passed to
  // a Wi-Fi Display media packetizer.
  virtual WiFiDisplayElementaryStreamInfo CreateElementaryStreamInfo()
      const = 0;

  // Sets callbacks for the obtained encoder instance:
  // |encoded_callback| is invoked to return the next encoded unit
  // |error_callback| is invoked to report a fatal encoder error
  void SetCallbacks(EncodedUnitCallback encoded_callback,
                    base::OnceClosure error_callback);

 protected:
  friend class base::RefCountedThreadSafe<WiFiDisplayMediaEncoder>;

  WiFiDisplayMediaEncoder();
  virtual ~WiFiDisplayMediaEncoder();

  base::ThreadChecker client_thread_checker_;
  EncodedUnitCallback encoded_callback_;
  base::OnceClosure error_callback_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_DISPLAY_SOURCE_WIFI_DISPLAY_WIFI_DISPLAY_MEDIA_ENCODER_H_
