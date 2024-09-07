/*
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * Portions are Copyright (C) 2001-6 mozilla.org
 *
 * Other contributors:
 *   Stuart Parmenter <stuart@mozilla.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_image_decoder.h"

#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image_metrics.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/private/SkJpegMetadataDecoder.h"

extern "C" {
#include <setjmp.h>
#include <stdio.h>  // jpeglib.h needs stdio FILE.
#include "jpeglib.h"
}

#if defined(ARCH_CPU_BIG_ENDIAN)
#error Blink assumes a little-endian target.
#endif

#if defined(JCS_ALPHA_EXTENSIONS)
#define TURBO_JPEG_RGB_SWIZZLE
#if SK_B32_SHIFT  // Output little-endian RGBA pixels (Android).
inline J_COLOR_SPACE rgbOutputColorSpace() {
  return JCS_EXT_RGBA;
}
#else  // Output little-endian BGRA pixels.
inline J_COLOR_SPACE rgbOutputColorSpace() {
  return JCS_EXT_BGRA;
}
#endif
inline bool turboSwizzled(J_COLOR_SPACE colorSpace) {
  return colorSpace == JCS_EXT_RGBA || colorSpace == JCS_EXT_BGRA;
}
#else
inline J_COLOR_SPACE rgbOutputColorSpace() {
  return JCS_RGB;
}
#endif

namespace {

// JPEG only supports a denominator of 8.
const unsigned g_scale_denominator = 8;

// Extracts the YUV subsampling format of an image given |info| which is assumed
// to have gone through a jpeg_read_header() call.
cc::YUVSubsampling YuvSubsampling(const jpeg_decompress_struct& info) {
  if (info.jpeg_color_space == JCS_YCbCr && info.num_components == 3 &&
      info.comp_info && info.comp_info[1].h_samp_factor == 1 &&
      info.comp_info[1].v_samp_factor == 1 &&
      info.comp_info[2].h_samp_factor == 1 &&
      info.comp_info[2].v_samp_factor == 1) {
    const int h = info.comp_info[0].h_samp_factor;
    const int v = info.comp_info[0].v_samp_factor;
    if (v == 1) {
      switch (h) {
        case 1:
          return cc::YUVSubsampling::k444;
        case 2:
          return cc::YUVSubsampling::k422;
        case 4:
          return cc::YUVSubsampling::k411;
      }
    } else if (v == 2) {
      switch (h) {
        case 1:
          return cc::YUVSubsampling::k440;
        case 2:
          return cc::YUVSubsampling::k420;
        case 4:
          return cc::YUVSubsampling::k410;
      }
    }
  }
  return cc::YUVSubsampling::kUnknown;
}

bool SubsamplingSupportedByDecodeToYUV(cc::YUVSubsampling subsampling) {
  // Only subsamplings 4:4:4, 4:2:2, and 4:2:0 are supported.
  return subsampling == cc::YUVSubsampling::k444 ||
         subsampling == cc::YUVSubsampling::k422 ||
         subsampling == cc::YUVSubsampling::k420;
}

// Rounds |size| to the smallest multiple of |alignment| that is greater than or
// equal to |size|.
// Note that base::bits::Align is not used here because the alignment is not
// guaranteed to be a power of two.
int Align(int size, int alignment) {
  // Width and height are 16 bits for a JPEG (i.e. < 65536) and the maximum
  // size of a JPEG MCU in either dimension is 8 * 4 == 32.
  DCHECK_GE(size, 0);
  DCHECK_LT(size, 1 << 16);
  DCHECK_GT(alignment, 0);
  DCHECK_LE(alignment, 32);

  if (size % alignment == 0) {
    return size;
  }

  return ((size + alignment) / alignment) * alignment;
}

}  // namespace

namespace blink {

struct decoder_error_mgr {
  DISALLOW_NEW();
  struct jpeg_error_mgr pub;  // "public" fields for IJG library
  int num_corrupt_warnings;   // Counts corrupt warning messages
  jmp_buf setjmp_buffer;      // For handling catastropic errors
};

struct decoder_source_mgr {
  DISALLOW_NEW();
  struct jpeg_source_mgr pub;  // "public" fields for IJG library
  raw_ptr<JPEGImageReader> reader;
};

enum jstate {
  kJpegHeader,  // Reading JFIF headers
  kJpegStartDecompress,
  kJpegDecompressProgressive,  // Output progressive pixels
  kJpegDecompressSequential,   // Output sequential pixels
  kJpegDone
};

void init_source(j_decompress_ptr jd);
boolean fill_input_buffer(j_decompress_ptr jd);
void skip_input_data(j_decompress_ptr jd, long num_bytes);
void term_source(j_decompress_ptr jd);
void error_exit(j_common_ptr cinfo);
void emit_message(j_common_ptr cinfo, int msg_level);

static gfx::Size ComputeYUVSize(const jpeg_decompress_struct* info,
                                int component) {
  return gfx::Size(info->comp_info[component].downsampled_width,
                   info->comp_info[component].downsampled_height);
}

static wtf_size_t ComputeYUVWidthBytes(const jpeg_decompress_struct* info,
                                       int component) {
  return info->comp_info[component].width_in_blocks * DCTSIZE;
}

static void ProgressMonitor(j_common_ptr info) {
  int scan = ((j_decompress_ptr)info)->input_scan_number;
  // Progressive images with a very large number of scans can cause the
  // decoder to hang.  Here we use the progress monitor to abort on
  // a very large number of scans.  100 is arbitrary, but much larger
  // than the number of scans we might expect in a normal image.
  if (scan >= 100) {
    error_exit(info);
  }
}

class JPEGImageReader final {
  USING_FAST_MALLOC(JPEGImageReader);

 public:
  JPEGImageReader(JPEGImageDecoder* decoder, wtf_size_t initial_offset)
      : decoder_(decoder),
        needs_restart_(false),
        restart_position_(initial_offset),
        next_read_position_(initial_offset),
        last_set_byte_(nullptr),
        state_(kJpegHeader),
        samples_(nullptr) {
    memset(&info_, 0, sizeof(jpeg_decompress_struct));

    // Set up the normal JPEG error routines, then override error_exit.
    info_.err = jpeg_std_error(&err_.pub);
    err_.pub.error_exit = error_exit;

    // Allocate and initialize JPEG decompression object.
    jpeg_create_decompress(&info_);

    // Initialize source manager.
    memset(&src_, 0, sizeof(decoder_source_mgr));
    info_.src = reinterpret_cast_ptr<jpeg_source_mgr*>(&src_);

    // Set up callback functions.
    src_.pub.init_source = init_source;
    src_.pub.fill_input_buffer = fill_input_buffer;
    src_.pub.skip_input_data = skip_input_data;
    src_.pub.resync_to_restart = jpeg_resync_to_restart;
    src_.pub.term_source = term_source;
    src_.reader = this;

    // Set up a progress monitor.
    info_.progress = &progress_mgr_;
    progress_mgr_.progress_monitor = ProgressMonitor;

    // Keep APP1 blocks, for obtaining exif and XMP data.
    jpeg_save_markers(&info_, JPEG_APP0 + 1, 0xFFFF);

    // Keep APP2 blocks, for obtaining ICC and MPF data.
    jpeg_save_markers(&info_, JPEG_APP0 + 2, 0xFFFF);
  }

  JPEGImageReader(const JPEGImageReader&) = delete;
  JPEGImageReader& operator=(const JPEGImageReader&) = delete;

  ~JPEGImageReader() {
    // Reset `metadata_decoder_` before `info_` because `metadata_decoder_`
    // points to memory owned by `info_`.
    metadata_decoder_ = nullptr;
    jpeg_destroy_decompress(&info_);
  }

  void SkipBytes(long num_bytes) {
    if (num_bytes <= 0) {
      return;
    }

    wtf_size_t bytes_to_skip = static_cast<wtf_size_t>(num_bytes);

    if (bytes_to_skip < info_.src->bytes_in_buffer) {
      // The next byte needed is in the buffer. Move to it.
      info_.src->bytes_in_buffer -= bytes_to_skip;
      info_.src->next_input_byte += bytes_to_skip;
    } else {
      // Move beyond the buffer and empty it.
      next_read_position_ = static_cast<wtf_size_t>(
          next_read_position_ + bytes_to_skip - info_.src->bytes_in_buffer);
      info_.src->bytes_in_buffer = 0;
      info_.src->next_input_byte = nullptr;
    }

    // This is a valid restart position.
    restart_position_ = static_cast<wtf_size_t>(next_read_position_ -
                                                info_.src->bytes_in_buffer);
    // We updated |next_input_byte|, so we need to update |last_byte_set_|
    // so we know not to update |restart_position_| again.
    last_set_byte_ = info_.src->next_input_byte;
  }

  bool FillBuffer() {
    if (needs_restart_) {
      needs_restart_ = false;
      next_read_position_ = restart_position_;
    } else {
      UpdateRestartPosition();
    }

    const char* segment;
    const size_t bytes = data_->GetSomeData(segment, next_read_position_);
    if (bytes == 0) {
      // We had to suspend. When we resume, we will need to start from the
      // restart position.
      needs_restart_ = true;
      ClearBuffer();
      return false;
    }

    next_read_position_ += bytes;
    info_.src->bytes_in_buffer = bytes;
    const JOCTET* next_byte = reinterpret_cast_ptr<const JOCTET*>(segment);
    info_.src->next_input_byte = next_byte;
    last_set_byte_ = next_byte;
    return true;
  }

  void SetData(scoped_refptr<SegmentReader> data) {
    if (data_ == data) {
      return;
    }

    data_ = std::move(data);

    // If a restart is needed, the next call to fillBuffer will read from the
    // new SegmentReader.
    if (needs_restart_) {
      return;
    }

    // Otherwise, empty the buffer, and leave the position the same, so
    // FillBuffer continues reading from the same position in the new
    // SegmentReader.
    next_read_position_ -= info_.src->bytes_in_buffer;
    ClearBuffer();
  }

  bool ShouldDecodeToOriginalSize() const {
    // We should decode only to original size if either dimension cannot fit a
    // whole number of MCUs.
    const int max_h_samp_factor = info_.max_h_samp_factor;
    const int max_v_samp_factor = info_.max_v_samp_factor;
    DCHECK_GE(max_h_samp_factor, 1);
    DCHECK_GE(max_v_samp_factor, 1);
    DCHECK_LE(max_h_samp_factor, 4);
    DCHECK_LE(max_v_samp_factor, 4);
    const int mcu_width = info_.max_h_samp_factor * DCTSIZE;
    const int mcu_height = info_.max_v_samp_factor * DCTSIZE;
    return info_.image_width % mcu_width != 0 ||
           info_.image_height % mcu_height != 0;
  }

  // Whether or not the horizontal and vertical sample factors of all components
  // hold valid values (i.e. 1, 2, 3, or 4). It also returns the maximal
  // horizontal and vertical sample factors via |max_h| and |max_v|.
  bool AreValidSampleFactorsAvailable(int* max_h, int* max_v) const {
    if (!info_.num_components) {
      return false;
    }

    const jpeg_component_info* comp_info = info_.comp_info;
    if (!comp_info) {
      return false;
    }

    *max_h = 0;
    *max_v = 0;
    for (int i = 0; i < info_.num_components; ++i) {
      if (comp_info[i].h_samp_factor < 1 || comp_info[i].h_samp_factor > 4 ||
          comp_info[i].v_samp_factor < 1 || comp_info[i].v_samp_factor > 4) {
        return false;
      }

      *max_h = std::max(*max_h, comp_info[i].h_samp_factor);
      *max_v = std::max(*max_v, comp_info[i].v_samp_factor);
    }
    return true;
  }

  // Decode the JPEG data.
  bool Decode(JPEGImageDecoder::DecodingMode decoding_mode) {
    // We need to do the setjmp here. Otherwise bad things will happen
    if (setjmp(err_.setjmp_buffer)) {
      return decoder_->SetFailed();
    }

    switch (state_) {
      case kJpegHeader: {
        // Read file parameters with jpeg_read_header().
        if (jpeg_read_header(&info_, true) == JPEG_SUSPENDED) {
          return false;  // I/O suspension.
        }

        switch (info_.jpeg_color_space) {
          case JCS_YCbCr:
            [[fallthrough]];  // libjpeg can convert YCbCr image pixels to RGB.
          case JCS_GRAYSCALE:
            [[fallthrough]];  // libjpeg can convert GRAYSCALE image pixels to
                              // RGB.
          case JCS_RGB:
            info_.out_color_space = rgbOutputColorSpace();
            break;
          case JCS_CMYK:
          case JCS_YCCK:
            // libjpeg can convert YCCK to CMYK, but neither to RGB, so we
            // manually convert CMKY to RGB.
            info_.out_color_space = JCS_CMYK;
            break;
          default:
            return decoder_->SetFailed();
        }

        state_ = kJpegStartDecompress;

        // Build the SkJpegMetadataDecoder to extract metadata from the
        // now-complete header.
        {
          std::vector<SkJpegMetadataDecoder::Segment> segments;
          for (auto* marker = info_.marker_list; marker;
               marker = marker->next) {
            segments.emplace_back(
                marker->marker,
                SkData::MakeWithoutCopy(marker->data, marker->data_length));
          }
          metadata_decoder_ = SkJpegMetadataDecoder::Make(std::move(segments));
        }

        // We can fill in the size now that the header is available.
        if (!decoder_->SetSize(info_.image_width, info_.image_height)) {
          return false;
        }

        // Calculate and set decoded size.
        int max_numerator = decoder_->DesiredScaleNumerator();
        info_.scale_denom = g_scale_denominator;

        if (decoder_->ShouldGenerateAllSizes()) {
          // Some images should not be scaled down by libjpeg_turbo because
          // doing so may cause artifacts. Specifically, if the image contains a
          // non-whole number of MCUs in either dimension, it's possible that
          // the encoder used bogus data to create the last row or column of
          // MCUs. This data may manifest when downscaling using libjpeg_turbo.
          // See https://crbug.com/890745 and
          // https://github.com/libjpeg-turbo/libjpeg-turbo/issues/297. Hence,
          // we'll only allow downscaling an image if both dimensions fit a
          // whole number of MCUs or if decoding to the original size would
          // cause us to exceed memory limits. The latter case is detected by
          // checking the |max_numerator| returned by DesiredScaleNumerator():
          // this method will return either |g_scale_denominator| if decoding to
          // the original size won't exceed the memory limit (see
          // |max_decoded_bytes_| in ImageDecoder) or something less than
          // |g_scale_denominator| otherwise to ensure the image is downscaled.
          Vector<SkISize> sizes;
          if (max_numerator == g_scale_denominator &&
              ShouldDecodeToOriginalSize()) {
            sizes.push_back(
                SkISize::Make(info_.image_width, info_.image_height));
          } else {
            sizes.reserve(max_numerator);
            for (int numerator = 1; numerator <= max_numerator; ++numerator) {
              info_.scale_num = numerator;
              jpeg_calc_output_dimensions(&info_);
              sizes.push_back(
                  SkISize::Make(info_.output_width, info_.output_height));
            }
          }
          decoder_->SetSupportedDecodeSizes(std::move(sizes));
        }

        info_.scale_num = max_numerator;
        jpeg_calc_output_dimensions(&info_);
        decoder_->SetDecodedSize(info_.output_width, info_.output_height);

        decoder_->ApplyExifMetadata(
            metadata_decoder_->getExifMetadata(/*copyData=*/false).get(),
            gfx::Size(info_.output_width, info_.output_height));

        // Allow color management of the decoded RGBA pixels if possible.
        if (!decoder_->IgnoresColorSpace()) {
          // Extract the ICC profile data without copying it (the function
          // ColorProfile::Create will make its own copy).
          sk_sp<SkData> profile_data =
              metadata_decoder_->getICCProfileData(/*copyData=*/false);
          if (profile_data) {
            std::unique_ptr<ColorProfile> profile = ColorProfile::Create(
                base::span(profile_data->bytes(), profile_data->size()));
            if (profile) {
              uint32_t data_color_space =
                  profile->GetProfile()->data_color_space;
              switch (info_.jpeg_color_space) {
                case JCS_CMYK:
                case JCS_YCCK:
                  if (data_color_space != skcms_Signature_CMYK) {
                    profile = nullptr;
                  }
                  break;
                case JCS_GRAYSCALE:
                  if (data_color_space != skcms_Signature_Gray &&
                      data_color_space != skcms_Signature_RGB) {
                    profile = nullptr;
                  }
                  break;
                default:
                  if (data_color_space != skcms_Signature_RGB) {
                    profile = nullptr;
                  }
                  break;
              }
              if (profile) {
                Decoder()->SetEmbeddedColorProfile(std::move(profile));
              }
            } else {
              DLOG(ERROR) << "Failed to parse image ICC profile";
            }
          }
        }

        // Don't allocate a giant and superfluous memory buffer when the
        // image is a sequential JPEG.
        info_.buffered_image = jpeg_has_multiple_scans(&info_);
        if (info_.buffered_image) {
          err_.pub.emit_message = emit_message;
          err_.num_corrupt_warnings = 0;
        }

        if (decoding_mode == JPEGImageDecoder::DecodingMode::kDecodeHeader) {
          // This exits the function while there is still potentially
          // data in the buffer. Before this function is called again,
          // the SharedBuffer may be collapsed (by a call to
          // MergeSegmentsIntoBuffer), invalidating the "buffer" (which
          // in reality is a pointer into the SharedBuffer's data).
          // Defensively empty the buffer, but first find the latest
          // restart position and signal to restart, so the next call to
          // FillBuffer will resume from the correct point.
          needs_restart_ = true;
          UpdateRestartPosition();
          ClearBuffer();
          return true;
        }
      }
        [[fallthrough]];
      case kJpegStartDecompress:
        if (decoding_mode == JPEGImageDecoder::DecodingMode::kDecodeToYuv) {
          DCHECK(decoder_->CanDecodeToYUV());
          DCHECK(decoder_->HasImagePlanes());
          info_.out_color_space = JCS_YCbCr;
          info_.raw_data_out = TRUE;
          uv_size_ = ComputeYUVSize(&info_, 1);
          // U size and V size have to be the same if we got here
          DCHECK_EQ(uv_size_, ComputeYUVSize(&info_, 2));
        }

        // Set parameters for decompression.
        // FIXME -- Should reset dct_method and dither mode for final pass
        // of progressive JPEG.
        info_.dct_method = JDCT_ISLOW;
        info_.dither_mode = JDITHER_FS;
        info_.do_fancy_upsampling = true;
        info_.do_block_smoothing = true;
        info_.enable_2pass_quant = false;
        // FIXME: should we just assert these?
        info_.enable_external_quant = false;
        info_.enable_1pass_quant = false;
        info_.quantize_colors = false;
        info_.colormap = nullptr;

        // Make a one-row-high sample array that will go away when done with
        // image. Always make it big enough to hold one RGBA row. Since this
        // uses the IJG memory manager, it must be allocated before the call
        // to jpeg_start_decompress().
        samples_ = AllocateSampleArray();

        // Start decompressor.
        if (!jpeg_start_decompress(&info_)) {
          return false;  // I/O suspension.
        }

        // If this is a progressive JPEG ...
        state_ = (info_.buffered_image) ? kJpegDecompressProgressive
                                        : kJpegDecompressSequential;
        [[fallthrough]];

      case kJpegDecompressSequential:
        if (state_ == kJpegDecompressSequential) {
          if (!decoder_->OutputScanlines()) {
            return false;  // I/O suspension.
          }

          // If we've completed image output...
          DCHECK_EQ(info_.output_scanline, info_.output_height);
          state_ = kJpegDone;
        }
        [[fallthrough]];

      case kJpegDecompressProgressive:
        if (state_ == kJpegDecompressProgressive) {
          auto all_components_seen = [](const jpeg_decompress_struct& info) {
            if (info.coef_bits) {
              for (int c = 0; c < info.num_components; ++c) {
                if (info.coef_bits[c][0] == -1) {
                  // Haven't seen this component yet.
                  return false;
                }
              }
            }
            return true;
          };
          int status = 0;
          int first_scan_to_display =
              all_components_seen(info_) ? info_.input_scan_number : 0;
          do {
            decoder_error_mgr* err =
                reinterpret_cast_ptr<decoder_error_mgr*>(info_.err);
            if (err->num_corrupt_warnings) {
              break;
            }
            status = jpeg_consume_input(&info_);
            if (status == JPEG_REACHED_SOS || status == JPEG_REACHED_EOI ||
                status == JPEG_SUSPENDED) {
              // record the first scan where all components are present
              if (!first_scan_to_display && all_components_seen(info_)) {
                first_scan_to_display = info_.input_scan_number;
              }
            }
          } while (!(status == JPEG_SUSPENDED || status == JPEG_REACHED_EOI));

          if (!first_scan_to_display) {
            return false;  // I/O suspension
          }

          for (;;) {
            if (!info_.output_scanline) {
              int scan = info_.input_scan_number;

              // If we haven't displayed anything yet
              // (output_scan_number == 0) and we have enough data for
              // a complete scan, force output of the last full scan, but only
              // if this last scan has seen DC data from all components.
              if (!info_.output_scan_number && (scan > first_scan_to_display) &&
                  (status != JPEG_REACHED_EOI)) {
                --scan;
              }

              if (!jpeg_start_output(&info_, scan)) {
                return false;  // I/O suspension.
              }
            }

            if (info_.output_scanline == 0xffffff) {
              info_.output_scanline = 0;
            }

            if (!decoder_->OutputScanlines()) {
              if (decoder_->Failed()) {
                return false;
              }
              // If no scan lines were read, flag it so we don't call
              // jpeg_start_output() multiple times for the same scan.
              if (!info_.output_scanline) {
                info_.output_scanline = 0xffffff;
              }

              return false;  // I/O suspension.
            }

            if (info_.output_scanline == info_.output_height) {
              if (!jpeg_finish_output(&info_)) {
                return false;  // I/O suspension.
              }

              if (jpeg_input_complete(&info_) &&
                  (info_.input_scan_number == info_.output_scan_number)) {
                break;
              }

              info_.output_scanline = 0;
            }
          }

          state_ = kJpegDone;
        }
        [[fallthrough]];

      case kJpegDone:
        // Finish decompression.
        if (info_.jpeg_color_space != JCS_GRAYSCALE &&
            decoder_->IsAllDataReceived()) {
          static constexpr char kType[] = "Jpeg";
          ImageDecoder::UpdateBppHistogram<kType>(decoder_->Size(),
                                                  data_->size());
        }
        return jpeg_finish_decompress(&info_);
    }

    return true;
  }

  jpeg_decompress_struct* Info() { return &info_; }
  JSAMPARRAY Samples() const { return samples_; }
  JPEGImageDecoder* Decoder() { return decoder_; }
  gfx::Size UvSize() const { return uv_size_; }
  bool HasStartedDecompression() const { return state_ > kJpegStartDecompress; }
  SkJpegMetadataDecoder* GetMetadataDecoder() {
    return metadata_decoder_.get();
  }

 private:
