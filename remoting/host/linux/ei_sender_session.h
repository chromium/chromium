// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_EI_SENDER_SESSION_H_
#define REMOTING_HOST_LINUX_EI_SENDER_SESSION_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/base/pointer_utils.h"
#include "remoting/proto/event.pb.h"
#include "third_party/libei/cipd/include/libei-1.0/libei.h"

namespace remoting {

class EiKeymap;
class EiKeyboardLayoutMonitor;
class EiInputInjector;

// Manages a sender-client connection to an EIS implementation to allow
// injecting input events.
class EiSenderSession {
 public:
  using CreateCallback = base::OnceCallback<void(
      base::expected<std::unique_ptr<EiSenderSession>, Loggable>)>;

  ~EiSenderSession();

  base::WeakPtr<EiSenderSession> GetWeakPtr();

  void SetKeyboardLayoutMonitor(base::WeakPtr<EiKeyboardLayoutMonitor> monitor);
  void SetInputInjector(base::WeakPtr<EiInputInjector> input_injector);

  // Injects an event for the provided |usb_keycode|. |is_press| should be true
  // for key-down and repeat events, and false for release events.
  void InjectKeyEvent(std::uint32_t usb_keycode, bool is_press);

  // Injects an absolute pointer move to the specified location. |region_id|
  // identifies the logical monitor containing the move target. |fractional_x|
  // and |fractional_y| are in the range [0.0, 1.0] and represent the position
  // of the move target within the monitor, with 0, 0 representing the top left
  // corner and 1, 1 representing the bottom right.
  void InjectAbsolutePointerMove(std::string_view region_id,
                                 float fractional_x,
                                 float fractional_y);

  // Injects a relative move, specified in logical pixels.
  void InjectRelativePointerMove(std::int32_t delta_x, std::int32_t delta_y);

  // Injects an event for the provided |button|. |is_press| should be true for
  // button-down events and false for button-up.
  void InjectButton(protocol::MouseEvent::MouseButton button, bool is_press);

  // Injects a "smooth" (pixel-exact) scroll event, where |delta_x| and
  // |delta_y| represent logical pixels. The direction is the same used by the
  // MouseEvent proto. That is, positive values scroll up and to the left.
  void InjectScrollDelta(double delta_x, double delta_y);

  // Injects a "discrete" (wheel-tick) scroll event, where |ticks_x| and
  // |ticks_y| represent wheel ticks (or fractions thereof). The direction is
  // the same used by the MouseEvent proto. That is, positive values scroll up
  // and to the left.
  void InjectScrollDiscrete(float ticks_x, float ticks_y);

  // Asynchronously attempts to establish a session with an EIS implementation
  // over |fd| and invokes |callback| with the result. Takes ownership of |fd|,
  // closing it if the session cannot be established.
  static void CreateWithFd(base::ScopedFD fd, CreateCallback callback);

 private:
  using InitCallback = base::OnceCallback<void(base::expected<void, Loggable>)>;

  using EiPtr = CRefCounted<ei, ei_ref, ei_unref>;
  using EiSeatPtr = CRefCounted<ei_seat, ei_seat_ref, ei_seat_unref>;
  using EiDevicePtr = CRefCounted<ei_device, ei_device_ref, ei_device_unref>;
  using EiRegionPtr = CRefCounted<ei_region, ei_region_ref, ei_region_unref>;
  using EiTouchPtr = CRefCounted<ei_touch, ei_touch_ref, ei_touch_unref>;
  using EiKeymapPtr = std::unique_ptr<EiKeymap>;

  // Events do not allow additional refs, but one still needs to call unref to
  // release them.
  using EiEventPtr = std::unique_ptr<ei_event, DeleteFunc<ei_event_unref>>;

  // Attached to each device as user data to track additional state.
  struct DeviceState {
    // New devices are paused until a resume event for them is received.
    bool resumed = false;
    // Whether we have told the EI server to expect events from this device.
    bool emulating = false;
    // TODO(rkjnsn): Include xkb_state for tracking layout state (e.g.,
    // current group, lock state) for keyboards.
  };

  // Construct an uninitialized instance.
  EiSenderSession();

