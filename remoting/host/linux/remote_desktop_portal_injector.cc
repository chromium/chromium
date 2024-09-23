// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/remote_desktop_portal_injector.h"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib-object.h>
#include <linux/input.h>
#include <poll.h>

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "remoting/base/logging.h"
#include "third_party/libei/cipd/include/libei.h"
#include "third_party/webrtc/modules/portal/portal_request_response.h"
#include "third_party/webrtc/modules/portal/scoped_glib.h"
#include "third_party/webrtc/modules/portal/xdg_desktop_portal_utils.h"

namespace remoting::xdg_portal {

using webrtc::Scoped;

namespace {
// TODO(crbug.com/40212673): See if these can be pulled from a common place.
constexpr int BUTTON_LEFT_KEYCODE = 272;
constexpr int BUTTON_RIGHT_KEYCODE = 273;
constexpr int BUTTON_MIDDLE_KEYCODE = 274;
constexpr int BUTTON_FORWARD_KEYCODE = 277;
constexpr int BUTTON_BACK_KEYCODE = 278;

// See:
// https://libinput.pages.freedesktop.org/libei/api/group__libei.html#gaf2ec4b04f6b3c706bad0f1cae66bea34
constexpr int EI_SCROLL_FACTOR = 120;

int EvdevCodeToMouseButton(int code) {
  switch (code) {
    case BUTTON_LEFT_KEYCODE:
      return BTN_LEFT;
    case BUTTON_RIGHT_KEYCODE:
      return BTN_RIGHT;
    case BUTTON_MIDDLE_KEYCODE:
      return BTN_MIDDLE;
    case BUTTON_BACK_KEYCODE:
      return BTN_BACK;
    case BUTTON_FORWARD_KEYCODE:
      return BTN_FORWARD;
    default:
      NOTREACHED() << "Undefined code: " << code;
  }
}

}  // namespace

RemoteDesktopPortalInjector::RemoteDesktopPortalInjector() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RemoteDesktopPortalInjector::~RemoteDesktopPortalInjector() {
  DCHECK(!ei_);
}

void RemoteDesktopPortalInjector::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ei_pointer_enabled_ = false;
  ei_keyboard_enabled_ = false;
  ei_absolute_pointer_enabled_ = false;
  if (ei_seat_) {
    ei_seat_unbind_capabilities(ei_seat_, EI_DEVICE_CAP_KEYBOARD,
                                EI_DEVICE_CAP_POINTER,
                                EI_DEVICE_CAP_POINTER_ABSOLUTE, nullptr);
  }
  if (ei_keyboard_) {
    ei_device_unref(ei_keyboard_);
    ei_keyboard_ = nullptr;
  }
  if (ei_pointer_) {
    ei_device_unref(ei_pointer_);
    ei_pointer_ = nullptr;
  }
  if (ei_absolute_pointer_) {
    ei_device_unref(ei_absolute_pointer_);
    ei_absolute_pointer_ = nullptr;
  }
  if (ei_seat_) {
    ei_event_watcher_->StopProcessingEvents();
    ei_seat_unref(ei_seat_);
    ei_seat_ = nullptr;
  }
  if (ei_) {
    ei_unref(ei_);
    ei_ = nullptr;
  }
}

// static
void RemoteDesktopPortalInjector::ValidateGDPBusProxyResult(
    GObject* proxy,
    GAsyncResult* result,
    gpointer user_data) {
  RemoteDesktopPortalInjector* that =
      static_cast<RemoteDesktopPortalInjector*>(user_data);
  DCHECK(that);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  Scoped<GError> error;
  Scoped<GVariant> variant(g_dbus_proxy_call_finish(
      reinterpret_cast<GDBusProxy*>(proxy), result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      return;
    }
    LOG(ERROR) << "Error in input injection (without EI): " << error->message;
  }
}