#if defined(USE_SYSTEM_LIBJPEG)
  NO_SANITIZE_CFI_ICALL
#endif
  JSAMPARRAY AllocateSampleArray() {
// Some output color spaces don't need the sample array: don't allocate in that
// case.
#if defined(TURBO_JPEG_RGB_SWIZZLE)
    if (turboSwizzled(info_.out_color_space)) {
      return nullptr;
    }
#endif

    if (info_.out_color_space != JCS_YCbCr) {
      return (*info_.mem->alloc_sarray)(
          reinterpret_cast_ptr<j_common_ptr>(&info_), JPOOL_IMAGE,
          4 * info_.output_width, 1);
    }

    // Compute the width of the Y plane in bytes.  This may be larger than the
    // output width, since the jpeg library requires that the allocated width be
    // a multiple of DCTSIZE.  Note that this buffer will be used as garbage
    // memory for rows that extend below the actual height of the image.  We can
    // reuse the same memory for the U and V planes, since we are guaranteed
    // that the Y plane width is at least as large as the U and V plane widths.
    int width_bytes = ComputeYUVWidthBytes(&info_, 0);
    return (*info_.mem->alloc_sarray)(
        reinterpret_cast_ptr<j_common_ptr>(&info_), JPOOL_IMAGE, width_bytes,
        1);
  }

  void UpdateRestartPosition() {
    if (last_set_byte_ != info_.src->next_input_byte) {
      // next_input_byte was updated by jpeg, meaning that it found a restart
      // position.
      restart_position_ = static_cast<wtf_size_t>(next_read_position_ -
                                                  info_.src->bytes_in_buffer);
    }
  }

  void ClearBuffer() {
    // Let libjpeg know that the buffer needs to be refilled.
    info_.src->bytes_in_buffer = 0;
    info_.src->next_input_byte = nullptr;
    last_set_byte_ = nullptr;
  }

  scoped_refptr<SegmentReader> data_;
  raw_ptr<JPEGImageDecoder> decoder_;

  // Input reading: True if we need to back up to restart_position_.
  bool needs_restart_;
  // If libjpeg needed to restart, this is the position to restart from.
  wtf_size_t restart_position_;
  // This is the position where we will read from, unless there is a restart.
  wtf_size_t next_read_position_;
  // This is how we know to update the restart position. It is the last value
  // we set to next_input_byte. libjpeg will update next_input_byte when it
  // has found the next restart position, so if it no longer matches this
  // value, we know we've reached the next restart position.
  raw_ptr<const JOCTET> last_set_byte_;

  jpeg_decompress_struct info_;
  decoder_error_mgr err_;
  decoder_source_mgr src_;
  jpeg_progress_mgr progress_mgr_;
  jstate state_;

  // The metadata decoder is populated once the full header (all segments up to
  // the first StartOfScan) has been received.
  std::unique_ptr<SkJpegMetadataDecoder> metadata_decoder_;

  JSAMPARRAY samples_;
  gfx::Size uv_size_;
};