  // Attempt to initialize this instance, invoking |callback| with the result.
  void InitWithFd(base::ScopedFD fd, InitCallback callback);

  // Invoked whenever the libei-provided event fd becomes readable, signaling
  // that there is work for the library to perform.
  void OnFdReadable();

  // Invoked in response to the various libei events.
  void OnConnected();
  void OnDisconnected(bool shutting_down);
  void OnSeatAdded(EiSeatPtr seat);
  void OnSeatRemoved(EiSeatPtr seat);
  void OnDeviceAdded(EiDevicePtr device);
  void OnDeviceRemoved(EiDevicePtr device);
  void OnDevicePaused(EiDevicePtr device);
  void OnDeviceResumed(EiDevicePtr device);

  // Invoked when a keymap finishes loading.
  void OnKeymapLoaded(EiDevicePtr device);

  // Processes all events currently available from libei.
  void ProcessEvents(bool shutting_down);

  // Stores the provided |device| in |map| keyed by each region ID associated
  // with the device. (The devices may be inserted more than once if it has
  // multiple regions.)
  void AddDeviceRegions(std::multimap<std::string,
                                      std::pair<EiRegionPtr, EiDevicePtr>,
                                      std::less<>>& map,
                        EiDevicePtr device);

  // Called when a new device is added. Allocates an associated DeviceState
  // object and attaches it to the device.
  void AllocDeviceState(const EiDevicePtr& device);

  // Gets the DeviceState associated with the passed device.
  DeviceState& GetDeviceState(const EiDevicePtr& device);

  // Called when a device is removed. Frees the associated DeviceState.
  void FreeDeviceState(const EiDevicePtr& device);

  InitCallback init_callback_;

  EiPtr ei_;
  // We currently assume that the first-received seat will be the default, and
  // the compositor won't do things like add a new seat and then remove the
  // original. If this turns out to be an invalid assumption, a vector of
  // currently valid seats could be maintained like for keyboards and relative
  // pointers.
  EiSeatPtr default_seat_;
  // Devices may be added and removed dynamically by the compositor at any time.
  // Indeed, because a device with keyboard capability has a fixed keymap, the
  // compositor must create a new device and remove the old one when the keymap
  // changes. That might result in the pointer getting removed and readded as
  // well if the compositor opts to provide both capabilities on the same
  // device.
  std::vector<std::tuple<EiDevicePtr, EiKeymapPtr>> keyboards_;
  std::vector<EiDevicePtr> relative_pointers_;
  std::vector<EiDevicePtr> button_devices_;
  std::vector<EiDevicePtr> scroll_devices_;
  // Touch and absolute pointer devices may have one or more region, each
  // region corresponding to a stream being captured. The compositor may choose
  // to provide one device with multiple regions, a separate device per region,
  // or something in between. The regions are mapped to streams by means of a
  // mapping ID string.
  //
  // The following multimaps allow efficiently looking up the appropriate region
  // and device for each injected input event. (The region is needed to convert
  // fractional coordinates to logical coordinates.).
  //
  // TODO(rkjnsn): Switch to std::flat_multimap when C++23 is available. That
  // should be more efficient since the number of devices are expected to be
  // relatively few and change infrequently.
  std::multimap<std::string, std::pair<EiRegionPtr, EiDevicePtr>, std::less<>>
      absolute_pointers_;
  std::multimap<std::string, std::pair<EiRegionPtr, EiDevicePtr>, std::less<>>
      touch_devices_;

  // libei requires a new sequence number each time ei_device_start_emulating()
  // is called. This tracks the most recently used sequence number.
  std::uint32_t start_emulating_sequence_ = 0;

  bool supports_touch_ = false;
  bool supports_relative_pointer_ = false;

  std::unique_ptr<base::FileDescriptorWatcher::Controller> fd_watcher_;

  base::WeakPtr<EiKeyboardLayoutMonitor> keyboard_layout_monitor_;
  base::WeakPtr<EiInputInjector> input_injector_;

  int subtick_pixels_x_ = 0;
  int subtick_pixels_y_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<EiSenderSession> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_EI_SENDER_SESSION_H_
