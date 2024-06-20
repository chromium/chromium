// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_H265_VAAPI_WRAPPER_H_
#define MEDIA_GPU_VAAPI_TEST_H265_VAAPI_WRAPPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "media/gpu/vaapi/test/h265_dpb.h"
#include "media/gpu/vaapi/test/scoped_va_config.h"
#include "media/gpu/vaapi/test/scoped_va_context.h"
#include "media/gpu/vaapi/test/vaapi_device.h"
#include "media/parsers/h265_parser.h"

namespace media::vaapi_test {

// TODO(b/241479848): Revisit the decoder implementations for each codec
// for refactoring out pieces that can be shared between the browser and
// the test binary.
class H265VaapiWrapper {
 public:
  explicit H265VaapiWrapper(const VaapiDevice& va_device);

  H265VaapiWrapper(const H265VaapiWrapper&) = delete;
  H265VaapiWrapper& operator=(const H265VaapiWrapper&) = delete;

  ~H265VaapiWrapper();

  // Generates a picture object with a SharedVASurface.
  scoped_refptr<H265Picture> CreateH265Picture(const H265SPS* sps);
  bool SubmitFrameMetadata(const H265SPS* sps,
                           const H265PPS* pps,
                           const H265SliceHeader* slice_hdr,
                           const H265Picture::Vector& ref_pic_list,
                           scoped_refptr<H265Picture> pic);
  bool SubmitSlice(const H265SPS* sps,
                   const H265PPS* pps,
                   const H265SliceHeader* slice_hdr,
                   const H265Picture::Vector& ref_pic_list0,
                   const H265Picture::Vector& ref_pic_list1,
                   const H265Picture::Vector& ref_pic_set_lt_curr,
                   const H265Picture::Vector& ref_pic_set_st_curr_after,
                   const H265Picture::Vector& ref_pic_set_st_curr_before,
                   scoped_refptr<H265Picture> pic,
                   const uint8_t* data,
                   size_t size,
                   const std::vector<SubsampleEntry>& subsamples);
  bool SubmitDecode(scoped_refptr<H265Picture> pic);
  // Reset any current state that may be cached in the accelerator, dropping
  // any cached parameters/slices that have not been committed yet.
  void Reset();
  bool IsChromaSamplingSupported(VideoChromaSampling chroma_sampling);

 private:
  void FillVAPicture(VAPictureHEVC* va_pic, scoped_refptr<H265Picture> pic);
  void FillVARefFramesFromRefList(const H265Picture::Vector& ref_pic_list,
                                  VAPictureHEVC* va_pics);

  // Returns |kInvalidRefPicIndex| if it cannot find a picture.
  int GetRefPicIndex(int poc);

  // Submits the slice data to the decoder for the prior slice that was just
  // submitted to us. This allows us to handle multi-slice pictures properly.
  // |last_slice| is set to true when submitting the last slice, false
  // otherwise.
  bool SubmitPriorSliceDataIfPresent(bool last_slice);

  static VAProfile GetProfile(const H265SPS* sps);
  unsigned int GetFormatForProfile(const VAProfile& profile);

  [[nodiscard]] bool SubmitBuffer(VABufferType va_buffer_type,
                                  size_t size,
                                  const void* data);

  // Convenient templatized version of SubmitBuffer() where |size| is deduced to
  // be the size of the type of |*data|.
  template <typename T>
  [[nodiscard]] bool SubmitBuffer(VABufferType va_buffer_type, const T* data) {
    return SubmitBuffer(va_buffer_type, sizeof(T), data);
  }
  // Batch-version of SubmitBuffer(), where the lock for accessing libva is
  // acquired only once.
  struct VABufferDescriptor {
    VABufferType type;
    size_t size;
    raw_ptr<const void> data;
  };
  [[nodiscard]] bool SubmitBuffers(
      const std::vector<VABufferDescriptor>& va_buffers);

  // Destroys all |pending_va_buffers_| sent via SubmitBuffer*(). Useful when a
  // pending job is to be cancelled (on reset or error).
  void DestroyPendingBuffers();

  // Executes job in hardware on target |va_surface_id| and destroys pending
  // buffers. Returns false if Execute() fails.
  [[nodiscard]] bool ExecuteAndDestroyPendingBuffers(VASurfaceID va_surface_id);

  // Stores the POCs (picture order counts) in the ReferenceFrames submitted as
  // the frame metadata so we can determine the indices for the reference frames
  // in the slice metadata.
  std::vector<int> ref_pic_list_pocs_;

  // Data from the prior/current slice for handling multi slice so we can
  // properly set the flag for the last slice.
  VASliceParameterBufferHEVC slice_param_;
  // |last_slice_data_| being not empty indicates we have a valid |slice_param_|
  // filled.
  std::vector<uint8_t> last_slice_data_;

  // Data queued up for the HW decode and will be committed on the next
  // execution.
  std::vector<VABufferID> pending_buffers_;

  const raw_ref<const VaapiDevice> va_device_;
  std::unique_ptr<ScopedVAConfig> va_config_;
  std::unique_ptr<ScopedVAContext> va_context_;
};

}  // namespace media::vaapi_test

#endif  // MEDIA_GPU_VAAPI_TEST_H265_VAAPI_WRAPPER_H_