void error_exit(
    j_common_ptr cinfo)  // Decoding failed: return control to the setjmp point.
{
  longjmp(reinterpret_cast_ptr<decoder_error_mgr*>(cinfo->err)->setjmp_buffer,
          -1);
}

void emit_message(j_common_ptr cinfo, int msg_level) {
  if (msg_level >= 0) {
    return;
  }

  decoder_error_mgr* err = reinterpret_cast_ptr<decoder_error_mgr*>(cinfo->err);
  err->pub.num_warnings++;

  // Detect and count corrupt JPEG warning messages.
  const char* warning = nullptr;
  int code = err->pub.msg_code;
  if (code > 0 && code <= err->pub.last_jpeg_message) {
    warning = err->pub.jpeg_message_table[code];
  }
  if (warning && !strncmp("Corrupt JPEG", warning, 12)) {
    err->num_corrupt_warnings++;
  }
}

void init_source(j_decompress_ptr) {}

void skip_input_data(j_decompress_ptr jd, long num_bytes) {
  reinterpret_cast_ptr<decoder_source_mgr*>(jd->src)->reader->SkipBytes(
      num_bytes);
}

boolean fill_input_buffer(j_decompress_ptr jd) {
  return reinterpret_cast_ptr<decoder_source_mgr*>(jd->src)
      ->reader->FillBuffer();
}

