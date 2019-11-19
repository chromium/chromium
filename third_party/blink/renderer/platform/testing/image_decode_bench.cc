// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides a minimal wrapping of the Blink image decoders. Used to perform
// a non-threaded, memory-to-memory image decode using micro second accuracy
// clocks to measure image decode time.
//
// TODO(noel): Consider integrating this tool in Chrome telemetry for realz,
// using the image corpora used to assess Blink image decode performance. See
// http://crbug.com/398235#c103 and http://crbug.com/258324#c5

#include <chrono>
#include <fstream>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_executor.h"
#include "mojo/core/embedder/embedder.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

namespace {

scoped_refptr<SharedBuffer> ReadFile(const char* name) {
  std::string file;
  if (base::ReadFileToString(base::FilePath::FromUTF8Unsafe(name), &file))
    return SharedBuffer::Create(file.data(), file.size());
  perror(name);
  exit(2);
  return SharedBuffer::Create();
}

struct ImageMeta {
  const char* name;
  int width;
  int height;
  int frames;
  // Cumulative time in seconds to decode all frames.
  double time;
};

void DecodeFailure(ImageMeta* image) {
  fprintf(stderr, "Failed to decode image %s\n", image->name);
  exit(3);
}

void DecodeImageData(SharedBuffer* data, ImageMeta* image) {
  const bool all_data_received = true;

  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      data, all_data_received, ImageDecoder::kAlphaPremultiplied,
      ImageDecoder::kDefaultBitDepth, ColorBehavior::Ignore());

  auto start = std::chrono::steady_clock::now();

  decoder->SetData(data, all_data_received);
  size_t frame_count = decoder->FrameCount();
  for (size_t index = 0; index < frame_count; ++index) {
    if (!decoder->DecodeFrameBufferAtIndex(index))
      DecodeFailure(image);
  }

  auto end = std::chrono::steady_clock::now();

  if (!frame_count || decoder->Failed())
    DecodeFailure(image);

  image->time += std::chrono::duration<double>(end - start).count();
  image->width = decoder->Size().Width();
  image->height = decoder->Size().Height();
  image->frames = frame_count;
}

}  // namespace

void ImageDecodeBenchMain(int argc, char* argv[]) {
  int option, iterations = 1;

  auto usage_exit = [&] {
    fprintf(stderr, "Usage: %s [-i iterations] file [file...]\n", argv[0]);
    exit(1);
  };

  for (option = 1; option < argc; ++option) {
    if (argv[option][0] != '-')
      break;  // End of optional arguments.
    if (std::string(argv[option]) != "-i")
      usage_exit();
    iterations = (++option < argc) ? atoi(argv[option]) : 0;
    if (iterations < 1)
      usage_exit();
  }

  if (option >= argc)
    usage_exit();

  // Setup Blink platform.

  std::unique_ptr<Platform> platform = std::make_unique<Platform>();
  Platform::CreateMainThreadAndInitialize(platform.get());

  // Bench each image file.

  while (option < argc) {
    const char* name = argv[option++];

    // Read entire file content into |data| (a contiguous block of memory) then
    // decode it to verify the image and record its ImageMeta data.

    ImageMeta image = {name, 0, 0, 0, 0};
    scoped_refptr<SharedBuffer> data = ReadFile(name);
    DecodeImageData(data.get(), &image);

    // Image decode bench for iterations.

    double total_time = 0.0;
    for (int i = 0; i < iterations; ++i) {
      image.time = 0.0;
      DecodeImageData(data.get(), &image);
      total_time += image.time;
    }

    // Results to stdout.

    double average_time = total_time / iterations;
    printf("%f %f %s\n", total_time, average_time, name);
  }
}

}  // namespace blink

int main(int argc, char* argv[]) {
  base::SingleThreadTaskExecutor main_task_executor;
  mojo::core::Init();
  base::CommandLine::Init(argc, argv);
  blink::ImageDecodeBenchMain(argc, argv);
  return 0;
}
