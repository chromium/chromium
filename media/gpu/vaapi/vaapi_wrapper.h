// Copyright 2013 The Chromium Authors
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

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/atomic_ref_count.h"
#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "build/chromeos_buildflags.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
enum class BufferFormat : uint8_t;
class NativePixmap;
class NativePixmapDmaBuf;
class Rect;
}

#define MAYBE_ASSERT_ACQUIRED(lock) \
  if (lock)                         \
    lock->AssertAcquired()

#if DCHECK_IS_ON()
#define VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker) \
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker)
#else
#define VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker) \
  CHECK(sequence_checker.CalledOnValidSequence())
#endif

namespace media {
constexpr unsigned int kInvalidVaRtFormat = 0u;

class VADisplayStateSingleton;
class VideoFrame;
class FrameResource;

// Enum, function and callback type to allow VaapiWrapper to log errors in VA
// function calls executed on behalf of its owner. |histogram_name| is prebound
// to allow for disinguishing such owners.
enum class VaapiFunctions;
void ReportVaapiErrorToUMA(const std::string& histogram_name,
                           VaapiFunctions value);
using ReportErrorToUMACB = base::RepeatingCallback<void(VaapiFunctions)>;

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
  kChromiumFakeDriver,
  kOther,
  kInvalid,
};

// A VADisplayStateHandle is somewhat like a scoped_refptr for a
// VADisplayStateSingleton (an internal class used to keep track of a singleton
// VADisplay). As long as a non-null VADisplayStateHandle exists, the underlying
// VADisplay is initialized and can be used. When the last non-null
// VADisplayStateHandle is destroyed, the underlying VADisplay is cleaned up.
//
// Unlike a scoped_refptr, a VADisplayStateHandle is move-only.
//
// Note: a VADisplayStateHandle instance is thread- and sequence-safe, but the
// underlying VADisplay may need protection. See the comments for the
// VADisplayStateSingleton documentation.
class VADisplayStateHandle {
 public:
  // Creates a null VADisplayStateHandle.
  VADisplayStateHandle();

  VADisplayStateHandle(VADisplayStateHandle&& other) = default;
  VADisplayStateHandle& operator=(VADisplayStateHandle&& other) = default;

  VADisplayStateHandle(const VADisplayStateHandle&) = delete;
  VADisplayStateHandle& operator=(const VADisplayStateHandle&) = delete;

  ~VADisplayStateHandle();

  VADisplayStateSingleton* operator->() { return va_display_state_; }

  explicit operator bool() const { return !!va_display_state_; }

 private:
  friend class VADisplayStateSingleton;

  explicit VADisplayStateHandle(VADisplayStateSingleton* va_display_state);

  raw_ptr<VADisplayStateSingleton> va_display_state_;
};

