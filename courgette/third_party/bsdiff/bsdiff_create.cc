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
// 2005-05-05 - Use the modified header struct from bspatch.h; use 32-bit
//              values throughout.
//                --Benjamin Smedberg <benjamin@smedbergs.us>
// 2005-05-18 - Use the same CRC algorithm as bzip2, and leverage the CRC table
//              provided by libbz2.
//                --Darin Fisher <darin@meer.net>
// 2007-11-14 - Changed to use Crc from Lzma library instead of Bzip library
//                --Rahul Kuchhal
// 2009-03-31 - Change to use Streams.  Added lots of comments.
//                --Stephen Adams <sra@chromium.org>
// 2010-05-26 - Use a paged array for V and I. The address space may be too
//              fragmented for these big arrays to be contiguous.
//                --Stephen Adams <sra@chromium.org>
// 2015-08-03 - Extract qsufsort portion to a separate file.
//                --Samuel Huang <huangs@chromium.org>
// 2015-08-12 - Interface change to search().
//                --Samuel Huang <huangs@chromium.org>
// 2016-07-29 - Replacing qsufsort with divsufsort.
//                --Samuel Huang <huangs@chromium.org>

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/third_party/bsdiff/bsdiff.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

#include "courgette/crc.h"
#include "courgette/streams.h"
#include "courgette/third_party/bsdiff/bsdiff_search.h"
#include "courgette/third_party/bsdiff/paged_array.h"
#include "courgette/third_party/divsufsort/divsufsort.h"

namespace {

using courgette::CalculateCrc;
using courgette::PagedArray;
using courgette::SinkStream;
using courgette::SinkStreamSet;
using courgette::SourceStream;
using courgette::SourceStreamSet;

}  // namespace

