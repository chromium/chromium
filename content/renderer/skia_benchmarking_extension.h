// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SKIA_BENCHMARKING_EXTENSION_H_
#define CONTENT_RENDERER_SKIA_BENCHMARKING_EXTENSION_H_

#include "gin/wrappable.h"

namespace blink {
class WebLocalFrame;
}

namespace gin {
class Arguments;
}

namespace content {

class SkiaBenchmarking : public gin::Wrappable<SkiaBenchmarking> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  SkiaBenchmarking(const SkiaBenchmarking&) = delete;
  SkiaBenchmarking& operator=(const SkiaBenchmarking&) = delete;

  static void Install(blink::WebLocalFrame* frame);

  // Wrapper around SkGraphics::Init that can be invoked multiple times.
  static void Initialize();

 private:
  SkiaBenchmarking();
  ~SkiaBenchmarking() override;

  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  // Rasterizes a Picture JSON-encoded by cc::Picture::AsValue().
  //
  // Takes a JSON-encoded cc::Picture and optionally rasterization parameters:
  //   {
  //     'scale':    {Number},
  //     'stop':     {Number},
  //     'overdraw': {Boolean},
  //     'clip':     [Number, Number, Number, Number]
  //   }
  //
  // Returns
  //  {
  //    'width':    {Number},
  //    'height':   {Number},
  //    'data':     {ArrayBuffer}
  //  }
  void Rasterize(gin::Arguments* args);

  // Extracts the Skia draw commands from a JSON-encoded cc::Picture.
  //
  // Takes a JSON-encoded cc::Picture and returns
  // [{ 'cmd': {String}, 'info': [String, ...] }, ...]
  void GetOps(gin::Arguments* args);

  // Returns timing information for the given picture.
  //
  // Takes a JSON-encoded cc::Picture and returns
  // { 'total_time': {Number}, 'cmd_times': [Number, ...] }
  void GetOpTimings(gin::Arguments* args);

  // Returns meta information for the given picture.
  //
  // Takes a base64 encoded SKP and returns
  // { 'width': {Number}, 'height': {Number} }
  void GetInfo(gin::Arguments* args);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SKIA_BENCHMARKING_EXTENSION_H_
