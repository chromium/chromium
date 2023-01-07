// Copyright 2003, 2004 Colin Percival
// All rights reserved
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted providing that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// For the terms under which this work may be distributed, please see
// the adjoining file "LICENSE".
//
// ChangeLog:
// 2009-03-31 - Change to use Streams.  Move CRC code to crc.{h,cc}
//                --Stephen Adams <sra@chromium.org>
// 2013-04-10 - Add wrapper method to apply a patch to files directly.
//                --Joshua Pawlicki <waffles@chromium.org>

// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/third_party/bsdiff/bsdiff.h"

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "courgette/crc.h"
#include "courgette/streams.h"

namespace {

using courgette::CalculateCrc;
using courgette::SinkStream;
using courgette::SinkStreamSet;
using courgette::SourceStream;
using courgette::SourceStreamSet;

}  // namespace

namespace bsdiff {

BSDiffStatus MBS_ReadHeader(SourceStream* stream, MBSPatchHeader* header) {
  if (!stream->Read(header->tag, sizeof(header->tag)))
    return READ_ERROR;
  if (!stream->ReadVarint32(&header->slen))
    return READ_ERROR;
  if (!stream->ReadVarint32(&header->scrc32))
    return READ_ERROR;
  if (!stream->ReadVarint32(&header->dlen))
    return READ_ERROR;

  // The string will have a NUL terminator that we don't use, hence '-1'.
  static_assert(sizeof(MBS_PATCH_HEADER_TAG) - 1 == sizeof(header->tag),
                "MBS_PATCH_HEADER_TAG must match header field size");
  if (memcmp(header->tag, MBS_PATCH_HEADER_TAG, 8) != 0)
    return UNEXPECTED_ERROR;

  return OK;
}

BSDiffStatus MBS_ApplyPatch(const MBSPatchHeader* header,
                            SourceStream* patch_stream,
                            const uint8_t* old_start,
                            size_t old_size,
                            SinkStream* new_stream) {
  const uint8_t* old_end = old_start + old_size;

  SourceStreamSet patch_streams;
  if (!patch_streams.Init(patch_stream))
    return READ_ERROR;

  SourceStream* control_stream_copy_counts = patch_streams.stream(0);
  SourceStream* control_stream_extra_counts = patch_streams.stream(1);
  SourceStream* control_stream_seeks = patch_streams.stream(2);
  SourceStream* diff_skips = patch_streams.stream(3);
  SourceStream* diff_bytes = patch_streams.stream(4);
  SourceStream* extra_bytes = patch_streams.stream(5);

  const uint8_t* extra_start = extra_bytes->Buffer();
  const uint8_t* extra_end = extra_start + extra_bytes->Remaining();
  const uint8_t* extra_position = extra_start;
  extra_bytes->Skip(extra_bytes->Remaining());

  const uint8_t* old_position = old_start;

  if (header->dlen && !new_stream->Reserve(header->dlen))
    return MEM_ERROR;

  uint32_t pending_diff_zeros = 0;
  if (!diff_skips->ReadVarint32(&pending_diff_zeros))
    return UNEXPECTED_ERROR;

  while (!control_stream_copy_counts->Empty()) {
    uint32_t copy_count, extra_count;
    int32_t seek_adjustment;
    if (!control_stream_copy_counts->ReadVarint32(&copy_count))
      return UNEXPECTED_ERROR;
    if (!control_stream_extra_counts->ReadVarint32(&extra_count))
      return UNEXPECTED_ERROR;
    if (!control_stream_seeks->ReadVarint32Signed(&seek_adjustment))
      return UNEXPECTED_ERROR;

#ifdef DEBUG_bsmedberg
    printf("Applying block:  copy: %-8u extra: %-8u seek: %+i\n", copy_count,
           extra_count, seek_adjustment);
#endif
    // Byte-wise arithmetically add bytes from old file to bytes from the diff
    // block.
    if (copy_count > static_cast<size_t>(old_end - old_position))
      return UNEXPECTED_ERROR;

    // Add together bytes from the 'old' file and the 'diff' stream.
    for (size_t i = 0; i < copy_count; ++i) {
      uint8_t diff_byte = 0;
      if (pending_diff_zeros) {
        --pending_diff_zeros;
      } else {
        if (!diff_skips->ReadVarint32(&pending_diff_zeros))
          return UNEXPECTED_ERROR;
        if (!diff_bytes->Read(&diff_byte, 1))
          return UNEXPECTED_ERROR;
      }
      uint8_t byte = old_position[i] + diff_byte;
      if (!new_stream->Write(&byte, 1))
        return MEM_ERROR;
    }
    old_position += copy_count;

    // Copy bytes from the extra block.
    if (extra_count > static_cast<size_t>(extra_end - extra_position))
      return UNEXPECTED_ERROR;

    if (!new_stream->Write(extra_position, extra_count))
      return MEM_ERROR;

    extra_position += extra_count;

    // "seek" forwards (or backwards) in oldfile.
    if (old_position + seek_adjustment < old_start ||
        old_position + seek_adjustment > old_end)
      return UNEXPECTED_ERROR;

    old_position += seek_adjustment;
  }

  if (!control_stream_copy_counts->Empty() ||
      !control_stream_extra_counts->Empty() || !control_stream_seeks->Empty() ||
      !diff_skips->Empty() || !diff_bytes->Empty() || !extra_bytes->Empty())
    return UNEXPECTED_ERROR;

  return OK;
}

BSDiffStatus ApplyBinaryPatch(SourceStream* old_stream,
                              SourceStream* patch_stream,
                              SinkStream* new_stream) {
  MBSPatchHeader header;
  BSDiffStatus ret = MBS_ReadHeader(patch_stream, &header);
  if (ret != OK)
    return ret;

  const uint8_t* old_start = old_stream->Buffer();
  size_t old_size = old_stream->Remaining();

  if (old_size != header.slen)
    return UNEXPECTED_ERROR;

  if (CalculateCrc(old_start, old_size) != header.scrc32)
    return CRC_ERROR;

  return MBS_ApplyPatch(&header, patch_stream, old_start, old_size, new_stream);
}

BSDiffStatus ApplyBinaryPatch(base::File old_file,
                              base::File patch_file,
                              base::File new_file) {
  // Set up the old stream.
  base::MemoryMappedFile old_file_mem;
  if (!old_file_mem.Initialize(std::move(old_file))) {
    return READ_ERROR;
  }
  SourceStream old_file_stream;
  old_file_stream.Init(old_file_mem.data(), old_file_mem.length());

  // Set up the patch stream.
  base::MemoryMappedFile patch_file_mem;
  if (!patch_file_mem.Initialize(std::move(patch_file))) {
    return READ_ERROR;
  }
  SourceStream patch_file_stream;
  patch_file_stream.Init(patch_file_mem.data(), patch_file_mem.length());

  // Set up the new stream and apply the patch.
  SinkStream new_sink_stream;
  BSDiffStatus status =
      ApplyBinaryPatch(&old_file_stream, &patch_file_stream, &new_sink_stream);
  if (status != OK) {
    return status;
  }

  // Write the stream to disk.
  int written = new_file.Write(
      0,
      reinterpret_cast<const char*>(new_sink_stream.Buffer()),
      static_cast<int>(new_sink_stream.Length()));
  if (written != static_cast<int>(new_sink_stream.Length()))
    return WRITE_ERROR;
  return OK;
}

BSDiffStatus ApplyBinaryPatch(const base::FilePath& old_file_path,
                              const base::FilePath& patch_file_path,
                              const base::FilePath& new_file_path) {
  BSDiffStatus result = ApplyBinaryPatch(
      base::File(old_file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_WIN_SHARE_DELETE),
      base::File(patch_file_path, base::File::FLAG_OPEN |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_WIN_SHARE_DELETE),
      base::File(new_file_path, base::File::FLAG_CREATE_ALWAYS |
                                    base::File::FLAG_WRITE |
                                    base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                    base::File::FLAG_WIN_SHARE_DELETE));
  if (result != OK)
    base::DeleteFile(new_file_path);
  return result;
}

}  // namespace bsdiff
