// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_param_traits.h"

#include <stdint.h>
#include <sstream>

#include "base/strings/stringprintf.h"
#include "ipc/ipc_message_protobuf_utils.h"
#include "ipc/ipc_message_utils.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace IPC {

// webrtc::DesktopVector

// static
void ParamTraits<webrtc::DesktopVector>::Write(base::Pickle* m,
                                               const webrtc::DesktopVector& p) {
  m->WriteInt(p.x());
  m->WriteInt(p.y());
}

// static
bool ParamTraits<webrtc::DesktopVector>::Read(const base::Pickle* m,
                                              base::PickleIterator* iter,
                                              webrtc::DesktopVector* r) {
  int x, y;
  if (!iter->ReadInt(&x) || !iter->ReadInt(&y))
    return false;
  *r = webrtc::DesktopVector(x, y);
  return true;
}

// static
void ParamTraits<webrtc::DesktopVector>::Log(const webrtc::DesktopVector& p,
                                             std::string* l) {
  l->append(base::StringPrintf("webrtc::DesktopVector(%d, %d)",
                               p.x(), p.y()));
}

// webrtc::DesktopSize

// static
void ParamTraits<webrtc::DesktopSize>::Write(base::Pickle* m,
                                             const webrtc::DesktopSize& p) {
  m->WriteInt(p.width());
  m->WriteInt(p.height());
}

// static
bool ParamTraits<webrtc::DesktopSize>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            webrtc::DesktopSize* r) {
  int width, height;
  if (!iter->ReadInt(&width) || !iter->ReadInt(&height))
    return false;
  *r = webrtc::DesktopSize(width, height);
  return true;
}

// static
void ParamTraits<webrtc::DesktopSize>::Log(const webrtc::DesktopSize& p,
                                           std::string* l) {
  l->append(base::StringPrintf("webrtc::DesktopSize(%d, %d)",
                               p.width(), p.height()));
}

// webrtc::DesktopRect

// static
void ParamTraits<webrtc::DesktopRect>::Write(base::Pickle* m,
                                             const webrtc::DesktopRect& p) {
  m->WriteInt(p.left());
  m->WriteInt(p.top());
  m->WriteInt(p.right());
  m->WriteInt(p.bottom());
}

// static
bool ParamTraits<webrtc::DesktopRect>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            webrtc::DesktopRect* r) {
  int left, right, top, bottom;
  if (!iter->ReadInt(&left) || !iter->ReadInt(&top) ||
      !iter->ReadInt(&right) || !iter->ReadInt(&bottom)) {
    return false;
  }
  *r = webrtc::DesktopRect::MakeLTRB(left, top, right, bottom);
  return true;
}

// static
void ParamTraits<webrtc::DesktopRect>::Log(const webrtc::DesktopRect& p,
                                           std::string* l) {
  l->append(base::StringPrintf("webrtc::DesktopRect(%d, %d, %d, %d)",
                               p.left(), p.top(), p.right(), p.bottom()));
}

// webrtc::MouseCursor

// static
void ParamTraits<webrtc::MouseCursor>::Write(base::Pickle* m,
                                             const webrtc::MouseCursor& p) {
  ParamTraits<webrtc::DesktopSize>::Write(m, p.image()->size());

  // Data is serialized in such a way that size is exactly width * height *
  // |kBytesPerPixel|.
  std::string data;
  uint8_t* current_row = p.image()->data();
  for (int y = 0; y < p.image()->size().height(); ++y) {
    data.append(current_row,
                current_row + p.image()->size().width() *
                    webrtc::DesktopFrame::kBytesPerPixel);
    current_row += p.image()->stride();
  }
  m->WriteData(reinterpret_cast<const char*>(p.image()->data()), data.size());

  ParamTraits<webrtc::DesktopVector>::Write(m, p.hotspot());
}

// static
bool ParamTraits<webrtc::MouseCursor>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            webrtc::MouseCursor* r) {
  webrtc::DesktopSize size;
  if (!ParamTraits<webrtc::DesktopSize>::Read(m, iter, &size) ||
      size.width() <= 0 || size.width() > (SHRT_MAX / 2) ||
      size.height() <= 0 || size.height() > (SHRT_MAX / 2)) {
    return false;
  }

  const int expected_length =
      size.width() * size.height() * webrtc::DesktopFrame::kBytesPerPixel;

  const char* data;
  int data_length;
  if (!iter->ReadData(&data, &data_length) || data_length != expected_length)
    return false;

  webrtc::DesktopVector hotspot;
  if (!ParamTraits<webrtc::DesktopVector>::Read(m, iter, &hotspot))
    return false;

  webrtc::BasicDesktopFrame* image = new webrtc::BasicDesktopFrame(size);
  memcpy(image->data(), data, data_length);

  r->set_image(image);
  r->set_hotspot(hotspot);
  return true;
}