// This class handles VA-API calls and ensures proper locking of VA-API calls
// to libva, the userspace shim to the HW codec driver. The thread safety of
// libva depends on the backend. If the backend is not thread-safe, we need to
// maintain a global lock that guards all libva calls. This class is fully
// synchronous and its constructor, all of its methods, and its destructor must
// be called on the same sequence. These methods may wait on the |va_lock_|
// which guards libva calls across all VaapiWrapper instances and other libva
// call sites. If the backend is known to be thread safe and the
// |kGlobalVaapiLock| flag is disabled, |va_lock_| will be null and won't guard
// any libva calls.
//
// This class is responsible for managing VAAPI connection, contexts and state.
// It is also responsible for managing and freeing VABuffers (not VASurfaces),
// which are used to queue parameters and slice data to the HW codec,
// as well as underlying memory for VASurfaces themselves.
class MEDIA_GPU_EXPORT VaapiWrapper
    : public base::RefCountedThreadSafe<VaapiWrapper> {
 public:
  // Whether it's okay or not to try to disable the VA-API global lock on the
  // current process. This is intended to be set only once during process
  // start-up.
  static bool allow_disabling_global_lock_;

  enum CodecMode {
    kDecode,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // NOTE: A kDecodeProtected VaapiWrapper is created using the actual video
    // profile and an extra VAProfileProtected, each with some special added
    // VAConfigAttribs. Then when CreateProtectedSession() is called, it will
    // then create a protected session using protected profile & entrypoint
    // which gets attached to the decoding context (or attached when the
    // decoding context is created or re-created). This then enables
    // decrypt + decode support in the driver and encrypted frame data can then
    // be submitted.
    kDecodeProtected,  // Decrypt + decode to protected surface.
#endif
    kEncodeConstantBitrate,  // Encode with Constant Bitrate algorithm.
    kEncodeConstantQuantizationParameter,  // Encode with Constant Quantization
                                           // Parameter algorithm.
    kEncodeVariableBitrate,  // Encode with variable bitrate algorithm.
    kVideoProcess,
    kCodecModeMax,
  };

  // This is enum associated with VASurfaceAttribUsageHint.
  enum class SurfaceUsageHint : int32_t {
    kGeneric = VA_SURFACE_ATTRIB_USAGE_HINT_GENERIC,
    kVideoDecoder = VA_SURFACE_ATTRIB_USAGE_HINT_DECODER,
    kVideoEncoder = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER,
    kVideoProcessWrite = VA_SURFACE_ATTRIB_USAGE_HINT_VPP_WRITE,
  };

  using InternalFormats = struct {
    bool yuv420 : 1;
    bool yuv420_10 : 1;
    bool yuv422 : 1;
    bool yuv444 : 1;
  };

  // Returns the type of the underlying VA-API implementation.
  static VAImplementation GetImplementationType();

  // Return an instance of VaapiWrapper initialized for |va_profile| and
  // |mode|. |report_error_to_uma_cb| will be called independently from
  // reporting errors to clients via method return values.
  // TODO(mcasas): Add and use a VaapiWrapperStatus instead of reusing here
  // DecoderStatus.
  static base::expected<scoped_refptr<VaapiWrapper>, DecoderStatus> Create(
      CodecMode mode,
      VAProfile va_profile,
      EncryptionScheme encryption_scheme,
      const ReportErrorToUMACB& report_error_to_uma_cb);

  // Create VaapiWrapper for VideoCodecProfile. It maps VideoCodecProfile
  // |profile| to VAProfile.
  // |report_error_to_uma_cb| will be called independently from reporting
  // errors to clients via method return values.
  // TODO(mcasas): Add and use a VaapiWrapperStatus instead of reusing here
  // DecoderStatus.
  static base::expected<scoped_refptr<VaapiWrapper>, DecoderStatus>
  CreateForVideoCodec(CodecMode mode,
                      VideoCodecProfile profile,
                      EncryptionScheme encryption_scheme,
                      const ReportErrorToUMACB& report_error_to_uma_cb);

  VaapiWrapper(const VaapiWrapper&) = delete;
  VaapiWrapper& operator=(const VaapiWrapper&) = delete;

  // Returns the supported SVC scalability modes for specified profile.
  static std::vector<SVCScalabilityMode> GetSupportedScalabilityModes(
      VideoCodecProfile media_profile,
      VAProfile va_profile);

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
  // Gets the minimum and maximum surface sizes allowed for |va_profile| in
  // |codec_mode|. Returns true if both sizes can be obtained, false otherwise.
  // Each dimension in |min_size| will be at least 1 (as long as this method
  // returns true). Additionally, because of the initialization in
  // VASupportedProfiles::FillProfileInfo_Locked(), the |max_size| is guaranteed
  // to not be empty (as long as this method returns true).
  static bool GetSupportedResolutions(VAProfile va_profile,
                                      CodecMode codec_mode,
                                      gfx::Size& min_size,
                                      gfx::Size& max_size);

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

  // Returns the pixel formats supported by the VPP.
  static std::vector<Fourcc> GetVppSupportedFormats();

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

  // Returns the list of supported profiles and entrypoints for a given |mode|.
  static std::map<VAProfile, std::vector<VAEntrypoint>>
  GetSupportedConfigurationsForCodecModeForTesting(CodecMode mode);

  static VAEntrypoint GetDefaultVaEntryPoint(CodecMode mode, VAProfile profile);

  static uint32_t BufferFormatToVARTFormat(gfx::BufferFormat fmt);

  // Creates |num_surfaces| VASurfaceIDs of |va_format|, |size| and
  // |surface_usage_hints| and, if successful, creates a |va_context_id_| of the
  // same size. |surface_usage_hints| may affect an alignment and tiling of the
  // created surface. Returns true if successful, with the created IDs in
  // |va_surfaces|. The client is responsible for destroying |va_surfaces| via
  // DestroyContextAndSurfaces() to free the allocated surfaces.
  [[nodiscard]] virtual bool CreateContextAndSurfaces(
      unsigned int va_format,
      const gfx::Size& size,
      const std::vector<SurfaceUsageHint>& surface_usage_hints,
      size_t num_surfaces,
      std::vector<VASurfaceID>* va_surfaces);

  // Creates |num_surfaces| ScopedVASurfaces of |va_format| and |size| and, if
  // successful, creates a |va_context_id_| of the same size. Returns an empty
  // vector if creation failed. If |visible_size| is supplied, the returned
  // ScopedVASurface's size is set to it. Otherwise, it's set to |size| (refer
  // to CreateScopedVASurfaces() for details).
  virtual std::vector<std::unique_ptr<ScopedVASurface>>
  CreateContextAndScopedVASurfaces(
      unsigned int va_format,
      const gfx::Size& size,
      const std::vector<SurfaceUsageHint>& usage_hints,
      size_t num_surfaces,
      const std::optional<gfx::Size>& visible_size);

  // Attempts to create a protected session that will be attached to the
  // decoding context to enable encrypted video decoding. If it cannot be
  // attached now, it will be attached when the decoding context is created or
  // re-created. |encryption| should be the encryption scheme from the
  // DecryptConfig. |hw_config| should have been obtained from the OEMCrypto
  // implementation via the CdmFactoryDaemonProxy. |hw_identifier_out| is an
  // output parameter which will return session specific information which can
  // be passed through the ChromeOsCdmContext to retrieve encrypted key
  // information. Returns true on success and false otherwise.
  bool CreateProtectedSession(media::EncryptionScheme encryption,
                              const std::vector<uint8_t>& hw_config,
                              std::vector<uint8_t>* hw_identifier_out);
  // Returns true if and only if we have created a protected session and
  // querying libva indicates that our protected session is no longer alive,
  // otherwise this will return false.
  bool IsProtectedSessionDead();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns true if and only if |va_protected_session_id| is not VA_INVALID_ID
  // and querying libva indicates that the protected session identified by
  // |va_protected_session_id| is no longer alive.
  bool IsProtectedSessionDead(VAProtectedSessionID va_protected_session_id);

  // Returns the ID of the current protected session or VA_INVALID_ID if there's
  // none. This must be called on the same sequence as other methods that use
  // the protected session ID internally.
  //
  // TODO(b/183515581): update this documentation once we force the VaapiWrapper
  // to be used on a single sequence.
  VAProtectedSessionID GetProtectedSessionID() const;
#endif
  // If we have a protected session, destroys it immediately. This should be
  // used as part of recovering dead protected sessions.
  void DestroyProtectedSession();

  // Releases the |va_surfaces| and destroys |va_context_id_|.
  void DestroyContextAndSurfaces(std::vector<VASurfaceID> va_surfaces);

  // Creates a VAContextID of |size| (unless it's a Vpp context in which case
  // |size| is ignored and 0x0 is used instead). The client is responsible for
  // releasing said context via DestroyContext() or DestroyContextAndSurfaces(),
  // or it will be released on dtor.  If a valid |va_protected_session_id_|
  // exists, it will be attached to the newly created |va_context_id_| as well.
  [[nodiscard]] virtual bool CreateContext(const gfx::Size& size);

  // Returns true iff a VAContextID has been created and hasn't been destroyed.
  bool HasContext() const;

  // Destroys the context identified by |va_context_id_|.
  virtual void DestroyContext();

  // Requests |num_surfaces| ScopedVASurfaces of size |size|, |va_rt_format| and
  // optionally |va_fourcc|. Returns self-cleaning ScopedVASurfaces or empty
  // vector if creation failed. If |visible_size| is supplied, the returned
  // ScopedVASurfaces' size are set to it: for example, we may want to request a
  // 16x16 surface to decode a 13x12 JPEG: we may want to keep track of the
  // visible size 13x12 inside the ScopedVASurface to inform the surface's users
  // that that's the only region with meaningful content. If |visible_size| is
  // not supplied, we store |size| in the returned ScopedVASurfaces.
  virtual std::vector<std::unique_ptr<ScopedVASurface>> CreateScopedVASurfaces(
      unsigned int va_rt_format,
      const gfx::Size& size,
      const std::vector<SurfaceUsageHint>& usage_hints,
      size_t num_surfaces,
      const std::optional<gfx::Size>& visible_size,
      const std::optional<uint32_t>& va_fourcc);

  // Creates a self-releasing ScopedVASurface from |frame|. The created object
  // shares the ownership of the underlying buffer represented by |frame|.
  // |frame|->StorageType() must either be STORAGE_GPU_MEMORY_BUFFER or
  // STORAGE_DMABUFS. The ownership of the surface is transferred to the caller.
  // A caller can destroy |frame| after this method returns and the underlying
  // buffer will be kept alive by the ScopedVASurface. |protected_content|
  // should only be true if the format needs VA_RT_FORMAT_PROTECTED (currently
  // only true for AMD).
  std::unique_ptr<ScopedVASurface> CreateVASurfaceForFrameResource(
      const FrameResource& frame,
      bool protected_content);

  // Creates a self-releasing ScopedVASurface from |pixmap|. The created object
  // shares the ownership of the underlying buffer represented by |pixmap|. The
  // ownership of the surface is transferred to the caller. A caller can destroy
  // |pixmap| after this method returns and the underlying buffer will be kept
  // alive by the VASurface. |protected_content| should only be true if the
  // format needs VA_RT_FORMAT_PROTECTED (currently only true for AMD).
  virtual std::unique_ptr<ScopedVASurface> CreateVASurfaceForPixmap(
      scoped_refptr<const gfx::NativePixmap> pixmap,
      bool protected_content = false);

  // Creates a self-releasing ScopedVASurface from |buffers|. The ownership of
  // the surface is transferred to the caller.  |buffers| should be a pointer
  // array of size 1, with |buffer_size| corresponding to its size. |size|
  // should be the desired surface dimensions (which does not need to map to
  // |buffer_size| in any relevant way). |buffers| should be kept alive when
  // using the VASurface and for accessing the data after the operation is
  // complete.
  std::unique_ptr<ScopedVASurface> CreateVASurfaceForUserPtr(
      const gfx::Size& size,
      uintptr_t* buffers,
      size_t buffer_size);

  // Creates a self-releasing ScopedVASurface with specified usage hints. The
  // ownership of the surface is transferred to the caller. |size| should be
  // the desired surface dimensions.
  std::unique_ptr<ScopedVASurface> CreateVASurfaceWithUsageHints(
      unsigned int va_rt_format,
      const gfx::Size& size,
      const std::vector<SurfaceUsageHint>& usage_hints);

  // Implementations of the pixmap exporter for both types of VASurface.
  // See ExportVASurfaceAsNativePixmapDmaBufUnwrapped() for further
  // documentation.
  std::unique_ptr<NativePixmapAndSizeInfo> ExportVASurfaceAsNativePixmapDmaBuf(
      const ScopedVASurface& scoped_va_surface);

  // Synchronize the VASurface explicitly. This is useful when sharing a surface
  // between contexts.
  [[nodiscard]] bool SyncSurface(VASurfaceID va_surface_id);

  // Calls SubmitBuffer_Locked() to request libva to allocate a new VABufferID
  // of |va_buffer_type| and |size|, and to map-and-copy the |data| into it. The
  // allocated VABufferIDs stay alive until DestroyPendingBuffers_Locked(). Note
  // that this method does not submit the buffers for execution, they are simply
  // stored until ExecuteAndDestroyPendingBuffers()/Execute_Locked(). The
  // ownership of |data| stays with the caller. On failure, all pending buffers
  // are destroyed.
  [[nodiscard]] bool SubmitBuffer(VABufferType va_buffer_type,
                                  size_t size,
                                  const void* data);
  // Convenient templatized version of SubmitBuffer() where |size| is deduced to
  // be the size of the type of |*data|.
  template <typename T>
  [[nodiscard]] bool SubmitBuffer(VABufferType va_buffer_type, const T* data) {
    VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return SubmitBuffer(va_buffer_type, sizeof(T), data);
  }
  // Batch-version of SubmitBuffer(), where the lock for accessing libva is
  // acquired only once.
  struct VABufferDescriptor {
    VABufferType type;
    size_t size;
    raw_ptr<const void, DanglingUntriaged> data;
  };
  [[nodiscard]] bool SubmitBuffers(
      const std::vector<VABufferDescriptor>& va_buffers);

  // Destroys all |pending_va_buffers_| sent via SubmitBuffer*(). Useful when a
  // pending job is to be cancelled (on reset or error).
  void DestroyPendingBuffers();

  // Executes job in hardware on target |va_surface_id| and destroys pending
  // buffers. Returns false if Execute() fails.
  [[nodiscard]] virtual bool ExecuteAndDestroyPendingBuffers(
      VASurfaceID va_surface_id);

  // Maps each |va_buffers| ID and copies the data described by the associated
  // VABufferDescriptor into it; then calls Execute_Locked() on |va_surface_id|.
  [[nodiscard]] bool MapAndCopyAndExecute(
      VASurfaceID va_surface_id,
      const std::vector<std::pair<VABufferID, VABufferDescriptor>>& va_buffers);

  // Creates a ScopedVAImage from a VASurface |va_surface_id| and map it into
  // memory with the given |format| and |size|. If |format| is not equal to the
  // internal format, the underlying implementation will do format conversion if
  // supported. |size| should be smaller than or equal to the surface. If |size|
  // is smaller, the image will be cropped.
  std::unique_ptr<ScopedVAImage> CreateVaImage(VASurfaceID va_surface_id,
                                               const VAImageFormat& format,
                                               const gfx::Size& size);

  // Uploads contents of |frame| into |va_surface_id| for encode.
  [[nodiscard]] virtual bool UploadVideoFrameToSurface(
      const VideoFrame& frame,
      VASurfaceID va_surface_id,
      const gfx::Size& va_surface_size);

  // Creates a buffer of |size| bytes to be used as encode output.
  virtual std::unique_ptr<ScopedVABuffer> CreateVABuffer(VABufferType type,
                                                         size_t size);

  // Gets the encoded frame linear size of the buffer with given |buffer_id|.
  // |sync_surface_id| will be used as a sync point, i.e. it will have to become
  // idle before starting the acquirement. |sync_surface_id| should be the
  // source surface passed to the encode job. Returns 0 if it fails for any
  // reason.
  [[nodiscard]] virtual uint64_t GetEncodedChunkSize(
      VABufferID buffer_id,
      VASurfaceID sync_surface_id);

  // Downloads the contents of the buffer with given |buffer_id| into a buffer
  // of size |target_size|, pointed to by |target_ptr|. The number of bytes
  // downloaded will be returned in |coded_data_size|. |sync_surface_id| will
  // be used as a sync point, i.e. it will have to become idle before starting
  // the download. |sync_surface_id| should be the source surface passed
  // to the encode job. |sync_surface_id| will be nullopt when it has already
  // been synced in GetEncodedChunkSize(). In the case vaSyncSurface()
  // is not executed. Returns false if it fails for any reason. For example, the
  // linear size of the resulted encoded frame is larger than |target_size|.
  [[nodiscard]] virtual bool DownloadFromVABuffer(
      VABufferID buffer_id,
      std::optional<VASurfaceID> sync_surface_id,
      uint8_t* target_ptr,
      size_t target_size,
      size_t* coded_data_size);

  // Get the max number of reference frames for encoding supported by the
  // driver.
  // For H.264 encoding, the value represents the maximum number of reference
  // frames for both the reference picture list 0 (bottom 16 bits) and the
  // reference picture list 1 (top 16 bits).
  [[nodiscard]] virtual bool GetVAEncMaxNumOfRefFrames(
      VideoCodecProfile profile,
      size_t* max_ref_frames);

  // Gets packed headers are supported for encoding. This is called for
  // H264 encoding. |packed_sps|, |packed_pps| and |packed_slice| stands for
  // whether packed slice parameter set, packed picture parameter set and packed
  // slice header is supported, respectively.
  [[nodiscard]] virtual bool GetSupportedPackedHeaders(
      VideoCodecProfile profile,
      bool& packed_sps,
      bool& packed_pps,
      bool& packed_slice);

  // Gets the minimum segment block size supported for AV1 encoding.
  [[nodiscard]] bool GetMinAV1SegmentSize(VideoCodecProfile profile,
                                          uint32_t& min_seg_size);

  // Blits |va_surface_src_id| into |va_surface_dst_id| applying pixel format
  // conversion, cropping and scaling if needed. |src_rect| and |dest_rect| are
  // optional. They can be used to specify the area used in the blit. If
  // |va_protected_session_id| is provided and is not VA_INVALID_ID, the
  // corresponding protected session is attached to the VPP context prior to
  // submitting the VPP buffers and detached after submitting those buffers.
  [[nodiscard]] virtual bool BlitSurface(
      VASurfaceID va_surface_src_id,
      const gfx::Size& va_surface_src_size,
      VASurfaceID va_surface_dst_id,
      const gfx::Size& va_surface_dst_size,
      std::optional<gfx::Rect> src_rect = std::nullopt,
      std::optional<gfx::Rect> dest_rect = std::nullopt
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      VAProtectedSessionID va_protected_session_id = VA_INVALID_ID
#endif
  );

  // Initialize static data before sandbox is enabled.
  static void PreSandboxInitialization(
      bool allow_disabling_global_lock = false);

  // vaDestroySurfaces() a vector or a single VASurfaceID.
  virtual void DestroySurfaces(std::vector<VASurfaceID> va_surfaces);
  virtual void DestroySurface(VASurfaceID va_surface_id);

 protected:
  VaapiWrapper(VADisplayStateHandle va_display_state_handle, CodecMode mode);
  virtual ~VaapiWrapper();

 private:
  friend class base::RefCountedThreadSafe<VaapiWrapper>;
  friend class VaapiWrapperTest;
  friend class VaapiVideoDecoderTest;
  friend class VaapiVideoEncodeAcceleratorTest;

  FRIEND_TEST_ALL_PREFIXES(VaapiTest, LowQualityEncodingSetting);
  FRIEND_TEST_ALL_PREFIXES(VaapiTest, TooManyDecoderInstances);
  FRIEND_TEST_ALL_PREFIXES(VaapiUtilsTest, ScopedVAImage);
  FRIEND_TEST_ALL_PREFIXES(VaapiUtilsTest, BadScopedVAImage);
  FRIEND_TEST_ALL_PREFIXES(VaapiUtilsTest, BadScopedVABufferMapping);
  FRIEND_TEST_ALL_PREFIXES(VaapiMinigbmTest, AllocateAndCompareWithMinigbm);

  // There's a limit to the number of simultaneous decoder instances a given SoC
  // can have, e.g. sometimes the process where VaapiWrapper runs can exhaust
  // the FDs and subsequently crash (b/181264362). We run into stability
  // problems for various reasons when that limit is reached; this class method
  // provides that number to prevent that erroneous behaviour during Create().
  static int GetMaxNumDecoderInstances();

  [[nodiscard]] bool Initialize(VAProfile va_profile,
                                EncryptionScheme encryption_scheme);
  void Deinitialize();
  void VaInitialize(const ReportErrorToUMACB& report_error_to_uma_cb);

  // Tries to allocate |num_surfaces| VASurfaceIDs of |size| and |va_format|.
  // Fills |va_surfaces| and returns true if successful, or returns false.
  [[nodiscard]] bool CreateSurfaces(
      unsigned int va_format,
      const gfx::Size& size,
      const std::vector<SurfaceUsageHint>& usage_hints,
      size_t num_surfaces,
      std::vector<VASurfaceID>* va_surfaces);

  // Syncs and exports |va_surface_id| as a gfx::NativePixmapDmaBuf. Currently,
  // the only VAAPI surface pixel formats supported are VA_FOURCC_IMC3,
  // VA_FOURCC_NV12, VA_FOURCC_P010 and VA_FOURCC_ARGB.
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
  // - For VA_FOURCC_P010, the format of the returned NativePixmapDmaBuf is
  //   gfx::BufferFormat::P010.
  //
  // - For VA_FOURCC_ARGB, the format of the returned NativePixmapDmaBuf is
  //   gfx::BufferFormat::BGRA_8888.
  //
  // Returns nullptr on failure, or if the exported surface can't contain
  // |va_surface_size|.
  std::unique_ptr<NativePixmapAndSizeInfo>
  ExportVASurfaceAsNativePixmapDmaBufUnwrapped(
      VASurfaceID va_surface_id,
      const gfx::Size& va_surface_size);

  // Carries out the vaBeginPicture()-vaRenderPicture()-vaEndPicture() on target
  // |va_surface_id|. Returns false if any of these calls fails.
  [[nodiscard]] bool Execute_Locked(VASurfaceID va_surface_id,
                                    const std::vector<VABufferID>& va_buffers)
      EXCLUSIVE_LOCKS_REQUIRED(va_lock_);

  virtual void DestroyPendingBuffers_Locked()
      EXCLUSIVE_LOCKS_REQUIRED(va_lock_);

  // Requests libva to allocate a new VABufferID of type |va_buffer.type|, then
  // maps-and-copies |va_buffer.size| contents of |va_buffer.data| to it. If a
  // failure occurs, calls DestroyPendingBuffers_Locked() and returns false.
  [[nodiscard]] virtual bool SubmitBuffer_Locked(
      const VABufferDescriptor& va_buffer) EXCLUSIVE_LOCKS_REQUIRED(va_lock_);

  // Maps |va_buffer_id| and, if successful, copies the contents of |va_buffer|
  // into it.
  [[nodiscard]] bool MapAndCopy_Locked(VABufferID va_buffer_id,
                                       const VABufferDescriptor& va_buffer)
      EXCLUSIVE_LOCKS_REQUIRED(va_lock_);

  // Queries whether |va_profile_| and |va_entrypoint_| support encoding quality
  // setting and, if available, configures it to its maximum value, for lower
  // consumption and maximum speed.
  void MaybeSetLowQualityEncoding_Locked() EXCLUSIVE_LOCKS_REQUIRED(va_lock_);

  // If a protected session is active, attaches it to the decoding context.
  [[nodiscard]] bool MaybeAttachProtectedSession_Locked()
      EXCLUSIVE_LOCKS_REQUIRED(va_lock_);

  // Tracks the number of decoder instances globally in the process.
  static base::AtomicRefCount num_decoder_instances_;

  const CodecMode mode_;
  base::SequenceCheckerImpl sequence_checker_;

  // This is declared before |va_display_| and |va_lock_| to guarantee their
  // validity for as long as the VaapiWrapper is alive.
  VADisplayStateHandle va_display_state_handle_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // If using a global VA lock, this is a pointer to VADisplayStateSingleton's
  // member |va_lock_|. Guaranteed to be valid for the lifetime of the
  // VaapiWrapper due to the |va_display_state_handle_| above.
  raw_ptr<base::Lock> va_lock_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Guaranteed to be valid for the lifetime of the VaapiWrapper due to the
  // |va_display_state_handle_| above.
  VADisplay va_display_ GUARDED_BY(va_lock_)
      GUARDED_BY_CONTEXT(sequence_checker_);

  // VA handles.
  // All valid after successful Initialize() and until Deinitialize().
  VAConfigID va_config_id_ GUARDED_BY_CONTEXT(sequence_checker_){VA_INVALID_ID};
  // Created in CreateContext() or CreateContextAndSurfaces() and valid until
  // DestroyContext() or DestroyContextAndSurfaces().
  VAContextID va_context_id_ GUARDED_BY_CONTEXT(sequence_checker_){
      VA_INVALID_ID};

  // Profile and entrypoint configured for the corresponding |va_context_id_|.
  VAProfile va_profile_ GUARDED_BY_CONTEXT(sequence_checker_);
  VAEntrypoint va_entrypoint_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Data queued up for HW codec, to be committed on next execution.
  // TODO(b/166646505): let callers manage the lifetime of these buffers.
  std::vector<VABufferID> pending_va_buffers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // VA buffer to be used for kVideoProcess. Allocated the first time around,
  // and reused afterwards.
  std::unique_ptr<ScopedVABuffer> va_buffer_for_vpp_
      GUARDED_BY_CONTEXT(sequence_checker_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For protected decode mode.
  VAConfigID va_protected_config_id_ GUARDED_BY_CONTEXT(sequence_checker_){
      VA_INVALID_ID};
  VAProtectedSessionID va_protected_session_id_
      GUARDED_BY_CONTEXT(sequence_checker_){VA_INVALID_ID};
#endif

  // Called to report codec errors to UMA. Errors to clients are reported via
  // return values from public methods.
  ReportErrorToUMACB report_error_to_uma_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_WRAPPER_H_
