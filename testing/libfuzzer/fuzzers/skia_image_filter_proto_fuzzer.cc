// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Takes an Input protobuf message from libprotobuf-mutator, converts it to an
// actual skia image filter and then applies it to a canvas for the purpose of
// fuzzing skia. This should uncover bugs that could be used by a compromised
// renderer to exploit the browser process.

#include <stdlib.h>

#include <iostream>
#include <string>

#include "testing/libfuzzer/proto/skia_image_filter_proto_converter.h"

#include "base/process/memory.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageFilter.h"

protobuf_mutator::protobuf::LogSilencer log_silencer;

using skia_image_filter_proto_converter::Input;
using skia_image_filter_proto_converter::Converter;

static const int kBitmapSize = 24;

struct Environment {
  base::TestDiscardableMemoryAllocator* discardable_memory_allocator;
  Environment() {
    base::EnableTerminationOnOutOfMemory();
    discardable_memory_allocator = new base::TestDiscardableMemoryAllocator();
    base::DiscardableMemoryAllocator::SetInstance(discardable_memory_allocator);
  }
};

DEFINE_PROTO_FUZZER(const Input& input) {
  [[maybe_unused]] static Environment environment = Environment();

  static Converter converter = Converter();
  std::string ipc_filter_message = converter.Convert(input);

  // Allow the flattened skia filter to be retrieved easily.
  if (getenv("LPM_DUMP_NATIVE_INPUT")) {
    // Don't write a newline since it will make the output invalid (so that it
    // cannot be fed to filter_fuzz_stub) Flush instead.
    std::cout << ipc_filter_message << std::flush;
  }

  sk_sp<SkImageFilter> flattenable = SkImageFilter::Deserialize(
      ipc_filter_message.c_str(), ipc_filter_message.size());

  if (!flattenable)
    return;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(kBitmapSize, kBitmapSize);
  SkCanvas canvas(bitmap);
  canvas.clear(0x00000000);
  SkPaint paint;
  paint.setImageFilter(flattenable);
  canvas.save();
  canvas.clipRect(SkRect::MakeXYWH(0, 0, SkIntToScalar(kBitmapSize),
                                   SkIntToScalar(kBitmapSize)));
  canvas.drawImage(bitmap.asImage(), 0, 0, SkSamplingOptions(), &paint);
  canvas.restore();
}
