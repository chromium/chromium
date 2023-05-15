// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_BOX_WRITER_H_
#define MEDIA_MUXERS_MP4_BOX_WRITER_H_

#include <memory>
#include <vector>

#include "base/sequence_checker.h"
#include "media/base/media_export.h"
#include "media/formats/mp4/fourccs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

class BoxByteStream;
class Mp4MuxerContext;

// The Mp4BoxWriter is parent class for all box writers.
// Every box writers must derive from Mp4BoxWriter.
// The Mp4BoxWriter has container for the children boxes and the derived box
// should add the children in its ctor, not any other places for better
// maintenance of the code.
class MEDIA_EXPORT Mp4BoxWriter {
 public:
  explicit Mp4BoxWriter(const Mp4MuxerContext& context);
  virtual ~Mp4BoxWriter();
  Mp4BoxWriter(const Mp4BoxWriter&) = delete;
  Mp4BoxWriter& operator=(const Mp4BoxWriter&) = delete;

  // Write the box that will also calls children's Write if it has any.
  virtual void Write(BoxByteStream& writer) = 0;

  // Same as `Write()` but creates a `BoxByteStream` and writes to `context`.
  // It is expected that it will create box itself as well as its children.
  void WriteAndFlush();

 protected:
  // Write for children boxes. The function will calls Write
  // function of all children boxes.
  void WriteChildren(BoxByteStream& writer);

  // Add child box of the current box.
  void AddChildBox(std::unique_ptr<Mp4BoxWriter> box_writer);

  // Get the Mp4MuxerContext object.
  const Mp4MuxerContext& context() const { return context_; }

 private:
  const Mp4MuxerContext& context_;
  std::vector<std::unique_ptr<Mp4BoxWriter>> child_boxes_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_BOX_WRITER_H_
