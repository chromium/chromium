// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines the V4L2Device interface which is used by the
// V4L2DecodeAccelerator class to delegate/pass the device specific
// handling of any of the functionalities.

#ifndef MEDIA_GPU_V4L2_V4L2_DEVICE_H_
#define MEDIA_GPU_V4L2_V4L2_DEVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

// build_config.h must come before BUILDFLAG()
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <linux/media/av1-ctrls.h>
#endif
#include <linux/videodev2.h>

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/small_map.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device_poller.h"
#include "media/gpu/v4l2/v4l2_queue.h"
#include "media/gpu/v4l2/v4l2_utils.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

// This has not been accepted upstream.
#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1 v4l2_fourcc('A', 'V', '0', '1') /* AV1 */
#endif
// This has been upstreamed and backported for ChromeOS, but has not been
// picked up by the Chromium sysroots.
#ifndef V4L2_PIX_FMT_AV1_FRAME
#define V4L2_PIX_FMT_AV1_FRAME                        \
  v4l2_fourcc('A', 'V', '1', 'F') /* AV1 parsed frame \
                                   */
#endif

#ifndef V4L2_PIX_FMT_MT2T
#define V4L2_PIX_FMT_MT2T v4l2_fourcc('M', 'T', '2', 'T')
#endif
#ifndef V4L2_PIX_FMT_QC10C
#define V4L2_PIX_FMT_QC10C \
  v4l2_fourcc('Q', '1', '0', 'C') /* Qualcomm 10-bit compressed */
#endif

#define V4L2_PIX_FMT_INVALID v4l2_fourcc('0', '0', '0', '0')