void term_source(j_decompress_ptr jd) {
  reinterpret_cast_ptr<decoder_source_mgr*>(jd->src)
      ->reader->Decoder()
      ->Complete();
}

JPEGImageDecoder::JPEGImageDecoder(AlphaOption alpha_option,
                                   ColorBehavior color_behavior,
                                   cc::AuxImage aux_image,
                                   wtf_size_t max_decoded_bytes,
                                   wtf_size_t offset)
    : ImageDecoder(alpha_option,
                   ImageDecoder::kDefaultBitDepth,
                   color_behavior,
                   aux_image,
                   max_decoded_bytes),
      offset_(offset) {}

JPEGImageDecoder::~JPEGImageDecoder() = default;

String JPEGImageDecoder::FilenameExtension() const {
  return "jpg";
}

const AtomicString& JPEGImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, jpeg_mime_type, ("image/jpeg"));
  return jpeg_mime_type;
}

bool JPEGImageDecoder::SetSize(unsigned width, unsigned height) {
  if (!ImageDecoder::SetSize(width, height)) {
    return false;
  }

  if (!DesiredScaleNumerator()) {
    return SetFailed();
  }

  SetDecodedSize(width, height);
  return true;
}

void JPEGImageDecoder::OnSetData(scoped_refptr<SegmentReader> data) {
  // If we are decoding the gainmap image, replace `data` with the subset of
  // `data` that corresponds to the gainmap image itself. This strategy is
  // used because the underlying decoder is unaware of gainmap metadata, and
  // because the gainmap image itself is is a self-contained JPEG image (see
  // multi-picture format, also known as CIPA DC-007). This is in contrast with
  // other decoders (e.g AVIF), which are aware of gainmap metadata.
  if (data && aux_image_ == cc::AuxImage::kGainmap) {
    sk_sp<SkData> base_image_data = data->GetAsSkData();
    DCHECK(base_image_data);
    SkGainmapInfo gainmap_info;
    sk_sp<SkData> gainmap_image_data;
    auto base_metadata_decoder = SkJpegMetadataDecoder::Make(base_image_data);
    if (!base_metadata_decoder->findGainmapImage(
            base_image_data, gainmap_image_data, gainmap_info)) {
      SetFailed();
      return;
    }
    data = SegmentReader::CreateFromSkData(std::move(gainmap_image_data));
    data_ = data;
  }

  if (reader_) {
    reader_->SetData(std::move(data));

    // Changing YUV decoding mode is not allowed after decompression starts.
    if (reader_->HasStartedDecompression()) {
      return;
    }
  }

  if (allow_decode_to_yuv_) {
    return;
  }

  allow_decode_to_yuv_ =
      // Incremental YUV decoding is not currently supported (crbug.com/943519).
      IsAllDataReceived() &&
      // Ensures that the reader is created, the scale numbers are known,
      // the color profile is known, and the subsampling is known.
      IsSizeAvailable() &&
      // YUV decoding to a smaller size is not supported.
      reader_ && reader_->Info()->scale_num == reader_->Info()->scale_denom &&
      // TODO(crbug.com/911246): Support color space transformations on planar
      // data.
      !ColorTransform() &&
      SubsamplingSupportedByDecodeToYUV(GetYUVSubsampling());
}

