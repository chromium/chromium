// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_KEYS_LISTENER_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_KEYS_LISTENER_MANAGER_H_

#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "ui/base/accelerators/media_keys_listener.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

// The browser listens for media keys and uses them to control the active media
// session. However, there are cases where this behavior is undesirable, for
// example when an extension wants to handle media key presses instead. This
// class provides an interface for code outside of content (e.g. extensions) to
// receive global media key input instead of allowing the browser to use it to
// control the active session.
class MediaKeysListenerManager {
 public:
  // Returns the singleton instance.
  CONTENT_EXPORT static MediaKeysListenerManager* GetInstance();

  // Returns true if the MediaKeysListenerManager is enabled, taking OS and
  // feature flags into account.
  CONTENT_EXPORT static bool IsMediaKeysListenerManagerEnabled();

  // Start listening for a given media key. Returns true if the listener
  // successfully started listening for the key. This will prevent the
  // HardwareKeyMediaController from also handling the specified key.
  // Note: Delegates *must* call |StopWatchingMediaKey()| for each key code
  // they're listening to before destruction in order to prevent the
  // MediaKeysListenerManager from holding invalid pointers.
  // For instanced web apps that are beginning to listen for media keys,
  // the request_id of the media session that is associated with the web app
  // needs to be provided via |web_app_request_id|. Otherwise, this param can
  // be ignored.
  virtual bool StartWatchingMediaKey(ui::KeyboardCode key_code,
                                     ui::MediaKeysListener::Delegate* delegate,
                                     base::UnguessableToken web_app_request_id =
                                         base::UnguessableToken::Null()) = 0;

  // Stop listening for a given media key. This will free the key to be handled
  // by the HardwareKeyMediaController. Delegates must stop watching all keys
  // before they are destroyed in order to prevent the MediaKeysListenerManager
  // from holding invalid pointers.
  // For instanced web apps that are beginning to listen for media keys,
  // the request_id of the media session that is associated with the web app
  // needs to be provided via |web_app_request_id|. Otherwise, this param can
  // be ignored.
  virtual void StopWatchingMediaKey(ui::KeyboardCode key_code,
                                    ui::MediaKeysListener::Delegate* delegate,
                                    base::UnguessableToken web_app_request_id =
                                        base::UnguessableToken::Null()) = 0;

  // Prevent the browser from using media key presses to control the active
  // media session. This allows a caller to prevent the media key handling
  // without registering to receive the key events.  Note that this does not
  // prevent callers of |StartWatchingMediaKey()| from receiving media key
  // events. This does not need to be called if |StartWatchingMediaKey()| is
  // used, since |StartWatchingMediaKey()| will automatically prevent the
  // browser from using the media key presses.
  virtual void DisableInternalMediaKeyHandling() = 0;

  // Allows the browser to use media key presses to control the active media.
  // Only needs to be called if a call to |DisableInternalMediaKeyHandling()|
  // has been made.
  virtual void EnableInternalMediaKeyHandling() = 0;

 protected:
  virtual ~MediaKeysListenerManager();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_KEYS_LISTENER_MANAGER_H_