void RemoteDesktopPortalInjector::InjectMouseButton(int code, bool pressed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (use_ei_) {
    if (ei_pointer_enabled_) {
      ei_device_pointer_button(ei_pointer_, EvdevCodeToMouseButton(code),
                               pressed);
      ei_device_frame(ei_pointer_, ei_now(ei_));
    } else {
      // Non-ei injection is blocked by portal when EI has been enabled
      // successfully previously.
      LOG(ERROR)
          << "Unable to inject mouse button since EI pointer is disabled";
    }
    return;
  }

  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call(proxy_, "NotifyPointerButton",
                    g_variant_new("(oa{sv}iu)", session_handle_.c_str(),
                                  &builder, code, pressed),
                    G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                    ValidateGDPBusProxyResult, this);
}

void RemoteDesktopPortalInjector::InjectMouseScroll(int axis, int steps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (use_ei_) {
    if (ei_absolute_pointer_enabled_) {
      if (axis == ScrollType::VERTICAL_SCROLL) {
        ei_device_pointer_scroll_discrete(ei_absolute_pointer_, 0,
                                          steps * EI_SCROLL_FACTOR);
      } else {
        ei_device_pointer_scroll_discrete(ei_absolute_pointer_,
                                          steps * EI_SCROLL_FACTOR, 0);
      }
      ei_device_pointer_scroll_stop(ei_absolute_pointer_, true, true);
      ei_device_frame(ei_absolute_pointer_, ei_now(ei_));
    } else {
      LOG(ERROR) << "Unable to scroll mouse since EI pointer is disabled";
    }
    return;
  }

  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call(proxy_, "NotifyPointerAxisDiscrete",
                    g_variant_new("(oa{sv}ui)", session_handle_.c_str(),
                                  &builder, axis, steps),
                    G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                    ValidateGDPBusProxyResult, this);
}

void RemoteDesktopPortalInjector::MovePointerBy(int delta_x, int delta_y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (use_ei_) {
    if (ei_pointer_enabled_) {
      ei_device_pointer_motion(ei_pointer_, delta_x, delta_y);
      ei_device_frame(ei_pointer_, ei_now(ei_));
    } else {
      LOG(ERROR)
          << "Unable to do relative mouse move since EI pointer is disabled";
    }
    return;
  }

  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call(
      proxy_, "NotifyPointerMotion",
      g_variant_new("(oa{sv}dd)", session_handle_.c_str(), &builder,
                    static_cast<double>(delta_x), static_cast<double>(delta_y)),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      ValidateGDPBusProxyResult, this);
}

bool RemoteDesktopPortalInjector::InDeviceRegion(uint32_t x, uint32_t y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& region : device_regions_) {
    if ((x >= region.x && x < region.x + region.w) &&
        (y >= region.y && y < region.y + region.h)) {
      return true;
    }
  }
  return false;
}

void RemoteDesktopPortalInjector::MovePointerTo(int x, int y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (use_ei_) {
    if (ei_absolute_pointer_enabled_) {
      if (!InDeviceRegion(x, y)) {
        LOG(WARNING) << "Absolute coordinates " << x << "," << y
                     << "are not in any device region, ignoring";
        return;
      }
      ei_device_pointer_motion_absolute(ei_absolute_pointer_, x, y);
      ei_device_frame(ei_absolute_pointer_, ei_now(ei_));
    } else {
      LOG(ERROR)
          << "Unable to do absolute mouse move since EI pointer is disabled";
    }
    return;
  }

  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  VLOG(6) << "session handle: " << session_handle_
          << ", stream node id: " << pipewire_stream_node_id_;
  g_dbus_proxy_call(
      proxy_, "NotifyPointerMotionAbsolute",
      g_variant_new("(oa{sv}udd)", session_handle_.c_str(), &builder,
                    pipewire_stream_node_id_, static_cast<double>(x),
                    static_cast<double>(y)),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
      ValidateGDPBusProxyResult, this);
}