gfx::Size JPEGImageDecoder::DecodedSize() const {
  return decoded_size_;
}

void JPEGImageDecoder::SetDecodedSize(unsigned width, unsigned height) {
  decoded_size_ = gfx::Size(width, height);
}

cc::YUVSubsampling JPEGImageDecoder::GetYUVSubsampling() const {
  DCHECK(reader_->Info());
  // reader_->Info() should have gone through a jpeg_read_header() call.
  DCHECK(IsDecodedSizeAvailable());
  return YuvSubsampling(*reader_->Info());
}

gfx::Size JPEGImageDecoder::DecodedYUVSize(cc::YUVIndex index) const {
  DCHECK(reader_);
  const jpeg_decompress_struct* info = reader_->Info();

  DCHECK_EQ(info->jpeg_color_space, JCS_YCbCr);
  return ComputeYUVSize(info, static_cast<int>(index));
}

wtf_size_t JPEGImageDecoder::DecodedYUVWidthBytes(cc::YUVIndex index) const {
  DCHECK(reader_);
  const jpeg_decompress_struct* info = reader_->Info();

  DCHECK_EQ(info->jpeg_color_space, JCS_YCbCr);
  return ComputeYUVWidthBytes(info, static_cast<int>(index));
}

unsigned JPEGImageDecoder::DesiredScaleNumerator() const {
  wtf_size_t original_bytes = Size().width() * Size().height() * 4;

  return JPEGImageDecoder::DesiredScaleNumerator(
      max_decoded_bytes_, original_bytes, g_scale_denominator);
}

