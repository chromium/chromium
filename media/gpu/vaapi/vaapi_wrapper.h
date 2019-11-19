// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of VaapiWrapper, used by
// VaapiVideoDecodeAccelerator and VaapiH264Decoder for decode,
// and VaapiVideoEncodeAccelerator for encode, to interface
// with libva (VA-API library for hardware video codec).

#ifndef MEDIA_GPU_VAAPI_VAAPI_WRAPPER_H_
#define MEDIA_GPU_VAAPI_VAAPI_WRAPPER_H_

#include <stddef.h>
#include <stdint.h>
#include <va/va.h>

#include <memory>
#include <set>
#include <vector>

#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11.h"
#endif  // USE_X11

namespace gfx {
enum class BufferFormat;
class NativePixmap;
class NativePixmapDmaBuf;
}

namespace media {
constexpr unsigned int kInvalidVaRtFormat = 0u;

class ScopedVAImage;
class ScopedVASurface;
class VideoFrame;

// This struct holds a NativePixmapDmaBuf, usually the result of exporting a VA
// surface, and some associated size information needed to tell clients about
// the underlying buffer.
struct NativePixmapAndSizeInfo {
  NativePixmapAndSizeInfo();
  ~NativePixmapAndSizeInfo();

  // The VA-API internal buffer dimensions, which may be different than the
  // dimensions requested at the time of creation of the surface (but always
  // larger than or equal to those). This can be used for validation in, e.g.,
  // testing.
  gfx::Size va_surface_resolution;

  // The size of the underlying Buffer Object. A use case for this is when an
  // image decode is requested and the caller needs to know the size of the
  // allocated buffer for caching purposes.
  size_t byte_size = 0u;