void RemoteDesktopPortalInjector::InjectKeyPress(int code,
                                                 bool pressed,
                                                 bool is_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (use_ei_) {
    if (ei_keyboard_enabled_) {
      ei_device_keyboard_key(ei_keyboard_, code, pressed);
      ei_device_frame(ei_keyboard_, ei_now(ei_));
    } else {
      LOG(ERROR) << "Unable to inject key press since EI keyboard is disabled";
    }
    return;
  }

  DCHECK(proxy_);
  DCHECK(cancellable_);
  DCHECK(!session_handle_.empty());
  DCHECK(pipewire_stream_node_id_);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  VLOG(6) << "session handle: " << session_handle_;
  g_dbus_proxy_call(proxy_,
                    is_code ? "NotifyKeyboardKeycode" : "NotifyKeyboardKeysym",
                    g_variant_new("(oa{sv}iu)", session_handle_.c_str(),
                                  &builder, code, pressed),
                    G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, cancellable_,
                    ValidateGDPBusProxyResult, this);
}

void RemoteDesktopPortalInjector::SetSessionDetails(
    webrtc::xdg_portal::SessionDetails session_details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HOST_LOG << "Desktop portal session details received";
  proxy_ = session_details.proxy;
  cancellable_ = session_details.cancellable;
  session_handle_ = session_details.session_handle;
  pipewire_stream_node_id_ = session_details.pipewire_stream_node_id;
}

static void ei_loghandler(struct ei* ei,
                          enum ei_log_priority priority,
                          const char* message,
                          struct ei_log_context* ctx) {
  HOST_LOG << "[EI_LOGGER] " << ei_log_context_get_func(ctx) << " : "
           << message;
}

// static
void RemoteDesktopPortalInjector::OnEiFdRequested(GObject* object,
                                                  GAsyncResult* result,
                                                  gpointer user_data) {
  auto* proxy = reinterpret_cast<GDBusProxy*>(object);
  RemoteDesktopPortalInjector* that =
      static_cast<RemoteDesktopPortalInjector*>(user_data);
  DCHECK_CALLED_ON_VALID_SEQUENCE(that->sequence_checker_);

  Scoped<GError> error;
  Scoped<GUnixFDList> outlist;
  Scoped<GVariant> variant(g_dbus_proxy_call_with_unix_fd_list_finish(
      proxy, outlist.receive(), result, error.receive()));
  if (!variant) {
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      return;
    }
    LOG(ERROR) << "Failed to get the EI fd: " << error->message;
    std::move(that->on_libei_setup_done_).Run(false);
    return;
  }

  int32_t index;
  g_variant_get(variant.get(), "(h)", &index);

  that->ei_fd_ = g_unix_fd_list_get(outlist.get(), index, error.receive());

  if (that->ei_fd_ == -1) {
    LOG(ERROR) << "Failed to get file descriptor from the list: "
               << error->message;
    std::move(that->on_libei_setup_done_).Run(false);
    return;
  }

  that->ei_ = ei_new_sender(nullptr);
  ei_configure_name(that->ei_, "crd-input");
  ei_log_set_priority(that->ei_, EI_LOG_PRIORITY_INFO);
  ei_log_set_handler(that->ei_, ei_loghandler);
  ei_setup_backend_fd(that->ei_.get(), that->ei_fd_);

  that->ei_event_watcher_ =
      std::make_unique<EiEventWatcherGlib>(that->ei_fd_, that->ei_, that);

  that->ei_event_watcher_->StartProcessingEvents();
  that->use_ei_ = true;
  HOST_LOG << "Using LIBEI for input injection";
  std::move(that->on_libei_setup_done_).Run(true);
}

void RemoteDesktopPortalInjector::HandleRegions(struct ei_device* device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t idx = 0;
  struct ei_region* region;

  while ((region = ei_device_get_region(device, idx++))) {
    DeviceRegion device_region{.x = ei_region_get_x(region),
                               .y = ei_region_get_y(region),
                               .w = ei_region_get_width(region),
                               .h = ei_region_get_height(region)};

    HOST_LOG << "EI Device: " << ei_device_get_name(device) << " has region "
             << device_region.w << "x" << device_region.h << "@"
             << device_region.x << "," << device_region.y;
    device_regions_.push_back(std::move(device_region));
  }
}