// static
unsigned JPEGImageDecoder::DesiredScaleNumerator(wtf_size_t max_decoded_bytes,
                                                 wtf_size_t original_bytes,
                                                 unsigned scale_denominator) {
  if (original_bytes <= max_decoded_bytes) {
    return scale_denominator;
  }

  // Downsample according to the maximum decoded size.
  return static_cast<unsigned>(floor(sqrt(
      // MSVC needs explicit parameter type for sqrt().
      static_cast<float>(max_decoded_bytes) / original_bytes *
      scale_denominator * scale_denominator)));
}

bool JPEGImageDecoder::ShouldGenerateAllSizes() const {
  return supported_decode_sizes_.empty();
}

void JPEGImageDecoder::DecodeToYUV() {
  DCHECK(HasImagePlanes());
  DCHECK(CanDecodeToYUV());

  // Only 8-bit YUV decode is currently supported.
  DCHECK_EQ(image_planes_->color_type(), kGray_8_SkColorType);

  {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "Decode Image",
                 "imageType", "JPEG");
    Decode(DecodingMode::kDecodeToYuv);
  }
}

// TODO(crbug.com/919627): Confirm that this is correct for all cases.
SkYUVColorSpace JPEGImageDecoder::GetYUVColorSpace() const {
  return SkYUVColorSpace::kJPEG_SkYUVColorSpace;
}

void JPEGImageDecoder::SetSupportedDecodeSizes(Vector<SkISize> sizes) {
  supported_decode_sizes_ = std::move(sizes);
}

Vector<SkISize> JPEGImageDecoder::GetSupportedDecodeSizes() const {
  // DCHECK IsDecodedSizeAvailable instead of IsSizeAvailable, since the latter
  // has side effects of actually doing the decode.
  DCHECK(IsDecodedSizeAvailable());
  return supported_decode_sizes_;
}

bool JPEGImageDecoder::GetGainmapInfoAndData(
    SkGainmapInfo& out_gainmap_info,
    scoped_refptr<SegmentReader>& out_gainmap_data) const {
  auto* metadata_decoder = reader_ ? reader_->GetMetadataDecoder() : nullptr;
  if (!metadata_decoder) {
    return false;
  }

  if (!metadata_decoder->mightHaveGainmapImage()) {
    return false;
  }

  // TODO(crbug.com/356827770): This function will be removed once all decoders
  // rely on ImageDecoder::aux_image_ to decode the gainmap, instead of
  // extracting gainmap data.
  sk_sp<SkData> base_image_data = data_->GetAsSkData();
  DCHECK(base_image_data);
  sk_sp<SkData> gainmap_image_data;
  SkGainmapInfo gainmap_info;
  if (!metadata_decoder->findGainmapImage(base_image_data, gainmap_image_data,
                                          gainmap_info)) {
    return false;
  }
  out_gainmap_info = gainmap_info;
  out_gainmap_data = data_;
  return true;
}