namespace media {

class V4L2Queue;
class V4L2RequestRef;
class V4L2RequestsQueue;

struct V4L2ExtCtrl;

class MEDIA_GPU_EXPORT V4L2Device
    : public base::RefCountedThreadSafe<V4L2Device> {
 public:
  // Utility format conversion functions
  // Calculates the largest plane's allocation size requested by a V4L2 device.
  static gfx::Size AllocatedSizeFromV4L2Format(
      const struct v4l2_format& format);

  // Convert required H264 profile and level to V4L2 enums.
  static int32_t VideoCodecProfileToV4L2H264Profile(VideoCodecProfile profile);
  static int32_t H264LevelIdcToV4L2H264Level(uint8_t level_idc);

  enum class Type {
    kDecoder,
    kEncoder,
    kImageProcessor,
    kJpegDecoder,
    kJpegEncoder,
  };

  V4L2Device();

  // Open a V4L2 device of |type| for use with |v4l2_pixfmt|.
  // Return true on success.
  // The device will be closed in the destructor.
  [[nodiscard]] bool Open(Type type, uint32_t v4l2_pixfmt);

  // Returns whether Open() has been succeeded.
  bool IsValid();

  // Returns the driver name.
  std::string GetDriverName();

  // Returns the V4L2Queue corresponding to the requested |type|, or nullptr
  // if the requested queue type is not supported.
  scoped_refptr<V4L2Queue> GetQueue(enum v4l2_buf_type type);

  // Parameters and return value are the same as for the standard ioctl() system
  // call.
  [[nodiscard]] int Ioctl(int request, void* arg);

  // This method sleeps until either:
  // - SetDevicePollInterrupt() is called (on another thread),
  // - |poll_device| is true, and there is new data to be read from the device,
  //   or an event from the device has arrived; in the latter case
  //   |*event_pending| will be set to true.
  // Returns false on error, true otherwise.
  // This method should be called from a separate thread.
  bool Poll(bool poll_device, bool* event_pending);

  // These methods are used to interrupt the thread sleeping on Poll() and force
  // it to return regardless of device state, which is usually when the client
  // is no longer interested in what happens with the device (on cleanup,
  // client state change, etc.). When SetDevicePollInterrupt() is called, Poll()
  // will return immediately, and any subsequent calls to it will also do so
  // until ClearDevicePollInterrupt() is called.
  bool SetDevicePollInterrupt();
  bool ClearDevicePollInterrupt();

  // Wrappers for standard mmap/munmap system calls.
  void* Mmap(void* addr,
             unsigned int len,
             int prot,
             int flags,
             unsigned int offset);
  void Munmap(void* addr, unsigned int len);

  // Return true if the given V4L2 pixfmt can be used in CreateEGLImage()
  // for the current platform.
  bool CanCreateEGLImageFrom(const Fourcc fourcc) const;

  // Returns the preferred V4L2 input formats for |type| or empty if none.
  std::vector<uint32_t> PreferredInputFormat(Type type) const;

  // Get the supported bitrate control modes. This function should be called
  // when V4L2Device opens an encoder driver node.
  VideoEncodeAccelerator::SupportedRateControlMode
  GetSupportedRateControlMode();

  // NOTE: The below methods to query capabilities have a side effect of
  // closing the previously-open device, if any, and should not be called after
  // Open().
  // TODO(b/150431552): fix this.

  // Return V4L2 pixelformats supported by the available image processor
  // devices for |buf_type|.
  std::vector<uint32_t> GetSupportedImageProcessorPixelformats(
      v4l2_buf_type buf_type);

  // Return supported profiles for decoder, including only profiles for given
  // fourcc |pixelformats|.
  VideoDecodeAccelerator::SupportedProfiles GetSupportedDecodeProfiles(
      const std::vector<uint32_t>& pixelformats);

  // Return supported profiles for encoder.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedEncodeProfiles();

  // Return true if image processing is supported, false otherwise.
  bool IsImageProcessingSupported();

  // Return true if JPEG codec is supported, false otherwise.
  bool IsJpegDecodingSupported();
  bool IsJpegEncodingSupported();

  // Start polling on this V4L2Device. |event_callback| will be posted to
  // the caller's sequence if a buffer is ready to be dequeued and/or a V4L2
  // event has been posted. |error_callback| will be posted to the client's
  // sequence if a polling error has occurred.
  [[nodiscard]] bool StartPolling(
      V4L2DevicePoller::EventCallback event_callback,
      base::RepeatingClosure error_callback);
  // Stop polling this V4L2Device if polling was active. No new events will
  // be posted after this method has returned.
  [[nodiscard]] bool StopPolling();
  // Schedule a polling event if polling is enabled. This method is intended
  // to be called from V4L2Queue, clients should not need to call it directly.
  void SchedulePoll();

  // Attempt to dequeue a V4L2 event and return it.
  std::optional<struct v4l2_event> DequeueEvent();

  // Returns requests queue to get free requests. A null pointer is returned if
  // the queue creation failed or if requests are not supported.
  V4L2RequestsQueue* GetRequestsQueue();

  // Check whether the V4L2 control with specified |ctrl_id| is supported.
  bool IsCtrlExposed(uint32_t ctrl_id);
  // Set the specified list of |ctrls| for the specified |ctrl_class|, returns
  // whether the operation succeeded. If |request_ref| is not nullptr, the
  // controls are applied to the request instead of globally for the device.
  bool SetExtCtrls(uint32_t ctrl_class,
                   std::vector<V4L2ExtCtrl> ctrls,
                   V4L2RequestRef* request_ref = nullptr);

  // Get the value of a single control, or std::nullopt of the control is not
  // exposed by the device.
  std::optional<struct v4l2_ext_control> GetCtrl(uint32_t ctrl_id);

  // Set periodic keyframe placement (group of pictures length)
  bool SetGOPLength(uint32_t gop_length);

  void set_secure_allocate_cb(
      AllocateSecureBufferAsCallback secure_allocate_cb) {
    secure_allocate_cb_ = secure_allocate_cb;
  }
  AllocateSecureBufferAsCallback get_secure_allocate_cb() {
    return secure_allocate_cb_;
  }

 private:
  friend class base::RefCountedThreadSafe<V4L2Device>;
  // Vector of video device node paths and corresponding pixelformats supported
  // by each device node.
  using Devices = std::vector<std::pair<std::string, std::vector<uint32_t>>>;

  ~V4L2Device();

  VideoDecodeAccelerator::SupportedProfiles EnumerateSupportedDecodeProfiles(
      const std::vector<uint32_t>& pixelformats);

  VideoEncodeAccelerator::SupportedProfiles EnumerateSupportedEncodeProfiles();

  // Open device node for |path|.
  bool OpenDevicePath(const std::string& path);

  // Close the currently open device.
  void CloseDevice();

  // Enumerate all V4L2 devices on the system for |type| and store the results
  // under devices_by_type_[type].
  void EnumerateDevicesForType(V4L2Device::Type type);

  // Return device information for all devices of |type| available in the
  // system. Enumerates and queries devices on first run and caches the results
  // for subsequent calls.
  const Devices& GetDevicesForType(V4L2Device::Type type);

  // Return device node path for device of |type| supporting |pixfmt|, or
  // an empty string if the given combination is not supported by the system.
  std::string GetDevicePathFor(V4L2Device::Type type, uint32_t pixfmt);

  // Callback that is called upon a queue's destruction, to cleanup its pointer
  // in queues_.
  void OnQueueDestroyed(v4l2_buf_type buf_type);

  // Used if EnablePolling() is called to signal the user that an event
  // happened or a buffer is ready to be dequeued.
  std::unique_ptr<V4L2DevicePoller> device_poller_;

  // Indicates whether the request queue creation has been tried once.
  bool requests_queue_creation_called_ = false;

  // The request queue stores all requests allocated to be used.
  std::unique_ptr<V4L2RequestsQueue> requests_queue_;

  // Stores information for all devices available on the system
  // for each device Type.
  std::map<V4L2Device::Type, Devices> devices_by_type_;

  // The actual device fd.
  base::ScopedFD device_fd_;

  // eventfd fd to signal device poll thread when its poll() should be
  // interrupted.
  base::ScopedFD device_poll_interrupt_fd_;

  // Associates a v4l2_buf_type to its queue.
  base::flat_map<enum v4l2_buf_type, V4L2Queue*> queues_;

  // Callback to use for allocating secure buffers.
  AllocateSecureBufferAsCallback secure_allocate_cb_;

  SEQUENCE_CHECKER(client_sequence_checker_);
};

}  //  namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_DEVICE_H_
