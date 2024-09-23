// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginEmbedder handles messages coming from a BrowserPlugin's
// embedder that are not directed at any particular existing guest process.
// In the beginning, when a BrowserPlugin instance in the embedder renderer
// process requests an initial navigation, the WebContents for that renderer
// renderer creates a BrowserPluginEmbedder for itself. The
// BrowserPluginEmbedder, in turn, forwards the requests to a
// BrowserPluginGuestManager, which creates and manages the lifetime of the new
// guest.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_

#include "base/memory/raw_ptr.h"

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace content {

class BrowserPluginGuest;
class BrowserPluginGuestManager;
class WebContents;
class WebContentsImpl;

// TODO(wjmaclean): Get rid of "BrowserPlugin" in the name of this class.
// Perhaps "WebContentsEmbedderDelegate" would be better?
class BrowserPluginEmbedder {
 public:
  BrowserPluginEmbedder(const BrowserPluginEmbedder&) = delete;
  BrowserPluginEmbedder& operator=(const BrowserPluginEmbedder&) = delete;

  ~BrowserPluginEmbedder();

  static BrowserPluginEmbedder* Create(WebContentsImpl* web_contents);

  // Closes modal dialogs in all of the guests.
  void CancelGuestDialogs();

  // Used to handle special keyboard events.
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event);

  // Returns the "full page" guest if there is one. That is, if there is a
  // single BrowserPlugin in the embedder which takes up the full page, then it
  // is returned.
  BrowserPluginGuest* GetFullPageGuest();

  // Polls all guests for this web contents and returns true if any of them
  // are currently audible.
  bool AreAnyGuestsCurrentlyAudible();

 private:
  explicit BrowserPluginEmbedder(WebContentsImpl* web_contents);

  BrowserPluginGuestManager* GetBrowserPluginGuestManager() const;

  // Pointer to the WebContentsImpl that owns this object.
  raw_ptr<WebContentsImpl, DanglingUntriaged> web_contents_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_EMBEDDER_H_