gfx::Size JPEGImageDecoder::GetImageCodedSize() const {
  // We use the |max_{h,v}_samp_factor|s returned by
  // AreValidSampleFactorsAvailable() since the ones available via
  // Info()->max_{h,v}_samp_factor are not updated until the image is actually
  // being decoded.
  int max_h_samp_factor;
  int max_v_samp_factor;
  if (!reader_->AreValidSampleFactorsAvailable(&max_h_samp_factor,
                                               &max_v_samp_factor)) {
    return gfx::Size();
  }

  const int coded_width = Align(Size().width(), max_h_samp_factor * 8);
  const int coded_height = Align(Size().height(), max_v_samp_factor * 8);

  return gfx::Size(coded_width, coded_height);
}

void JPEGImageDecoder::DecodeSize() {
  Decode(DecodingMode::kDecodeHeader);
}

void JPEGImageDecoder::Decode(wtf_size_t) {
  // Use DecodeToYUV for YUV decoding.
  Decode(DecodingMode::kDecodeToBitmap);
}

cc::ImageHeaderMetadata JPEGImageDecoder::MakeMetadataForDecodeAcceleration()
    const {
  cc::ImageHeaderMetadata image_metadata =
      ImageDecoder::MakeMetadataForDecodeAcceleration();
  image_metadata.jpeg_is_progressive = reader_->Info()->buffered_image;
  image_metadata.coded_size = GetImageCodedSize();
  return image_metadata;
}

// At the moment we support only JCS_RGB and JCS_CMYK values of the
// J_COLOR_SPACE enum.
// If you need a specific implementation for other J_COLOR_SPACE values,
// please add a full template specialization for this function below.
template <J_COLOR_SPACE colorSpace>
void SetPixel(ImageFrame::PixelData*, JSAMPARRAY samples, int column) = delete;

// Used only for debugging with libjpeg (instead of libjpeg-turbo).
template <>
void SetPixel<JCS_RGB>(ImageFrame::PixelData* pixel,
                       JSAMPARRAY samples,
                       int column) {
  JSAMPLE* jsample = *samples + column * 3;
  ImageFrame::SetRGBARaw(pixel, jsample[0], jsample[1], jsample[2], 255);
}

template <>
void SetPixel<JCS_CMYK>(ImageFrame::PixelData* pixel,
                        JSAMPARRAY samples,
                        int column) {
  JSAMPLE* jsample = *samples + column * 4;

  // Source is 'Inverted CMYK', output is RGB.
  // See: http://www.easyrgb.com/math.php?MATH=M12#text12
  // Or: http://www.ilkeratalay.com/colorspacesfaq.php#rgb
  // From CMYK to CMY:
  // X =   X    * (1 -   K   ) +   K  [for X = C, M, or Y]
  // Thus, from Inverted CMYK to CMY is:
  // X = (1-iX) * (1 - (1-iK)) + (1-iK) => 1 - iX*iK
  // From CMY (0..1) to RGB (0..1):
  // R = 1 - C => 1 - (1 - iC*iK) => iC*iK  [G and B similar]
  unsigned k = jsample[3];
  ImageFrame::SetRGBARaw(pixel, jsample[0] * k / 255, jsample[1] * k / 255,
                         jsample[2] * k / 255, 255);
}

// Used only for JCS_CMYK and JCS_RGB output.  Note that JCS_RGB is used only
// for debugging with libjpeg (instead of libjpeg-turbo).
template <J_COLOR_SPACE colorSpace>
bool OutputRows(JPEGImageReader* reader, ImageFrame& buffer) {
  JSAMPARRAY samples = reader->Samples();
  jpeg_decompress_struct* info = reader->Info();
  int width = info->output_width;

  while (info->output_scanline < info->output_height) {
    // jpeg_read_scanlines will increase the scanline counter, so we
    // save the scanline before calling it.
    int y = info->output_scanline;
    // Request one scanline: returns 0 or 1 scanlines.
    if (jpeg_read_scanlines(info, samples, 1) != 1) {
      return false;
    }

    ImageFrame::PixelData* pixel = buffer.GetAddr(0, y);
    for (int x = 0; x < width; ++pixel, ++x) {
      SetPixel<colorSpace>(pixel, samples, x);
    }

    ColorProfileTransform* xform = reader->Decoder()->ColorTransform();
    if (xform) {
      ImageFrame::PixelData* row = buffer.GetAddr(0, y);
      skcms_AlphaFormat alpha_format = skcms_AlphaFormat_Unpremul;
      bool color_conversion_successful = skcms_Transform(
          row, XformColorFormat(), alpha_format, xform->SrcProfile(), row,
          XformColorFormat(), alpha_format, xform->DstProfile(), width);
      DCHECK(color_conversion_successful);
    }
  }

  buffer.SetPixelsChanged(true);
  return true;
}