void RemoteDesktopPortalInjector::OnEiSeatAddedEvent(struct ei_event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ei_seat_) {
    return;
  }

  ei_seat_ = ei_seat_ref(ei_event_get_seat(event));
  HOST_LOG << "EI seat added: " << ei_seat_get_name(ei_seat_);

  ei_seat_bind_capability(ei_seat_, EI_DEVICE_CAP_POINTER);
  ei_seat_bind_capability(ei_seat_, EI_DEVICE_CAP_KEYBOARD);
  ei_seat_bind_capability(ei_seat_, EI_DEVICE_CAP_POINTER_ABSOLUTE);
}

void RemoteDesktopPortalInjector::OnEiSeatRemovedEvent(struct ei_event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't need to close the devices, libei will give us the right events.
  if (ei_event_get_seat(event) == ei_seat_.get()) {
    ei_seat_unref(ei_seat_);
    ei_seat_ = nullptr;
  }
}

void RemoteDesktopPortalInjector::OnEiDeviceAddedEvent(struct ei_event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  struct ei_device* device = ei_device_ref(ei_event_get_device(event));

  if (ei_device_has_capability(device, EI_DEVICE_CAP_POINTER)) {
    HOST_LOG << "EI New pointer device: " << ei_device_get_name(device);
    if (ei_pointer_) {
      ei_device_unref(ei_pointer_);
    }
    ei_pointer_ = device;
  }
  if (ei_device_has_capability(device, EI_DEVICE_CAP_KEYBOARD)) {
    HOST_LOG << "EI New keyboard device: " << ei_device_get_name(device);
    if (ei_keyboard_) {
      ei_device_unref(ei_keyboard_);
    }
    ei_keyboard_ = device;
  }
  if (ei_device_has_capability(device, EI_DEVICE_CAP_POINTER_ABSOLUTE)) {
    HOST_LOG << "EI New absolute pointer device: "
             << ei_device_get_name(device);
    if (ei_absolute_pointer_) {
      ei_device_unref(ei_absolute_pointer_);
    }
    ei_absolute_pointer_ = device;
    HandleRegions(ei_absolute_pointer_);
  }
}

void RemoteDesktopPortalInjector::OnEiDeviceResumedEvent(
    struct ei_event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  struct ei_device* event_device = ei_event_get_device(event);
  if (!event_device) {
    return;
  }

  if (event_device == ei_pointer_.get()) {
    HOST_LOG << "EI Emulating the pointer device now";
    ei_device_start_emulating(ei_pointer_, ++device_serial_);
    ei_pointer_enabled_ = true;
  } else if (event_device == ei_keyboard_.get()) {
    HOST_LOG << "EI Emulating the keyboard device now";
    ei_device_start_emulating(ei_keyboard_, ++device_serial_);
    ei_keyboard_enabled_ = true;
  } else if (event_device == ei_absolute_pointer_.get()) {
    HOST_LOG << "EI Emulating the absolute pointer device now";
    ei_device_start_emulating(ei_absolute_pointer_, ++device_serial_);
    ei_absolute_pointer_enabled_ = true;
  } else {
    LOG(WARNING) << "EI unknown device resumed: "
                 << ei_device_get_name(event_device);
  }
}

