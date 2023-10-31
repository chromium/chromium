// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Don't include this file from any .h files because it pulls in some X headers.

#ifndef REMOTING_HOST_LINUX_X_SERVER_CLIPBOARD_H_
#define REMOTING_HOST_LINUX_X_SERVER_CLIPBOARD_H_

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/xfixes.h"
#include "ui/gfx/x/xproto.h"

namespace remoting {

// A class to allow manipulation of the X clipboard, using only X API calls.
// This class is not thread-safe, so all its methods must be called on the
// application's main event-processing thread.
class XServerClipboard {
 public:
  // Called when new clipboard data has been received from the owner of the X
  // selection (primary or clipboard).
  // |mime_type| is the MIME type associated with the data. This will be one of
  // the types listed in remoting/base/constants.h.
  // |data| is the clipboard data from the associated X event, encoded with the
  // specified MIME-type.
  typedef base::RepeatingCallback<void(const std::string& mime_type,
                                       const std::string& data)>
      ClipboardChangedCallback;

  XServerClipboard();

  XServerClipboard(const XServerClipboard&) = delete;
  XServerClipboard& operator=(const XServerClipboard&) = delete;

  ~XServerClipboard();

  // Start monitoring |connection|'s selections, and invoke |callback| whenever
  // their content changes. The caller must ensure |connection| is still valid
  // whenever any other methods are called on this object.
  void Init(x11::Connection* connection,
            const ClipboardChangedCallback& callback);

  // Copy data to the X Clipboard.  This acquires ownership of the
  // PRIMARY and CLIPBOARD selections.
  void SetClipboard(const std::string& mime_type, const std::string& data);

  // Process |event| if it is an X selection notification. The caller should
  // invoke this for every event it receives from |connection|.
  void ProcessXEvent(const x11::Event& event);

 private:
  // Handlers called by ProcessXEvent() for each event type.
  void OnSetSelectionOwnerNotify(x11::Atom selection, x11::Time timestamp);
  void OnPropertyNotify(const x11::PropertyNotifyEvent& event);
  void OnSelectionNotify(const x11::SelectionNotifyEvent& event);
  void OnSelectionRequest(const x11::SelectionRequestEvent& event);
  void OnSelectionClear(const x11::SelectionClearEvent& event);

  // Used by OnSelectionRequest() to respond to requests for details of our
  // clipboard content. This is done by changing the property |property| of the
  // |requestor| window (these values come from the XSelectionRequestEvent).
  // |target| must be a string type (STRING or UTF8_STRING).
  void SendTargetsResponse(x11::Window requestor, x11::Atom property);
  void SendTimestampResponse(x11::Window requestor, x11::Atom property);
  void SendStringResponse(x11::Window requestor,
                          x11::Atom property,
                          x11::Atom target);

  // Called by OnSelectionNotify() when the selection owner has replied to a
  // request for information about a selection.
  // |event| is the raw X event from the notification.
  // |type|, |format| etc are the results from XGetWindowProperty(), or 0 if
  // there is no associated data.
  void HandleSelectionNotify(const x11::SelectionNotifyEvent& event,
                             x11::Atom type,
                             int format,
                             int item_count,
                             const void* data);

  // These methods return true if selection processing is complete, false
  // otherwise. They are called from HandleSelectionNotify(), and take the same
  // arguments.
  bool HandleSelectionTargetsEvent(const x11::SelectionNotifyEvent& event,
                                   int format,
                                   int item_count,
                                   const void* data);
  bool HandleSelectionStringEvent(const x11::SelectionNotifyEvent& event,
                                  int format,
                                  int item_count,
                                  const void* data);

  // Notify the registered callback of new clipboard text.
  void NotifyClipboardText(const std::string& text);

  // These methods trigger the X server or selection owner to send back an
  // event containing the requested information.
  void RequestSelectionTargets(x11::Atom selection);
  void RequestSelectionString(x11::Atom selection, x11::Atom target);

  // Assert ownership of the specified |selection|.
  void AssertSelectionOwnership(x11::Atom selection);
  bool IsSelectionOwner(x11::Atom selection);

  x11::Connection* connection() { return connection_; }

  // Stores the connection supplied to Init().
  raw_ptr<x11::Connection> connection_ = nullptr;

  // Window through which clipboard events are received, or BadValue if the
  // window could not be created.
  x11::Window clipboard_window_ = x11::Window::None;

  // Cached atoms for various strings, initialized during Init().
  x11::Atom clipboard_atom_ = x11::Atom::None;
  x11::Atom large_selection_atom_ = x11::Atom::None;
  x11::Atom selection_string_atom_ = x11::Atom::None;
  x11::Atom targets_atom_ = x11::Atom::None;
  x11::Atom timestamp_atom_ = x11::Atom::None;
  x11::Atom utf8_string_atom_ = x11::Atom::None;

  // The set of X selections owned by |clipboard_window_| (can be Primary or
  // Clipboard or both).
  std::set<x11::Atom> selections_owned_;

  // Clipboard content to return to other applications when |clipboard_window_|
  // owns a selection.
  std::string data_;

  // Stores the property to use for large transfers, or None if a large
  // transfer is not currently in-progress.
  x11::Atom large_selection_property_ = x11::Atom::None;

  // Remembers the start time of selection processing, and is set to null when
  // processing is complete. This is used to decide whether to begin processing
  // a new selection or continue with the current selection.
  base::TimeTicks get_selections_time_;

  // |callback| argument supplied to Init().
  ClipboardChangedCallback callback_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_X_SERVER_CLIPBOARD_H_