static bool OutputRawData(JPEGImageReader* reader, ImagePlanes* image_planes) {
  JSAMPARRAY samples = reader->Samples();
  jpeg_decompress_struct* info = reader->Info();

  DCHECK_EQ(info->out_color_space, JCS_YCbCr);

  JSAMPARRAY bufferraw[3];
  JSAMPROW bufferraw2[32];
  bufferraw[0] = &bufferraw2[0];   // Y channel rows (8 or 16)
  bufferraw[1] = &bufferraw2[16];  // U channel rows (8)
  bufferraw[2] = &bufferraw2[24];  // V channel rows (8)
  int y_height = info->output_height;
  int v = info->comp_info[0].v_samp_factor;
  gfx::Size uv_size = reader->UvSize();
  int uv_height = uv_size.height();
  JSAMPROW output_y =
      static_cast<JSAMPROW>(image_planes->Plane(cc::YUVIndex::kY));
  JSAMPROW output_u =
      static_cast<JSAMPROW>(image_planes->Plane(cc::YUVIndex::kU));
  JSAMPROW output_v =
      static_cast<JSAMPROW>(image_planes->Plane(cc::YUVIndex::kV));
  wtf_size_t row_bytes_y = image_planes->RowBytes(cc::YUVIndex::kY);
  wtf_size_t row_bytes_u = image_planes->RowBytes(cc::YUVIndex::kU);
  wtf_size_t row_bytes_v = image_planes->RowBytes(cc::YUVIndex::kV);

  // Request 8 or 16 scanlines: returns 0 or more scanlines.
  int y_scanlines_to_read = DCTSIZE * v;
  JSAMPROW dummy_row = *samples;
  while (info->output_scanline < info->output_height) {
    // Assign 8 or 16 rows of memory to read the Y channel.
    for (int i = 0; i < y_scanlines_to_read; ++i) {
      int scanline = info->output_scanline + i;
      if (scanline < y_height) {
        bufferraw2[i] = &output_y[scanline * row_bytes_y];
      } else {
        bufferraw2[i] = dummy_row;
      }
    }

    // Assign 8 rows of memory to read the U and V channels.
    int scaled_scanline = info->output_scanline / v;
    for (int i = 0; i < 8; ++i) {
      int scanline = scaled_scanline + i;
      if (scanline < uv_height) {
        bufferraw2[16 + i] = &output_u[scanline * row_bytes_u];
        bufferraw2[24 + i] = &output_v[scanline * row_bytes_v];
      } else {
        bufferraw2[16 + i] = dummy_row;
        bufferraw2[24 + i] = dummy_row;
      }
    }

    JDIMENSION scanlines_read =
        jpeg_read_raw_data(info, bufferraw, y_scanlines_to_read);
    if (!scanlines_read) {
      return false;
    }
  }

  info->output_scanline = std::min(info->output_scanline, info->output_height);
  image_planes->SetHasCompleteScan();
  return true;
}

bool JPEGImageDecoder::OutputScanlines() {
  if (HasImagePlanes()) {
    return OutputRawData(reader_.get(), image_planes_.get());
  }

  if (frame_buffer_cache_.empty()) {
    return false;
  }

  jpeg_decompress_struct* info = reader_->Info();

  // Initialize the framebuffer if needed.
  ImageFrame& buffer = frame_buffer_cache_[0];
  if (buffer.GetStatus() == ImageFrame::kFrameEmpty) {
    DCHECK_EQ(info->output_width,
              static_cast<JDIMENSION>(decoded_size_.width()));
    DCHECK_EQ(info->output_height,
              static_cast<JDIMENSION>(decoded_size_.height()));

    if (!buffer.AllocatePixelData(info->output_width, info->output_height,
                                  ColorSpaceForSkImages())) {
      return SetFailed();
    }

    buffer.ZeroFillPixelData();
    // The buffer is transparent outside the decoded area while the image is
    // loading. The image will be marked fully opaque in Complete().
    buffer.SetStatus(ImageFrame::kFramePartial);
    buffer.SetHasAlpha(true);

    // For JPEGs, the frame always fills the entire image.
    buffer.SetOriginalFrameRect(gfx::Rect(Size()));
  }

#if defined(TURBO_JPEG_RGB_SWIZZLE)
  if (turboSwizzled(info->out_color_space)) {
    while (info->output_scanline < info->output_height) {
      unsigned char* row = reinterpret_cast_ptr<unsigned char*>(
          buffer.GetAddr(0, info->output_scanline));
      if (jpeg_read_scanlines(info, &row, 1) != 1) {
        return false;
      }

      ColorProfileTransform* xform = ColorTransform();
      if (xform) {
        skcms_AlphaFormat alpha_format = skcms_AlphaFormat_Unpremul;
        bool color_conversion_successful = skcms_Transform(
            row, XformColorFormat(), alpha_format, xform->SrcProfile(), row,
            XformColorFormat(), alpha_format, xform->DstProfile(),
            info->output_width);
        DCHECK(color_conversion_successful);
      }
    }
    buffer.SetPixelsChanged(true);
    return true;
  }
#endif

  switch (info->out_color_space) {
    case JCS_RGB:
      return OutputRows<JCS_RGB>(reader_.get(), buffer);
    case JCS_CMYK:
      return OutputRows<JCS_CMYK>(reader_.get(), buffer);
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return SetFailed();
}

void JPEGImageDecoder::Complete() {
  if (frame_buffer_cache_.empty()) {
    return;
  }

  frame_buffer_cache_[0].SetHasAlpha(false);
  frame_buffer_cache_[0].SetStatus(ImageFrame::kFrameComplete);
}

inline bool IsComplete(const JPEGImageDecoder* decoder,
                       JPEGImageDecoder::DecodingMode decoding_mode) {
  if (decoding_mode == JPEGImageDecoder::DecodingMode::kDecodeToYuv) {
    DCHECK(decoder->HasImagePlanes());
    return true;
  }

  return decoder->FrameIsDecodedAtIndex(0);
}

void JPEGImageDecoder::Decode(DecodingMode decoding_mode) {
  if (Failed()) {
    return;
  }

  if (!reader_) {
    reader_ = std::make_unique<JPEGImageReader>(this, offset_);
    reader_->SetData(data_);
  }

  // If we couldn't decode the image but have received all the data, decoding
  // has failed.
  if (!reader_->Decode(decoding_mode) && IsAllDataReceived()) {
    SetFailed();
  }

  // If decoding is done or failed, we don't need the JPEGImageReader anymore.
  if (IsComplete(this, decoding_mode) || Failed()) {
    reader_.reset();
  }
}

}  // namespace blink