void RemoteDesktopPortalInjector::OnEiDevicePausedEvent(
    struct ei_event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  struct ei_device* event_device = ei_event_get_device(event);
  if (!event_device) {
    return;
  }

  if (event_device == ei_pointer_.get()) {
    HOST_LOG << "EI Pointer device paused";
    ei_device_stop_emulating(ei_pointer_);
    ei_pointer_enabled_ = false;
  } else if (event_device == ei_keyboard_.get()) {
    HOST_LOG << "EI Keyboard device paused";
    ei_device_stop_emulating(ei_keyboard_);
    ei_keyboard_enabled_ = false;
  } else if (event_device == ei_absolute_pointer_.get()) {
    HOST_LOG << "EI Absolute pointer device paused";
    ei_device_stop_emulating(ei_absolute_pointer_);
    ei_absolute_pointer_enabled_ = false;
  } else {
    LOG(WARNING) << "EI Unknown device paused: "
                 << ei_device_get_name(event_device);
  }
}

void RemoteDesktopPortalInjector::OnEiDeviceRemovedEvent(
    struct ei_event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  struct ei_device* event_device = ei_event_get_device(event);
  if (!event_device) {
    return;
  }

  if (event_device == ei_pointer_.get()) {
    HOST_LOG << "EI Pointer device removed";
    ei_device_unref(ei_pointer_);
    ei_pointer_ = nullptr;
    ei_pointer_enabled_ = false;
  } else if (event_device == ei_keyboard_.get()) {
    HOST_LOG << "EI Keyboard device removed";
    ei_device_unref(ei_keyboard_);
    ei_keyboard_ = nullptr;
    ei_keyboard_enabled_ = false;
  } else if (event_device == ei_absolute_pointer_.get()) {
    HOST_LOG << "EI Absolute pointer device removed";
    ei_device_unref(ei_absolute_pointer_);
    ei_absolute_pointer_ = nullptr;
    ei_absolute_pointer_enabled_ = false;
    device_regions_.clear();
  } else {
    LOG(WARNING) << "EI unknown device removed: "
                 << ei_device_get_name(event_device);
  }
}

void RemoteDesktopPortalInjector::HandleEiEvent(struct ei_event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (ei_event_get_type(event)) {
    case EI_EVENT_CONNECT:
      HOST_LOG << "EI connected";
      break;
    case EI_EVENT_DISCONNECT: {
      HOST_LOG << "EI disconnected";
      Shutdown();
      break;
    }
    case EI_EVENT_SEAT_ADDED:
      OnEiSeatAddedEvent(event);
      break;
    case EI_EVENT_SEAT_REMOVED:
      OnEiSeatRemovedEvent(event);
      break;
    case EI_EVENT_DEVICE_ADDED:
      OnEiDeviceAddedEvent(event);
      break;
    case EI_EVENT_DEVICE_RESUMED:
      OnEiDeviceResumedEvent(event);
      break;
    case EI_EVENT_DEVICE_PAUSED:
      OnEiDevicePausedEvent(event);
      break;
    case EI_EVENT_DEVICE_REMOVED:
      OnEiDeviceRemovedEvent(event);
      break;
    case EI_EVENT_FRAME:
    case EI_EVENT_DEVICE_START_EMULATING:
    case EI_EVENT_DEVICE_STOP_EMULATING:
    case EI_EVENT_POINTER_MOTION:
    case EI_EVENT_POINTER_MOTION_ABSOLUTE:
    case EI_EVENT_POINTER_BUTTON:
    case EI_EVENT_POINTER_SCROLL:
    case EI_EVENT_POINTER_SCROLL_DISCRETE:
    case EI_EVENT_KEYBOARD_KEY:
      break;
    default:
      LOG(WARNING) << "Unexpected EI event observed type: "
                   << ei_event_get_type(event);
  }
}

void RemoteDesktopPortalInjector::SetupLibei(
    base::OnceCallback<void(bool)> OnLibeiDone) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_libei_setup_done_ = std::move(OnLibeiDone);
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  g_dbus_proxy_call_with_unix_fd_list(
      proxy_, "ConnectToEIS",
      g_variant_new("(oa{sv})", session_handle_.c_str(), &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, /*fd_list=*/nullptr, cancellable_,
      OnEiFdRequested, this);
}

}  // namespace remoting::xdg_portal
