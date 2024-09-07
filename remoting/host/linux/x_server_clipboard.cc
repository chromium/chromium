// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/host/linux/x_server_clipboard.h"

#include <limits>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/base/util.h"
#include "ui/gfx/x/extension_manager.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xproto.h"

namespace remoting {

XServerClipboard::XServerClipboard() = default;

XServerClipboard::~XServerClipboard() = default;

void XServerClipboard::Init(x11::Connection* connection,
                            const ClipboardChangedCallback& callback) {
  connection_ = connection;
  callback_ = callback;

  if (!connection_->xfixes().present()) {
    HOST_LOG << "X server does not support XFixes.";
    return;
  }

  clipboard_window_ = connection_->GenerateId<x11::Window>();
  connection_->CreateWindow({
      .wid = clipboard_window_,
      .parent = connection_->default_root(),
      .width = 1,
      .height = 1,
      .override_redirect = x11::Bool32(true),
  });

  // TODO(lambroslambrou): Use ui::X11AtomCache for this, either by adding a
  // dependency on ui/ or by moving X11AtomCache to base/.
  static const char* const kAtomNames[] = {"CLIPBOARD",        "INCR",
                                           "SELECTION_STRING", "TARGETS",
                                           "TIMESTAMP",        "UTF8_STRING"};
  static const int kNumAtomNames = std::size(kAtomNames);

  x11::Future<x11::InternAtomReply> futures[kNumAtomNames];
  for (size_t i = 0; i < kNumAtomNames; i++) {
    futures[i] = connection_->InternAtom({false, kAtomNames[i]});
  }
  connection_->Flush();
  x11::Atom atoms[kNumAtomNames];
  memset(atoms, 0, sizeof(atoms));
  for (size_t i = 0; i < kNumAtomNames; i++) {
    if (auto reply = futures[i].Sync()) {
      atoms[i] = reply->atom;
    } else {
      LOG(ERROR) << "Failed to intern atom(s)";
      break;
    }
  }
  clipboard_atom_ = atoms[0];
  large_selection_atom_ = atoms[1];
  selection_string_atom_ = atoms[2];
  targets_atom_ = atoms[3];
  timestamp_atom_ = atoms[4];
  utf8_string_atom_ = atoms[5];
  static_assert(kNumAtomNames >= 6, "kAtomNames is too small");

  connection_->xfixes().SelectSelectionInput(
      {static_cast<x11::Window>(clipboard_window_),
       static_cast<x11::Atom>(clipboard_atom_),
       x11::XFixes::SelectionEventMask::SetSelectionOwner});
  connection_->Flush();
}

void XServerClipboard::SetClipboard(const std::string& mime_type,
                                    const std::string& data) {
  DCHECK(connection_->Ready());

  if (clipboard_window_ == x11::Window::None) {
    return;
  }

  // Currently only UTF-8 is supported.
  if (mime_type != kMimeTypeTextUtf8) {
    return;
  }
  if (!base::IsStringUTF8AllowingNoncharacters(data)) {
    LOG(ERROR) << "ClipboardEvent: data is not UTF-8 encoded.";
    return;
  }

  data_ = data;

  AssertSelectionOwnership(x11::Atom::PRIMARY);
  AssertSelectionOwnership(clipboard_atom_);
}

void XServerClipboard::ProcessXEvent(const x11::Event& event) {
  if (clipboard_window_ == x11::Window::None) {
    return;
  }

  if (auto* property_notify = event.As<x11::PropertyNotifyEvent>()) {
    if (property_notify->window == clipboard_window_) {
      OnPropertyNotify(*property_notify);
    }
  } else if (auto* selection_notify = event.As<x11::SelectionNotifyEvent>()) {
    if (selection_notify->requestor == clipboard_window_) {
      OnSelectionNotify(*selection_notify);
    }
  } else if (auto* selection_request = event.As<x11::SelectionRequestEvent>()) {
    if (selection_request->owner == clipboard_window_) {
      OnSelectionRequest(*selection_request);
    }
  } else if (auto* selection_clear = event.As<x11::SelectionClearEvent>()) {
    if (selection_clear->owner == clipboard_window_) {
      OnSelectionClear(*selection_clear);
    }
  } else if (auto* xfixes_selection_notify =
                 event.As<x11::XFixes::SelectionNotifyEvent>()) {
    if (xfixes_selection_notify->window == clipboard_window_) {
      OnSetSelectionOwnerNotify(xfixes_selection_notify->selection,
                                xfixes_selection_notify->selection_timestamp);
    }
  }
}

void XServerClipboard::OnSetSelectionOwnerNotify(x11::Atom selection,
                                                 x11::Time timestamp) {
  // Protect against receiving new XFixes selection notifications whilst we're
  // in the middle of waiting for information from the current selection owner.
  // A reasonable timeout allows for misbehaving apps that don't respond
  // quickly to our requests.
  if (!get_selections_time_.is_null() &&
      (base::TimeTicks::Now() - get_selections_time_) < base::Seconds(5)) {
    // TODO(lambroslambrou): Instead of ignoring this notification, cancel any
    // pending request operations and ignore the resulting events, before
    // dispatching new requests here.
    return;
  }

  // Only process CLIPBOARD selections.
  if (selection != clipboard_atom_) {
    return;
  }

  // If we own the selection, don't request details for it.
  if (IsSelectionOwner(selection)) {
    return;
  }

  get_selections_time_ = base::TimeTicks::Now();

  // Before getting the value of the chosen selection, request the list of
  // target formats it supports.
  RequestSelectionTargets(selection);
}

void XServerClipboard::OnPropertyNotify(const x11::PropertyNotifyEvent& event) {
  if (large_selection_property_ != x11::Atom::None &&
      event.atom == large_selection_property_ &&
      event.state == x11::Property::NewValue) {
    auto req = connection()->GetProperty({
        .c_delete = true,
        .window = clipboard_window_,
        .property = large_selection_property_,
        .type = x11::Atom::Any,
        .long_length = std::numeric_limits<uint32_t>::max(),
    });
    if (auto reply = req.Sync()) {
      if (reply->type != x11::Atom::None) {
        // TODO(lambroslambrou): Properly support large transfers -
        // http://crbug.com/151447.

        // If the property is zero-length then the large transfer is complete.
        if (reply->value_len == 0) {
          large_selection_property_ = x11::Atom::None;
        }
      }
    }
  }
}

void XServerClipboard::OnSelectionNotify(
    const x11::SelectionNotifyEvent& event) {
  if (event.property != x11::Atom::None) {
    auto req = connection()->GetProperty({
        .c_delete = true,
        .window = clipboard_window_,
        .property = event.property,
        .type = x11::Atom::Any,
        .long_length = std::numeric_limits<uint32_t>::max(),
    });
    if (auto reply = req.Sync()) {
      if (reply->type == large_selection_atom_) {
        // Large selection - just read and ignore these for now.
        large_selection_property_ = event.property;
      } else {
        // Standard selection - call the selection notifier.
        large_selection_property_ = x11::Atom::None;
        if (reply->type != x11::Atom::None) {
          HandleSelectionNotify(event, reply->type, reply->format,
                                reply->value_len, reply->value->bytes());
          return;
        }
      }
    }
  }
  HandleSelectionNotify(event, x11::Atom::None, 0, 0, nullptr);
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
  if (!IsSelectionOwner(selection_event.selection)) {
    selection_event.property = x11::Atom::None;
  } else {
    selection_event.property = property;
    if (selection_event.target == static_cast<x11::Atom>(targets_atom_)) {
      SendTargetsResponse(selection_event.requestor, selection_event.property);
    } else if (selection_event.target ==
               static_cast<x11::Atom>(timestamp_atom_)) {
      SendTimestampResponse(selection_event.requestor,
                            selection_event.property);
    } else if (selection_event.target ==
                   static_cast<x11::Atom>(utf8_string_atom_) ||
               selection_event.target == x11::Atom::STRING) {
      SendStringResponse(selection_event.requestor, selection_event.property,
                         selection_event.target);
    }
  }
  connection_->SendEvent(selection_event, selection_event.requestor,
                         x11::EventMask::NoEvent);
}

void XServerClipboard::OnSelectionClear(const x11::SelectionClearEvent& event) {
  selections_owned_.erase(event.selection);
}

void XServerClipboard::SendTargetsResponse(x11::Window requestor,
                                           x11::Atom property) {
  // Respond advertising x11::Atom::STRING, UTF8_STRING and TIMESTAMP data for
  // the selection.
  x11::Atom targets[3] = {
      timestamp_atom_,
      utf8_string_atom_,
      x11::Atom::STRING,
  };
  connection_->ChangeProperty({
      .mode = x11::PropMode::Replace,
      .window = requestor,
      .property = property,
      .type = x11::Atom::ATOM,
      .format = CHAR_BIT * sizeof(x11::Atom),
      .data_len = std::size(targets),
      .data = base::MakeRefCounted<base::RefCountedStaticMemory>(
          base::as_byte_span(targets)),
  });
  connection_->Flush();
}

void XServerClipboard::SendTimestampResponse(x11::Window requestor,
                                             x11::Atom property) {
  // Respond with the timestamp of our selection; we always return
  // CurrentTime since our selections are set by remote clients, so there
  // is no associated local X event.

  // TODO(lambroslambrou): Should use a proper timestamp here instead of
  // CurrentTime.  ICCCM recommends doing a zero-length property append,
  // and getting a timestamp from the subsequent PropertyNotify event.
  x11::Time time = x11::Time::CurrentTime;
  connection_->ChangeProperty({
      .mode = x11::PropMode::Replace,
      .window = requestor,
      .property = property,
      .type = x11::Atom::INTEGER,
      .format = CHAR_BIT * sizeof(x11::Time),
      .data_len = 1,
      .data = base::MakeRefCounted<base::RefCountedStaticMemory>(
          base::byte_span_from_ref(time)),
  });
  connection_->Flush();
}

void XServerClipboard::SendStringResponse(x11::Window requestor,
                                          x11::Atom property,
                                          x11::Atom target) {
  if (!data_.empty()) {
    // Return the actual string data; we always return UTF8, regardless of
    // the configured locale.
    connection_->ChangeProperty({
        .mode = x11::PropMode::Replace,
        .window = requestor,
        .property = property,
        .type = target,
        .format = 8,
        .data_len = static_cast<uint32_t>(data_.size()),
        .data = base::MakeRefCounted<base::RefCountedStaticMemory>(
            base::as_byte_span(data_)),
    });
    connection_->Flush();
  }
}

void XServerClipboard::HandleSelectionNotify(
    const x11::SelectionNotifyEvent& event,
    x11::Atom type,
    int format,
    int item_count,
    const void* data) {
  bool finished = false;

  auto target = event.target;
  if (target == targets_atom_) {
    finished = HandleSelectionTargetsEvent(event, format, item_count, data);
  } else if (target == utf8_string_atom_ || target == x11::Atom::STRING) {
    finished = HandleSelectionStringEvent(event, format, item_count, data);
  }

  if (finished) {
    get_selections_time_ = base::TimeTicks();
  }
}

bool XServerClipboard::HandleSelectionTargetsEvent(
    const x11::SelectionNotifyEvent& event,
    int format,
    int item_count,
    const void* data) {
  auto selection = event.selection;
  if (event.property == targets_atom_) {
    if (data && format == 32) {
      const uint32_t* targets = static_cast<const uint32_t*>(data);
      for (int i = 0; i < item_count; i++) {
        if (targets[i] == static_cast<uint32_t>(utf8_string_atom_)) {
          RequestSelectionString(selection, utf8_string_atom_);
          return false;
        }
      }
    }
  }
  RequestSelectionString(selection, x11::Atom::STRING);
  return false;
}

bool XServerClipboard::HandleSelectionStringEvent(
    const x11::SelectionNotifyEvent& event,
    int format,
    int item_count,
    const void* data) {
  auto property = event.property;
  auto target = event.target;

  if (property != selection_string_atom_ || !data || format != 8) {
    return true;
  }

  std::string text(static_cast<const char*>(data), item_count);

  if (target == x11::Atom::STRING || target == utf8_string_atom_) {
    NotifyClipboardText(text);
  }

  return true;
}

void XServerClipboard::NotifyClipboardText(const std::string& text) {
  data_ = text;
  callback_.Run(kMimeTypeTextUtf8, data_);
}

void XServerClipboard::RequestSelectionTargets(x11::Atom selection) {
  connection_->ConvertSelection({clipboard_window_, selection, targets_atom_,
                                 targets_atom_, x11::Time::CurrentTime});
}

void XServerClipboard::RequestSelectionString(x11::Atom selection,
                                              x11::Atom target) {
  connection_->ConvertSelection({clipboard_window_, selection, target,
                                 selection_string_atom_,
                                 x11::Time::CurrentTime});
}

void XServerClipboard::AssertSelectionOwnership(x11::Atom selection) {
  connection_->SetSelectionOwner(
      {clipboard_window_, selection, x11::Time::CurrentTime});
  auto reply = connection_->GetSelectionOwner({selection}).Sync();
  auto owner = reply ? reply->owner : x11::Window::None;
  if (owner == clipboard_window_) {
    selections_owned_.insert(selection);
  } else {
    LOG(ERROR) << "XSetSelectionOwner failed for selection "
               << static_cast<uint32_t>(selection);
  }
}

bool XServerClipboard::IsSelectionOwner(x11::Atom selection) {
  return base::Contains(selections_owned_, selection);
}

}  // namespace remoting