// static
void ParamTraits<webrtc::MouseCursor>::Log(
    const webrtc::MouseCursor& p,
    std::string* l) {
  l->append(base::StringPrintf(
      "webrtc::DesktopRect{image(%d, %d), hotspot(%d, %d)}",
      p.image()->size().width(), p.image()->size().height(),
      p.hotspot().x(), p.hotspot().y()));
}

// remoting::ScreenResolution

// static
void ParamTraits<remoting::ScreenResolution>::Write(
    base::Pickle* m,
    const remoting::ScreenResolution& p) {
  ParamTraits<webrtc::DesktopSize>::Write(m, p.dimensions());
  ParamTraits<webrtc::DesktopVector>::Write(m, p.dpi());
}

// static
bool ParamTraits<remoting::ScreenResolution>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    remoting::ScreenResolution* r) {
  webrtc::DesktopSize size;
  webrtc::DesktopVector dpi;
  if (!ParamTraits<webrtc::DesktopSize>::Read(m, iter, &size) ||
      !ParamTraits<webrtc::DesktopVector>::Read(m, iter, &dpi)) {
    return false;
  }
  if (size.width() < 0 || size.height() < 0 ||
      dpi.x() < 0 || dpi.y() < 0) {
    return false;
  }
  *r = remoting::ScreenResolution(size, dpi);
  return true;
}

// static
void ParamTraits<remoting::ScreenResolution>::Log(
    const remoting::ScreenResolution& p,
    std::string* l) {
  l->append(base::StringPrintf("webrtc::ScreenResolution(%d, %d, %d, %d)",
                               p.dimensions().width(), p.dimensions().height(),
                               p.dpi().x(), p.dpi().y()));
}

// remoting::DesktopEnvironmentOptions

// static
void ParamTraits<remoting::DesktopEnvironmentOptions>::Write(
    base::Pickle* m,
    const remoting::DesktopEnvironmentOptions& p) {
  m->WriteBool(p.enable_curtaining());
  m->WriteBool(p.enable_user_interface());
  m->WriteBool(p.desktop_capture_options()->use_update_notifications());
  m->WriteBool(p.desktop_capture_options()->disable_effects());
  m->WriteBool(p.desktop_capture_options()->detect_updated_region());
#if defined(WEBRTC_WIN)
  m->WriteBool(p.desktop_capture_options()->allow_use_magnification_api());
  m->WriteBool(p.desktop_capture_options()->allow_directx_capturer());
#endif  // defined(WEBRTC_WIN)
}

// static
bool ParamTraits<remoting::DesktopEnvironmentOptions>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    remoting::DesktopEnvironmentOptions* r) {
  *r = remoting::DesktopEnvironmentOptions::CreateDefault();
  bool enable_curtaining;
  bool enable_user_interface;
  bool use_update_notifications;
  bool disable_effects;
  bool detect_updated_region;

  if (!iter->ReadBool(&enable_curtaining) ||
      !iter->ReadBool(&enable_user_interface) ||
      !iter->ReadBool(&use_update_notifications) ||
      !iter->ReadBool(&disable_effects) ||
      !iter->ReadBool(&detect_updated_region)) {
    return false;
  }

  r->set_enable_curtaining(enable_curtaining);
  r->set_enable_user_interface(enable_user_interface);
  r->desktop_capture_options()->set_use_update_notifications(
      use_update_notifications);
  r->desktop_capture_options()->set_detect_updated_region(
      detect_updated_region);
  r->desktop_capture_options()->set_disable_effects(disable_effects);

#if defined(WEBRTC_WIN)
  bool allow_use_magnification_api;
  bool allow_directx_capturer;

  if (!iter->ReadBool(&allow_use_magnification_api) ||
      !iter->ReadBool(&allow_directx_capturer)) {
    return false;
  }

  r->desktop_capture_options()->set_allow_use_magnification_api(
      allow_use_magnification_api);
  r->desktop_capture_options()->set_allow_directx_capturer(
      allow_directx_capturer);
#endif  // defined(WEBRTC_WIN)

  return true;
}

// static
void ParamTraits<remoting::DesktopEnvironmentOptions>::Log(
    const remoting::DesktopEnvironmentOptions& p,
    std::string* l) {
  l->append("DesktopEnvironmentOptions()");
}

