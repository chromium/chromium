// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// EME creates session IDs dynamically, so we have no idea what it will be.
// As the tests only need to create a single session, keep track of the
// last session ID created.
var savedSessionId = 'UnknownSessionId';

function createPersistentSession() {
  // This function creates a persistent-license type session, and resolves
  // with the created session object on success.
  return navigator
      .requestMediaKeySystemAccess(
          'org.chromium.externalclearkey', [{
            initDataTypes: ['keyids'],
            audioCapabilities: [
              // Include a set of codecs that should cover all user agents.
              {contentType: 'audio/mp4; codecs="mp4a.40.2"'},
              {contentType: 'audio/webm; codecs="opus"'}
            ],
            persistentState: 'required',
            sessionTypes: ['persistent-license'],
          }])
      .then(function(access) {
        return access.createMediaKeys();
      })
      .then(function(mediaKeys) {
        return mediaKeys.createSession('persistent-license');
      });
}

function handleMessageEvent(e) {
  var session = e.target;
  var te = new TextEncoder();
  var license = te.encode(
      '{"keys":[{"kty":"oct","k":"tQ0bJVWb6b0KPL6KtZIy_A","kid":"LwVHf8JLtPrv2GUXFW2v_A"}],"type":"persistent-license"}');

  savedSessionId = session.sessionId;
  return session.update(license).then(() => true, () => false);
}

async function setMediaLicense() {
  var te = new TextEncoder();
  var initData = te.encode('{"kids":["LwVHf8JLtPrv2GUXFW2v_A"]}');

  try {
    const session = await createPersistentSession();
    // generateRequest() will trigger a 'message' event, which we need to
    // wait for in order to call update() which provides the license.
    const handled = new Promise((resolve, reject) => {
      session.addEventListener('message', (e) => {
        handleMessageEvent(e).then(resolve, reject)
      }, false);
    });
    await session.generateRequest('keyids', initData);
    await handled;
    return true;
  } catch {
    return false;
  }
}

async function hasMediaLicense() {
  try {
    const session = await createPersistentSession();
    const result = await session.load(savedSessionId);
    // |result| is a boolean, indicating if the session was loaded or not.
    return result;
  }catch {
    return false;
  }
}