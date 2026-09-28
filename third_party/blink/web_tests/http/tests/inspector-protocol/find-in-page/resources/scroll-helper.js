// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The renderer scrolls to a find-in-page match on its own schedule, not
// necessarily before a FindInPage command's response arrives, so tests must
// wait for the scroll event instead of reading window.scrollY immediately
// after the command resolves.
//
// Get the promise before issuing the find command expected to scroll, and await
// it afterwards, e.g.:
//   const {session, dp} = await testRunner.startBlank(...);
//   ...
//   const scrollYPromise = waitForScroll(session);
//   await dp.FindInPage.findFirst({query: 'needle'});
//   const resultingY = await scrollYPromise;

(function() {

function waitForScroll(session) {
  return session.evaluateAsync(
      () => new Promise(
          resolve => window.addEventListener(
              'scroll', () => resolve(window.scrollY), {once: true})));
}

return {waitForScroll};
})()