// remoting::protocol::ActionRequest

// static
void ParamTraits<remoting::protocol::ActionRequest>::Write(
    base::Pickle* m,
    const param_type& p) {
  std::string serialized_action_request;
  bool result = p.SerializeToString(&serialized_action_request);
  DCHECK(result);
  m->WriteString(serialized_action_request);
}

// static
bool ParamTraits<remoting::protocol::ActionRequest>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  std::string serialized_action_request;
  if (!iter->ReadString(&serialized_action_request))
    return false;

  return p->ParseFromString(serialized_action_request);
}

// static
void ParamTraits<remoting::protocol::ActionRequest>::Log(const param_type& p,
                                                         std::string* l) {
  l->append(base::StringPrintf("ActionRequest action: %d, id: %u", p.action(),
                               p.request_id()));
}

// remoting::protocol::VideoLayout

// static
void ParamTraits<remoting::protocol::VideoLayout>::Write(
    base::Pickle* m,
    const remoting::protocol::VideoLayout& p) {
  std::string serialized_video_layout;
  bool result = p.SerializeToString(&serialized_video_layout);
  DCHECK(result);
  m->WriteString(serialized_video_layout);
}

// static
bool ParamTraits<remoting::protocol::VideoLayout>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    remoting::protocol::VideoLayout* p) {
  std::string serialized_video_layout;
  if (!iter->ReadString(&serialized_video_layout))
    return false;

  return p->ParseFromString(serialized_video_layout);
}

// static
void ParamTraits<remoting::protocol::VideoLayout>::Log(
    const remoting::protocol::VideoLayout& p,
    std::string* l) {
  l->append(base::StringPrintf("protocol::VideoLayout(["));
  for (int i = 0; i < p.video_track_size(); i++) {
    remoting::protocol::VideoTrackLayout track = p.video_track(i);
    l->append("])");
    if (i != 0)
      l->append(",");
    l->append(base::StringPrintf("{(%d,%d) %dx%d}", track.position_x(),
                                 track.position_y(), track.width(),
                                 track.height()));
  }
  l->append("])");
}

// remoting::protocol::KeyboardLayout

// static
void ParamTraits<remoting::protocol::KeyboardLayout>::Write(
    base::Pickle* m,
    const remoting::protocol::KeyboardLayout& p) {
  std::string serialized_keyboard_layout;
  bool result = p.SerializeToString(&serialized_keyboard_layout);
  DCHECK(result);
  m->WriteString(serialized_keyboard_layout);
}

// static
bool ParamTraits<remoting::protocol::KeyboardLayout>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    remoting::protocol::KeyboardLayout* p) {
  std::string serialized_keyboard_layout;
  if (!iter->ReadString(&serialized_keyboard_layout))
    return false;

  return p->ParseFromString(serialized_keyboard_layout);
}

// static
void ParamTraits<remoting::protocol::KeyboardLayout>::Log(
    const remoting::protocol::KeyboardLayout& p,
    std::string* l) {
  l->append("[protocol::KeyboardLayout]");
}

// remoting::protocol::FileTransfer_Error

// static
void IPC::ParamTraits<remoting::protocol::FileTransfer_Error>::Write(
    base::Pickle* m,
    const param_type& p) {
  std::string serialized_file_transfer_error;
  bool result = p.SerializeToString(&serialized_file_transfer_error);
  DCHECK(result);
  m->WriteString(serialized_file_transfer_error);
}

// static
bool ParamTraits<remoting::protocol::FileTransfer_Error>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  std::string serialized_file_transfer_error;
  if (!iter->ReadString(&serialized_file_transfer_error))
    return false;

  return p->ParseFromString(serialized_file_transfer_error);
}

// static
void ParamTraits<remoting::protocol::FileTransfer_Error>::Log(
    const param_type& p,
    std::string* l) {
  std::ostringstream formatted;
  formatted << p;
  l->append(
      base::StringPrintf("FileTransfer Error: %s", formatted.str().c_str()));
}

// remoting::Monostate

// static
void IPC::ParamTraits<remoting::Monostate>::Write(base::Pickle*,
                                                  const param_type&) {}

// static
bool ParamTraits<remoting::Monostate>::Read(const base::Pickle*,
                                            base::PickleIterator*,
                                            param_type*) {
  return true;
}

// static
void ParamTraits<remoting::Monostate>::Log(const param_type&, std::string* l) {
  l->append("()");
}

}  // namespace IPC
