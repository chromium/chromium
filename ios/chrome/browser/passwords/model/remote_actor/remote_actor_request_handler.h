// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_REQUEST_HANDLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_REQUEST_HANDLER_H_

class Browser;
struct RemoteActorRequest;

// Handles the Remote Actor credential sharing `request` in `browser`: decrypts
// its payload on a background thread if needed, checks the preconditions, then
// shows the credential bottom sheet, or opens the Gemini callback URL on
// failure. Does nothing if `browser` is null or destroyed during the
// decryption.
void HandleRemoteActorCredentialSharingRequest(Browser* browser,
                                               RemoteActorRequest request);

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_REQUEST_HANDLER_H_
