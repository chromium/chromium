// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/x_server_clipboard.h"

#include "base/callback.h"
#include "base/stl_util.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/base/util.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_util.h"

namespace remoting {

XServerClipboard::XServerClipboard()
    : clipboard_window_(x11::None),
      clipboard_atom_(x11::None),
      large_selection_atom_(x11::None),
      selection_string_atom_(x11::None),
      targets_atom_(x11::None),
      timestamp_atom_(x11::None),
      utf8_string_atom_(x11::None),
      large_selection_property_(x11::None) {}

XServerClipboard::~XServerClipboard() = default;

void XServerClipboard::Init(x11::Connection* connection,
                            const ClipboardChangedCallback& callback) {
  connection_ = connection;
  callback_ = callback;

  // If any of these X API calls fail, an X Error will be raised, crashing the
  // process.  This is unlikely to occur in practice, and even if it does, it
  // would mean the X server is in a bad state, so it's not worth trying to
  // trap such errors here.

  // TODO(lambroslambrou): Consider using ScopedXErrorHandler here, or consider
  // placing responsibility for handling X Errors outside this class, since
  // X Error handlers are global to all X connections.

  if (!connection_->xfixes().present()) {
    HOST_LOG << "X server does not support XFixes.";
    return;
  }

  // Let the server know the client version.
  connection_->xfixes().QueryVersion(
      {x11::XFixes::major_version, x11::XFixes::minor_version});

  clipboard_window_ =
      XCreateSimpleWindow(connection_->display(),
                          XDefaultRootWindow(connection_->display()), 0, 0, 1,
                          1,  // x, y, width, height
                          0, 0, 0);

  // TODO(lambroslambrou): Use ui::X11AtomCache for this, either by adding a
  // dependency on ui/ or by moving X11AtomCache to base/.
  static const char* const kAtomNames[] = {"CLIPBOARD",        "INCR",
                                           "SELECTION_STRING", "TARGETS",
                                           "TIMESTAMP",        "UTF8_STRING"};
  static const int kNumAtomNames = base::size(kAtomNames);

  Atom atoms[kNumAtomNames];
  if (XInternAtoms(connection_->display(), const_cast<char**>(kAtomNames),
                   kNumAtomNames, x11::False, atoms)) {
    clipboard_atom_ = atoms[0];
    large_selection_atom_ = atoms[1];
    selection_string_atom_ = atoms[2];
    targets_atom_ = atoms[3];
    timestamp_atom_ = atoms[4];
    utf8_string_atom_ = atoms[5];
    static_assert(kNumAtomNames >= 6, "kAtomNames is too small");
  } else {
    LOG(ERROR) << "XInternAtoms failed";
  }

  connection_->xfixes().SelectSelectionInput(
      {static_cast<x11::Window>(clipboard_window_),
       static_cast<x11::Atom>(clipboard_atom_),
       x11::XFixes::SelectionEventMask::SetSelectionOwner});
  connection_->Flush();
}

void XServerClipboard::SetClipboard(const std::string& mime_type,
                                    const std::string& data) {
  DCHECK(connection_->display());

  if (clipboard_window_ == x11::None)
    return;

  // Currently only UTF-8 is supported.
  if (mime_type != kMimeTypeTextUtf8)
    return;
  if (!StringIsUtf8(data.c_str(), data.length())) {
    LOG(ERROR) << "ClipboardEvent: data is not UTF-8 encoded.";
    return;
  }

  data_ = data;

  AssertSelectionOwnership(static_cast<uint32_t>(x11::Atom::PRIMARY));
  AssertSelectionOwnership(clipboard_atom_);
}

void XServerClipboard::ProcessXEvent(const x11::Event& event) {
  if (clipboard_window_ == x11::None ||
      event.window() != static_cast<x11::Window>(clipboard_window_)) {
    return;
  }

  if (auto* property_notify = event.As<x11::PropertyNotifyEvent>())
    OnPropertyNotify(*property_notify);
  else if (auto* selection_notify = event.As<x11::SelectionNotifyEvent>())
    OnSelectionNotify(*selection_notify);
  else if (auto* selection_request = event.As<x11::SelectionRequestEvent>())
    OnSelectionRequest(*selection_request);
  else if (auto* selection_clear = event.As<x11::SelectionClearEvent>())
    OnSelectionClear(*selection_clear);

  if (auto* xfixes_selection_notify =
          event.As<x11::XFixes::SelectionNotifyEvent>()) {
    OnSetSelectionOwnerNotify(
        static_cast<uint32_t>(xfixes_selection_notify->selection),
        static_cast<uint32_t>(xfixes_selection_notify->selection_timestamp));
  }
}

void XServerClipboard::OnSetSelectionOwnerNotify(Atom selection,
                                                 Time timestamp) {
  // Protect against receiving new XFixes selection notifications whilst we're
  // in the middle of waiting for information from the current selection owner.
  // A reasonable timeout allows for misbehaving apps that don't respond
  // quickly to our requests.
  if (!get_selections_time_.is_null() &&
      (base::TimeTicks::Now() - get_selections_time_) <
          base::TimeDelta::FromSeconds(5)) {
    // TODO(lambroslambrou): Instead of ignoring this notification, cancel any
    // pending request operations and ignore the resulting events, before
    // dispatching new requests here.
    return;
  }

  // Only process CLIPBOARD selections.
  if (selection != clipboard_atom_)
    return;

  // If we own the selection, don't request details for it.
  if (IsSelectionOwner(selection))
    return;

  get_selections_time_ = base::TimeTicks::Now();

  // Before getting the value of the chosen selection, request the list of
  // target formats it supports.
  RequestSelectionTargets(selection);
}

void XServerClipboard::OnPropertyNotify(const x11::PropertyNotifyEvent& event) {
  if (large_selection_property_ != x11::None &&
      event.atom == static_cast<x11::Atom>(large_selection_property_) &&
      event.state == x11::Property::NewValue) {
    Atom type;
    int format;
    unsigned long item_count, after;
    unsigned char* data;
    XGetWindowProperty(connection_->display(), clipboard_window_,
                       large_selection_property_, 0, ~0L, x11::True,
                       AnyPropertyType, &type, &format, &item_count, &after,
                       &data);
    if (type != x11::None) {
      // TODO(lambroslambrou): Properly support large transfers -
      // http://crbug.com/151447.
      XFree(data);

      // If the property is zero-length then the large transfer is complete.
      if (item_count == 0)
        large_selection_property_ = x11::None;
    }
  }
}

void XServerClipboard::OnSelectionNotify(
    const x11::SelectionNotifyEvent& event) {
  if (event.property != x11::Atom::None) {
    Atom type;
    int format;
    unsigned long item_count, after;
    unsigned char* data;
    XGetWindowProperty(connection_->display(), clipboard_window_,
                       static_cast<uint32_t>(event.property), 0, ~0L, x11::True,
                       AnyPropertyType, &type, &format, &item_count, &after,
                       &data);
    if (type == large_selection_atom_) {
      // Large selection - just read and ignore these for now.
      large_selection_property_ = static_cast<uint32_t>(event.property);
    } else {
      // Standard selection - call the selection notifier.
      large_selection_property_ = x11::None;
      if (type != x11::None) {
        HandleSelectionNotify(event, type, format, item_count, data);
        XFree(data);
        return;
      }
    }
  }
  HandleSelectionNotify(event, 0, 0, 0, nullptr);
}

void XServerClipboard::OnSelectionRequest(
    const x11::SelectionRequestEvent& event) {
  x11::SelectionNotifyEvent selection_event;
  selection_event.requestor = event.requestor;
  selection_event.selection = event.selection;
  selection_event.time = event.time;
  selection_event.target = event.target;
  auto property =
      event.property == x11::Atom::None ? event.target : event.property;
  if (!IsSelectionOwner(static_cast<uint32_t>(selection_event.selection))) {
    selection_event.property = x11::Atom::None;
  } else {
    selection_event.property = property;
    if (selection_event.target == static_cast<x11::Atom>(targets_atom_)) {
      SendTargetsResponse(static_cast<uint32_t>(selection_event.requestor),
                          static_cast<uint32_t>(selection_event.property));
    } else if (selection_event.target ==
               static_cast<x11::Atom>(timestamp_atom_)) {
      SendTimestampResponse(static_cast<uint32_t>(selection_event.requestor),
                            static_cast<uint32_t>(selection_event.property));
    } else if (selection_event.target ==
                   static_cast<x11::Atom>(utf8_string_atom_) ||
               selection_event.target == x11::Atom::STRING) {
      SendStringResponse(static_cast<uint32_t>(selection_event.requestor),
                         static_cast<uint32_t>(selection_event.property),
                         static_cast<uint32_t>(selection_event.target));
    }
  }
  x11::SendEvent(selection_event, selection_event.requestor,
                 x11::EventMask::NoEvent, connection_);
}

void XServerClipboard::OnSelectionClear(const x11::SelectionClearEvent& event) {
  selections_owned_.erase(static_cast<uint32_t>(event.selection));
}

void XServerClipboard::SendTargetsResponse(Window requestor, Atom property) {
  // Respond advertising x11::Atom::STRING, UTF8_STRING and TIMESTAMP data for
  // the selection.
  Atom targets[3];
  targets[0] = timestamp_atom_;
  targets[1] = utf8_string_atom_;
  targets[2] = static_cast<uint32_t>(x11::Atom::STRING);
  XChangeProperty(connection_->display(), requestor, property,
                  static_cast<uint32_t>(x11::Atom::ATOM), 32, PropModeReplace,
                  reinterpret_cast<unsigned char*>(targets), 3);
}

void XServerClipboard::SendTimestampResponse(Window requestor, Atom property) {
  // Respond with the timestamp of our selection; we always return
  // CurrentTime since our selections are set by remote clients, so there
  // is no associated local X event.

  // TODO(lambroslambrou): Should use a proper timestamp here instead of
  // CurrentTime.  ICCCM recommends doing a zero-length property append,
  // and getting a timestamp from the subsequent PropertyNotify event.
  Time time = x11::CurrentTime;
  XChangeProperty(connection_->display(), requestor, property,
                  static_cast<uint32_t>(x11::Atom::INTEGER), 32,
                  PropModeReplace, reinterpret_cast<unsigned char*>(&time), 1);
}

void XServerClipboard::SendStringResponse(Window requestor,
                                          Atom property,
                                          Atom target) {
  if (!data_.empty()) {
    // Return the actual string data; we always return UTF8, regardless of
    // the configured locale.
    XChangeProperty(
        connection_->display(), requestor, property, target, 8, PropModeReplace,
        reinterpret_cast<unsigned char*>(const_cast<char*>(data_.data())),
        data_.size());
  }
}

void XServerClipboard::HandleSelectionNotify(
    const x11::SelectionNotifyEvent& event,
    Atom type,
    int format,
    int item_count,
    void* data) {
  bool finished = false;

  auto target = static_cast<uint32_t>(event.target);
  if (target == targets_atom_) {
    finished = HandleSelectionTargetsEvent(event, format, item_count, data);
  } else if (target == utf8_string_atom_ ||
             target == static_cast<uint32_t>(x11::Atom::STRING)) {
    finished = HandleSelectionStringEvent(event, format, item_count, data);
  }

  if (finished)
    get_selections_time_ = base::TimeTicks();
}

bool XServerClipboard::HandleSelectionTargetsEvent(
    const x11::SelectionNotifyEvent& event,
    int format,
    int item_count,
    void* data) {
  auto selection = static_cast<uint32_t>(event.selection);
  if (event.property == static_cast<x11::Atom>(targets_atom_)) {
    if (data && format == 32) {
      // The XGetWindowProperty man-page specifies that the returned
      // property data will be an array of |long|s in the case where
      // |format| == 32.  Although the items are 32-bit values (as stored and
      // sent over the X protocol), Xlib presents the data to the client as an
      // array of |long|s, with zero-padding on a 64-bit system where |long|
      // is bigger than 32 bits.
      const long* targets = static_cast<const long*>(data);
      for (int i = 0; i < item_count; i++) {
        if (targets[i] == static_cast<long>(utf8_string_atom_)) {
          RequestSelectionString(selection, utf8_string_atom_);
          return false;
        }
      }
    }
  }
  RequestSelectionString(selection, static_cast<uint32_t>(x11::Atom::STRING));
  return false;
}

bool XServerClipboard::HandleSelectionStringEvent(
    const x11::SelectionNotifyEvent& event,
    int format,
    int item_count,
    void* data) {
  auto property = static_cast<uint32_t>(event.property);
  auto target = static_cast<uint32_t>(event.target);

  if (property != selection_string_atom_ || !data || format != 8)
    return true;

  std::string text(static_cast<char*>(data), item_count);

  if (target == static_cast<uint32_t>(x11::Atom::STRING) ||
      target == utf8_string_atom_)
    NotifyClipboardText(text);

  return true;
}

void XServerClipboard::NotifyClipboardText(const std::string& text) {
  data_ = text;
  callback_.Run(kMimeTypeTextUtf8, data_);
}

void XServerClipboard::RequestSelectionTargets(Atom selection) {
  XConvertSelection(connection_->display(), selection, targets_atom_,
                    targets_atom_, clipboard_window_, x11::CurrentTime);
}

void XServerClipboard::RequestSelectionString(Atom selection, Atom target) {
  XConvertSelection(connection_->display(), selection, target,
                    selection_string_atom_, clipboard_window_,
                    x11::CurrentTime);
}

void XServerClipboard::AssertSelectionOwnership(Atom selection) {
  XSetSelectionOwner(connection_->display(), selection, clipboard_window_,
                     x11::CurrentTime);
  if (XGetSelectionOwner(connection_->display(), selection) ==
      clipboard_window_) {
    selections_owned_.insert(selection);
  } else {
    LOG(ERROR) << "XSetSelectionOwner failed for selection " << selection;
  }
}

bool XServerClipboard::IsSelectionOwner(Atom selection) {
  return selections_owned_.find(selection) != selections_owned_.end();
}

}  // namespace remoting