namespace bsdiff {

static CheckBool WriteHeader(SinkStream* stream, MBSPatchHeader* header) {
  bool ok = stream->Write(header->tag, sizeof(header->tag));
  ok &= stream->WriteVarint32(header->slen);
  ok &= stream->WriteVarint32(header->scrc32);
  ok &= stream->WriteVarint32(header->dlen);
  return ok;
}

BSDiffStatus CreateBinaryPatch(SourceStream* old_stream,
                               SourceStream* new_stream,
                               SinkStream* patch_stream) {
  base::Time start_bsdiff_time = base::Time::Now();
  VLOG(1) << "Start bsdiff";
  size_t initial_patch_stream_length = patch_stream->Length();

  SinkStreamSet patch_streams;
  SinkStream* control_stream_copy_counts = patch_streams.stream(0);
  SinkStream* control_stream_extra_counts = patch_streams.stream(1);
  SinkStream* control_stream_seeks = patch_streams.stream(2);
  SinkStream* diff_skips = patch_streams.stream(3);
  SinkStream* diff_bytes = patch_streams.stream(4);
  SinkStream* extra_bytes = patch_streams.stream(5);

  const uint8_t* old = old_stream->Buffer();
  const int oldsize = static_cast<int>(old_stream->Remaining());

  uint32_t pending_diff_zeros = 0;

  PagedArray<divsuf::saidx_t> I;

  if (!I.Allocate(oldsize + 1)) {
    LOG(ERROR) << "Could not allocate I[], " << ((oldsize + 1) * sizeof(int))
               << " bytes";
    return MEM_ERROR;
  }

  base::Time q_start_time = base::Time::Now();
  divsuf::saint_t result = divsuf::divsufsort_include_empty(
      old, I.begin(), oldsize);
  VLOG(1) << " done divsufsort "
          << (base::Time::Now() - q_start_time).InSecondsF();
  if (result != 0)
    return UNEXPECTED_ERROR;

  const uint8_t* newbuf = new_stream->Buffer();
  const int newsize = static_cast<int>(new_stream->Remaining());

  int control_length = 0;
  int diff_bytes_length = 0;
  int diff_bytes_nonzero = 0;
  int extra_bytes_length = 0;

  // The patch format is a sequence of triples <copy,extra,seek> where 'copy' is
  // the number of bytes to copy from the old file (possibly with mistakes),
  // 'extra' is the number of bytes to copy from a stream of fresh bytes, and
  // 'seek' is an offset to move to the position to copy for the next triple.
  //
  // The invariant at the top of this loop is that we are committed to emitting
  // a triple for the part of |newbuf| surrounding a 'seed' match near
  // |lastscan|.  We are searching for a second match that will be the 'seed' of
  // the next triple.  As we scan through |newbuf|, one of four things can
  // happen at the current position |scan|:
  //
  //  1. We find a nice match that appears to be consistent with the current
  //     seed.  Continue scanning.  It is likely that this match will become
  //     part of the 'copy'.
  //
  //  2. We find match which does much better than extending the current seed
  //     old match.  Emit a triple for the current seed and take this match as
  //     the new seed for a new triple.  By 'much better' we remove 8 mismatched
  //     bytes by taking the new seed.
  //
  //  3. There is not a good match.  Continue scanning.  These bytes will likely
  //     become part of the 'extra'.
  //
  //  4. There is no match because we reached the end of the input, |newbuf|.

  // This is how the loop advances through the bytes of |newbuf|:
  //
  // ...012345678901234567890123456789...
  //    ssssssssss                      Seed at |lastscan|
  //              xxyyyxxyyxy           |scan| forward, cases (3)(x) & (1)(y)
  //                         mmmmmmmm   New match will start new seed case (2).
  //    fffffffffffffff                 |lenf| = scan forward from |lastscan|
  //                     bbbb           |lenb| = scan back from new seed |scan|.
  //    ddddddddddddddd                 Emit diff bytes for the 'copy'.
  //                   xx               Emit extra bytes.
  //                     ssssssssssss   |lastscan = scan - lenb| is new seed.
  //                                 x  Cases (1) and (3) ....

  int lastscan = 0, lastpos = 0, lastoffset = 0;

  int scan = 0;
  SearchResult match(0, 0);

  while (scan < newsize) {
    int oldscore = 0;  // Count of how many bytes of the current match at |scan|
                       // extend the match at |lastscan|.
    match.pos = 0;

    scan += match.size;
    for (int scsc = scan; scan < newsize; ++scan) {
      match = search<PagedArray<divsuf::saidx_t>&>(
          I, old, oldsize, newbuf + scan, newsize - scan);

      for (; scsc < scan + match.size; scsc++)
        if ((scsc + lastoffset < oldsize) &&
            (old[scsc + lastoffset] == newbuf[scsc]))
          oldscore++;

      if ((match.size == oldscore) && (match.size != 0))
        break;  // Good continuing match, case (1)
      if (match.size > oldscore + 8)
        break;  // New seed match, case (2)

      if ((scan + lastoffset < oldsize) &&
          (old[scan + lastoffset] == newbuf[scan]))
        oldscore--;
      // Case (3) continues in this loop until we fall out of the loop (4).
    }

    if ((match.size != oldscore) || (scan == newsize)) {  // Cases (2) and (4)
      // This next chunk of code finds the boundary between the bytes to be
      // copied as part of the current triple, and the bytes to be copied as
      // part of the next triple.  The |lastscan| match is extended forwards as
      // far as possible provided doing to does not add too many mistakes.  The
      // |scan| match is extended backwards in a similar way.

      // Extend the current match (if any) backwards.  |lenb| is the maximal
      // extension for which less than half the byte positions in the extension
      // are wrong.
      int lenb = 0;
      if (scan < newsize) {  // i.e. not case (4); there is a match to extend.
        int score = 0, Sb = 0;
        for (int i = 1; (scan >= lastscan + i) && (match.pos >= i); i++) {
          if (old[match.pos - i] == newbuf[scan - i])
            score++;
          if (score * 2 - i > Sb * 2 - lenb) {
            Sb = score;
            lenb = i;
          }
        }
      }

      // Extend the lastscan match forward; |lenf| is the maximal extension for
      // which less than half of the byte positions in entire lastscan match are
      // wrong.  There is a subtle point here: |lastscan| points to before the
      // seed match by |lenb| bytes from the previous iteration.  This is why
      // the loop measures the total number of mistakes in the the match, not
      // just the from the match.
      int lenf = 0;
      {
        int score = 0, Sf = 0;
        for (int i = 0; (lastscan + i < scan) && (lastpos + i < oldsize);) {
          if (old[lastpos + i] == newbuf[lastscan + i])
            score++;
          i++;
          if (score * 2 - i > Sf * 2 - lenf) {
            Sf = score;
            lenf = i;
          }
        }
      }

      // If the extended scans overlap, pick a position in the overlap region
      // that maximizes the exact matching bytes.
      if (lastscan + lenf > scan - lenb) {
        int overlap = (lastscan + lenf) - (scan - lenb);
        int score = 0;
        int Ss = 0, lens = 0;
        for (int i = 0; i < overlap; i++) {
          if (newbuf[lastscan + lenf - overlap + i] ==
              old[lastpos + lenf - overlap + i]) {
            score++;
          }
          if (newbuf[scan - lenb + i] == old[match.pos - lenb + i]) {
            score--;
          }
          if (score > Ss) {
            Ss = score;
            lens = i + 1;
          }
        }

        lenf += lens - overlap;
        lenb -= lens;
      };

      for (int i = 0; i < lenf; i++) {
        uint8_t diff_byte = newbuf[lastscan + i] - old[lastpos + i];
        if (diff_byte) {
          ++diff_bytes_nonzero;
          if (!diff_skips->WriteVarint32(pending_diff_zeros))
            return MEM_ERROR;
          pending_diff_zeros = 0;
          if (!diff_bytes->Write(&diff_byte, 1))
            return MEM_ERROR;
        } else {
          ++pending_diff_zeros;
        }
      }
      int gap = (scan - lenb) - (lastscan + lenf);
      for (int i = 0; i < gap; i++) {
        if (!extra_bytes->Write(&newbuf[lastscan + lenf + i], 1))
          return MEM_ERROR;
      }

      diff_bytes_length += lenf;
      extra_bytes_length += gap;

      uint32_t copy_count = lenf;
      uint32_t extra_count = gap;
      int32_t seek_adjustment = ((match.pos - lenb) - (lastpos + lenf));

      if (!control_stream_copy_counts->WriteVarint32(copy_count) ||
          !control_stream_extra_counts->WriteVarint32(extra_count) ||
          !control_stream_seeks->WriteVarint32Signed(seek_adjustment)) {
        return MEM_ERROR;
      }

      ++control_length;
#ifdef DEBUG_bsmedberg
      VLOG(1) << StringPrintf(
          "Writing a block:  copy: %-8u extra: %-8u seek: %+-9d", copy_count,
          extra_count, seek_adjustment);
#endif

      lastscan = scan - lenb;  // Include the backward extension in seed.
      lastpos = match.pos - lenb;    //  ditto.
      lastoffset = lastpos - lastscan;
    }
  }

  if (!diff_skips->WriteVarint32(pending_diff_zeros))
    return MEM_ERROR;

  I.clear();

  MBSPatchHeader header;
  // The string will have a null terminator that we don't use, hence '-1'.
  static_assert(sizeof(MBS_PATCH_HEADER_TAG) - 1 == sizeof(header.tag),
                "MBS_PATCH_HEADER_TAG must match header field size");
  memcpy(header.tag, MBS_PATCH_HEADER_TAG, sizeof(header.tag));
  header.slen = oldsize;
  header.scrc32 = CalculateCrc(old, oldsize);
  header.dlen = newsize;

  if (!WriteHeader(patch_stream, &header))
    return MEM_ERROR;

  size_t diff_skips_length = diff_skips->Length();
  if (!patch_streams.CopyTo(patch_stream))
    return MEM_ERROR;

  VLOG(1) << "Control tuples: " << control_length
          << "  copy bytes: " << diff_bytes_length
          << "  mistakes: " << diff_bytes_nonzero
          << "  (skips: " << diff_skips_length << ")"
          << "  extra bytes: " << extra_bytes_length
          << "\nUncompressed bsdiff patch size "
          << patch_stream->Length() - initial_patch_stream_length
          << "\nEnd bsdiff "
          << (base::Time::Now() - start_bsdiff_time).InSecondsF();

  return OK;
}

}  // namespace bsdiff
