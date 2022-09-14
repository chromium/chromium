// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Workaround for preventing the leaking of local file contents.
 * See crbug.com/1122059.
 */

/** Beginning of anonymous object */
(function() {

/** @private */
// Store originals to prevent calling a modified version later.
const originalNavigatorShare_ = Navigator.prototype.share;
// Navigator.share is only supported in secure contexts, do not create function
// if it does not exist.
if (!originalNavigatorShare_) {
  return;
}

const originalNavigator_ = navigator;
const originalReflectApply_ = Reflect.apply;
const originalObjectDefineProperty_ = Object.defineProperty;

/**
 * Wraps navigator.share() to prevent sharing URLs with "file:" scheme.
 * NOTE: This code is sensitive and easy to break. See comments in
 * crbug.com/1122059 and review comments in crrev.com/c/2378274.
 * TODO:(crbug.com/1123689): Remove this workaround once WebKit fix is released.
 */
Navigator.prototype.share = function(data) {
  // Copy values to a new Object to prevent functions returning different
  // data from data.url. crbug.com/1122059#c23
  const validatedData = {};
  if (data.hasOwnProperty('files')) {
    originalObjectDefineProperty_(validatedData, 'files',
        { value: data.files, configurable: false, writable: false })
  }
  if (data.hasOwnProperty('text')) {
    originalObjectDefineProperty_(validatedData, 'text',
        { value: data.text, configurable: false, writable: false })
  }
  if (data.hasOwnProperty('title')) {
    originalObjectDefineProperty_(validatedData, 'title',
        { value: data.title, configurable: false, writable: false })
  }

  let url = undefined;
  if (data.hasOwnProperty('url')) {
    url = data['url']?.toString();

    let proceed = false;
    if (url === undefined) {
      // Allow url key to be set without value.
      proceed = true;
    } else {
      // file: URLs are not allowed.
      if (url.length >= 5 &&
          (url[0] == 'f' || url[0] == 'F') &&
          (url[1] == 'i' || url[1] == 'I') &&
          (url[2] == 'l' || url[2] == 'L') &&
          (url[3] == 'e' || url[3] == 'E') &&
          url[4] == ':') {
        proceed = false;
      } else {
        proceed = true;
      }
    }

    if (!proceed) {
      throw new Error("Sharing is not supported for this type of url.");
    }
  }

  originalObjectDefineProperty_(validatedData, 'url',
      { value: url, configurable: false, writable: false })

  return originalReflectApply_(originalNavigatorShare_,
                               originalNavigator_,
                               [validatedData]);
};

}());  // End of anonymous object
