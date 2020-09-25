// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GUEST_HOST_H_
#define CONTENT_PUBLIC_BROWSER_GUEST_HOST_H_

namespace content {

// A GuestHost is the content API for a guest WebContents.
// Guests are top-level frames that can be embedded within other pages.
// The content module manages routing of input events and compositing, but all
// other operations are managed outside of content. To limit exposure of
// implementation details within content, content embedders must use this
// interface for loading, sizing, and cleanup of guests.
//
// This class currently only serves as a base class for BrowserPluginGuest, and
// its API can only be accessed by a BrowserPluginGuestDelegate.
class GuestHost {
 public:
  // Called when the GuestHost is about to be destroyed.
  virtual void WillDestroy() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GUEST_HOST_H_