  // Contains the information needed to use the surface in a graphics API,
  // including the visible size (|pixmap|->GetBufferSize()) which should be no
  // larger than |va_surface_resolution|.
  scoped_refptr<gfx::NativePixmapDmaBuf> pixmap;
};

enum class VAImplementation {
  kMesaGallium,
  kIntelI965,
  kIntelIHD,
  kOther,
  kInvalid,
};

// This class handles VA-API calls and ensures proper locking of VA-API calls
// to libva, the userspace shim to the HW codec driver. libva is not
// thread-safe, so we have to perform locking ourselves. This class is fully
// synchronous and its methods can be called from any thread and may wait on
// the va_lock_ while other, concurrent calls run.
//
// This class is responsible for managing VAAPI connection, contexts and state.
// It is also responsible for managing and freeing VABuffers (not VASurfaces),
// which are used to queue parameters and slice data to the HW codec,
// as well as underlying memory for VASurfaces themselves.
class MEDIA_GPU_EXPORT VaapiWrapper
    : public base::RefCountedThreadSafe<VaapiWrapper> {
 public:
  enum CodecMode {
    kDecode,
    kEncode,
    kVideoProcess,
    kCodecModeMax,
  };

  // This is enum associated with VASurfaceAttribUsageHint.
  enum class SurfaceUsageHint : uint8_t {
    kVideoDecoder,
    kVideoEncoder,
    kVideoProcessWrite,
    kGeneric,
  };

  using InternalFormats = struct {
    bool yuv420 : 1;
    bool yuv422 : 1;
    bool yuv444 : 1;
  };

  // Returns the type of the underlying VA-API implementation.
  static VAImplementation GetImplementationType();

  // Return an instance of VaapiWrapper initialized for |va_profile| and
  // |mode|. |report_error_to_uma_cb| will be called independently from
  // reporting errors to clients via method return values.
  static scoped_refptr<VaapiWrapper> Create(
      CodecMode mode,
      VAProfile va_profile,
      const base::Closure& report_error_to_uma_cb);

  // Create VaapiWrapper for VideoCodecProfile. It maps VideoCodecProfile
  // |profile| to VAProfile.
  // |report_error_to_uma_cb| will be called independently from reporting
  // errors to clients via method return values.
  static scoped_refptr<VaapiWrapper> CreateForVideoCodec(
      CodecMode mode,
      VideoCodecProfile profile,
      const base::Closure& report_error_to_uma_cb);

  // Return the supported video encode profiles.
  static VideoEncodeAccelerator::SupportedProfiles GetSupportedEncodeProfiles();

  // Return the supported video decode profiles.
  static VideoDecodeAccelerator::SupportedProfiles GetSupportedDecodeProfiles();

  // Return true when decoding using |va_profile| is supported.
  static bool IsDecodeSupported(VAProfile va_profile);

  // Returns the supported internal formats for decoding using |va_profile|. If
  // decoding is not supported for that profile, returns InternalFormats{}.
  static InternalFormats GetDecodeSupportedInternalFormats(
      VAProfile va_profile);

  // Returns true if |rt_format| is supported for decoding using |va_profile|.
  // Returns false if |rt_format| or |va_profile| is not supported for decoding.
  static bool IsDecodingSupportedForInternalFormat(VAProfile va_profile,
                                                   unsigned int rt_format);

  // Gets the minimum surface size allowed for decoding using |va_profile|.
  // Returns true if the size can be obtained, false otherwise. The minimum
  // dimension (width or height) returned is 1. Particularly, if a dimension is
  // not reported by the driver, the dimension is returned as 1.
  static bool GetDecodeMinResolution(VAProfile va_profile, gfx::Size* min_size);

  // Gets the maximum surface size allowed for decoding using |va_profile|.
  // Returns true if the size can be obtained, false otherwise. Because of the
  // initialization in VASupportedProfiles::FillProfileInfo_Locked(), the size
  // is guaranteed to not be empty (as long as this method returns true).
  static bool GetDecodeMaxResolution(VAProfile va_profile, gfx::Size* max_size);

  // Obtains a suitable FOURCC that can be used in vaCreateImage() +
  // vaGetImage(). |rt_format| corresponds to the JPEG's subsampling format.
  // |preferred_fourcc| is the FOURCC of the format preferred by the caller. If
  // it is determined that the VAAPI driver can do the conversion from the
  // internal format (|rt_format|), *|suitable_fourcc| is set to
  // |preferred_fourcc|. Otherwise, it is set to a supported format. Returns
  // true if a suitable FOURCC could be determined, false otherwise (e.g., if
  // the |rt_format| is unsupported by the driver). If |preferred_fourcc| is not
  // a supported image format, *|suitable_fourcc| is set to VA_FOURCC_I420.
  static bool GetJpegDecodeSuitableImageFourCC(unsigned int rt_format,
                                               uint32_t preferred_fourcc,
                                               uint32_t* suitable_fourcc);

  // Checks the surface size is allowed for VPP. Returns true if the size is
  // supported, false otherwise.
  static bool IsVppResolutionAllowed(const gfx::Size& size);

  // Returns true if the VPP supports converting from/to |fourcc|.
  static bool IsVppFormatSupported(uint32_t fourcc);

  // Returns true if VPP supports the format conversion from a JPEG decoded
  // internal surface to a FOURCC. |rt_format| corresponds to the JPEG's
  // subsampling format. |fourcc| is the output surface's FOURCC.
  static bool IsVppSupportedForJpegDecodedSurfaceToFourCC(
      unsigned int rt_format,
      uint32_t fourcc);

  // Return true when JPEG encode is supported.
  static bool IsJpegEncodeSupported();

  // Return true when the specified image format is supported.
  static bool IsImageFormatSupported(const VAImageFormat& format);

  // Returns the list of VAImageFormats supported by the driver.
  static const std::vector<VAImageFormat>& GetSupportedImageFormatsForTesting();

  static uint32_t BufferFormatToVARTFormat(gfx::BufferFormat fmt);

  // Creates |num_surfaces| VASurfaceIDs of |va_format|, |size| and
  // |surface_usage_hint| and, if successful, creates a |va_context_id_| of the
  // same size. |surface_usage_hint| may affect an alignment and tiling of the
  // created surface. Returns true if successful, with the created IDs in
  // |va_surfaces|. The client is responsible for destroying |va_surfaces| via
  // DestroyContextAndSurfaces() to free the allocated surfaces.
  virtual bool CreateContextAndSurfaces(unsigned int va_format,
                                        const gfx::Size& size,
                                        SurfaceUsageHint surface_usage_hint,
                                        size_t num_surfaces,
                                        std::vector<VASurfaceID>* va_surfaces);

  // Creates a single ScopedVASurface of |va_format| and |size| and, if
  // successful, creates a |va_context_id_| of the same size. Returns nullptr if
  // creation failed. If |visible_size| is supplied, the returned
  // ScopedVASurface's size is set to it. Otherwise, it's set to |size| (refer
  // to CreateScopedVASurface() for details).
  std::unique_ptr<ScopedVASurface> CreateContextAndScopedVASurface(
      unsigned int va_format,
      const gfx::Size& size,
      const base::Optional<gfx::Size>& visible_size = base::nullopt);

  // Releases the |va_surfaces| and destroys |va_context_id_|.
  virtual void DestroyContextAndSurfaces(std::vector<VASurfaceID> va_surfaces);

  // Creates a VA Context of |size| and sets |va_context_id_|. The client is
  // responsible for releasing it via DestroyContext() or
  // DestroyContextAndSurfaces(), or it will be released on dtor.
  virtual bool CreateContext(const gfx::Size& size);

  // Destroys the context identified by |va_context_id_|.
  void DestroyContext();

  // Requests a VA surface of size |size| and |va_rt_format|. Returns a
  // self-cleaning ScopedVASurface or nullptr if creation failed. If
  // |visible_size| is supplied, the returned ScopedVASurface's size is set to
  // it: for example, we may want to request a 16x16 surface to decode a 13x12
  // JPEG: we may want to keep track of the visible size 13x12 inside the
  // ScopedVASurface to inform the surface's users that that's the only region
  // with meaningful content. If |visible_size| is not supplied, we store |size|
  // in the returned ScopedVASurface.
  std::unique_ptr<ScopedVASurface> CreateScopedVASurface(
      unsigned int va_rt_format,
      const gfx::Size& size,
      const base::Optional<gfx::Size>& visible_size = base::nullopt);

  // Creates a self-releasing VASurface from |frame|. The created VASurface
  // doesn't have the ownership of |frame|, while it shares the ownership of the
  // underlying buffer represented by |frame|. In other words, the buffer is
  // alive at least until both |frame| and the created VASurface are destroyed.
  scoped_refptr<VASurface> CreateVASurfaceForVideoFrame(
      const VideoFrame* frame);

  // Creates a self-releasing VASurface from |pixmap|. The created VASurface
  // shares the ownership of the underlying buffer represented by |pixmap|. The
  // ownership of the surface is transferred to the caller. A caller can destroy
  // |pixmap| after this method returns and the underlying buffer will be kept
  // alive by the VASurface.
  scoped_refptr<VASurface> CreateVASurfaceForPixmap(
      scoped_refptr<gfx::NativePixmap> pixmap);

  // Syncs and exports |va_surface| as a gfx::NativePixmapDmaBuf. Currently, the
  // only VAAPI surface pixel formats supported are VA_FOURCC_IMC3 and
  // VA_FOURCC_NV12.
  //
  // Notes:
  //
  // - For VA_FOURCC_IMC3, the format of the returned NativePixmapDmaBuf is
  //   gfx::BufferFormat::YVU_420 because we don't have a YUV_420 format. The
  //   planes are flipped accordingly, i.e.,
  //   gfx::NativePixmapDmaBuf::GetDmaBufOffset(1) refers to the V plane.
  //   TODO(andrescj): revisit once crrev.com/c/1573718 lands.
  //
  // - For VA_FOURCC_NV12, the format of the returned NativePixmapDmaBuf is
  //   gfx::BufferFormat::YUV_420_BIPLANAR.
  //
  // Returns nullptr on failure.
  std::unique_ptr<NativePixmapAndSizeInfo> ExportVASurfaceAsNativePixmapDmaBuf(
      const ScopedVASurface& va_surface);

  // Synchronize the VASurface explicitly. This is useful when sharing a surface
  // between contexts.
  bool SyncSurface(VASurfaceID va_surface_id);

  // Submit parameters or slice data of |va_buffer_type|, copying them from
  // |buffer| of size |size|, into HW codec. The data in |buffer| is no
  // longer needed and can be freed after this method returns.
  // Data submitted via this method awaits in the HW codec until
  // ExecuteAndDestroyPendingBuffers() is called to execute or
  // DestroyPendingBuffers() is used to cancel a pending job.
  bool SubmitBuffer(VABufferType va_buffer_type,
                    size_t size,
                    const void* buffer);

  // Convenient templatized version of SubmitBuffer() where |size| is deduced to
  // be the size of the type of |*buffer|.
  template <typename T>
  bool SubmitBuffer(VABufferType va_buffer_type, const T* buffer) {
    return SubmitBuffer(va_buffer_type, sizeof(T), buffer);
  }

  // Submit a VAEncMiscParameterBuffer of given |misc_param_type|, copying its
  // data from |buffer| of size |size|, into HW codec. The data in |buffer| is
  // no longer needed and can be freed after this method returns.
  // Data submitted via this method awaits in the HW codec until
  // ExecuteAndDestroyPendingBuffers() is called to execute or
  // DestroyPendingBuffers() is used to cancel a pending job.
  bool SubmitVAEncMiscParamBuffer(VAEncMiscParameterType misc_param_type,
                                  size_t size,
                                  const void* buffer);

  // Cancel and destroy all buffers queued to the HW codec via SubmitBuffer().
  // Useful when a pending job is to be cancelled (on reset or error).
  void DestroyPendingBuffers();

  // Execute job in hardware on target |va_surface_id| and destroy pending
  // buffers. Return false if Execute() fails.
  bool ExecuteAndDestroyPendingBuffers(VASurfaceID va_surface_id);

#if defined(USE_X11)
  // Put data from |va_surface_id| into |x_pixmap| of size
  // |dest_size|, converting/scaling to it.
  bool PutSurfaceIntoPixmap(VASurfaceID va_surface_id,
                            Pixmap x_pixmap,
                            gfx::Size dest_size);
#endif  // USE_X11

  // Creates a ScopedVAImage from a VASurface |va_surface_id| and map it into
  // memory with the given |format| and |size|. If |format| is not equal to the
  // internal format, the underlying implementation will do format conversion if
  // supported. |size| should be smaller than or equal to the surface. If |size|
  // is smaller, the image will be cropped.
  std::unique_ptr<ScopedVAImage> CreateVaImage(VASurfaceID va_surface_id,
                                               VAImageFormat* format,
                                               const gfx::Size& size);

  // Upload contents of |frame| into |va_surface_id| for encode.
  bool UploadVideoFrameToSurface(const VideoFrame& frame,
                                 VASurfaceID va_surface_id);

  // Create a buffer of |size| bytes to be used as encode output.
  bool CreateVABuffer(size_t size, VABufferID* buffer_id);

  // Download the contents of the buffer with given |buffer_id| into a buffer of
  // size |target_size|, pointed to by |target_ptr|. The number of bytes
  // downloaded will be returned in |coded_data_size|. |sync_surface_id| will
  // be used as a sync point, i.e. it will have to become idle before starting
  // the download. |sync_surface_id| should be the source surface passed
  // to the encode job.
  bool DownloadFromVABuffer(VABufferID buffer_id,
                            VASurfaceID sync_surface_id,
                            uint8_t* target_ptr,
                            size_t target_size,
                            size_t* coded_data_size);

  // Deletes the VA buffer identified by |buffer_id|.
  void DestroyVABuffer(VABufferID buffer_id);

  // Destroy all previously-allocated (and not yet destroyed) buffers.
  void DestroyVABuffers();

  // Get the max number of reference frames for encoding supported by the
  // driver.
  // For H.264 encoding, the value represents the maximum number of reference
  // frames for both the reference picture list 0 (bottom 16 bits) and the
  // reference picture list 1 (top 16 bits).
  bool GetVAEncMaxNumOfRefFrames(VideoCodecProfile profile,
                                 size_t* max_ref_frames);

  // Blits a VASurface |va_surface_src| into another VASurface
  // |va_surface_dest| applying pixel format conversion and scaling
  // if needed.
  bool BlitSurface(const scoped_refptr<VASurface>& va_surface_src,
                   const scoped_refptr<VASurface>& va_surface_dest);

  // Initialize static data before sandbox is enabled.
  static void PreSandboxInitialization();

  // vaDestroySurfaces() a vector or a single VASurfaceID.
  void DestroySurfaces(std::vector<VASurfaceID> va_surfaces);
  void DestroySurface(VASurfaceID va_surface_id);

 protected:
  VaapiWrapper(CodecMode mode);
  virtual ~VaapiWrapper();

 private:
  friend class base::RefCountedThreadSafe<VaapiWrapper>;

  FRIEND_TEST_ALL_PREFIXES(VaapiUtilsTest, ScopedVAImage);
  FRIEND_TEST_ALL_PREFIXES(VaapiUtilsTest, BadScopedVAImage);
  FRIEND_TEST_ALL_PREFIXES(VaapiUtilsTest, BadScopedVABufferMapping);

  bool Initialize(CodecMode mode, VAProfile va_profile);
  void Deinitialize();
  bool VaInitialize(const base::Closure& report_error_to_uma_cb);

  // Tries to allocate |num_surfaces| VASurfaceIDs of |size| and |va_format|.
  // Fills |va_surfaces| and returns true if successful, or returns false.
  bool CreateSurfaces(unsigned int va_format,
                      const gfx::Size& size,
                      SurfaceUsageHint usage_hint,
                      size_t num_surfaces,
                      std::vector<VASurfaceID>* va_surfaces);

  // Execute pending job in hardware and destroy pending buffers. Return false
  // if vaapi driver refuses to accept parameter or slice buffers submitted
  // by client, or if execution fails in hardware.
  bool Execute(VASurfaceID va_surface_id);
  bool Execute_Locked(VASurfaceID va_surface_id)
      EXCLUSIVE_LOCKS_REQUIRED(va_lock_);

  void DestroyPendingBuffers_Locked() EXCLUSIVE_LOCKS_REQUIRED(va_lock_);

  // Attempt to set render mode to "render to texture.". Failure is non-fatal.
  void TryToSetVADisplayAttributeToLocalGPU();

  // Check low-power encode support for the given profile
  bool IsLowPowerEncSupported(VAProfile va_profile) const;

  const CodecMode mode_;

  // Pointer to VADisplayState's member |va_lock_|. Guaranteed to be valid for
  // the lifetime of VaapiWrapper.
  base::Lock* va_lock_;

  // VA handles.
  // All valid after successful Initialize() and until Deinitialize().
  VADisplay va_display_ GUARDED_BY(va_lock_);
  VAConfigID va_config_id_;
  // Created in CreateContext() or CreateContextAndSurfaces() and valid until
  // DestroyContext() or DestroyContextAndSurfaces().
  VAContextID va_context_id_;

  // Data queued up for HW codec, to be committed on next execution.
  std::vector<VABufferID> pending_slice_bufs_;
  std::vector<VABufferID> pending_va_bufs_;

  // Buffers for kEncode or kVideoProcess.
  std::set<VABufferID> va_buffers_;

  // Called to report codec errors to UMA. Errors to clients are reported via
  // return values from public methods.
  base::Closure report_error_to_uma_cb_;

  DISALLOW_COPY_AND_ASSIGN(VaapiWrapper);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_WRAPPER_H_
