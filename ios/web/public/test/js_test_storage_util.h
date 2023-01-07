// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_JS_TEST_STORAGE_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_JS_TEST_STORAGE_UTIL_H_

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

namespace web {
class WebFrame;
class WebState;

namespace test {

// These functions synchronously execute JavaScript to access different types of
// storage (e.g. localStorage, cookies,etc.).

// Sets a persistent cookie with `key`, `value` on `web_frame`. Returns true on
// success and false on failure.
bool SetCookie(WebFrame* web_frame, NSString* key, NSString* value);

// Gets a csv list of all cookies from `web_frame` and places it in `cookies`.
// Returns true on success and false on failure.
bool GetCookies(WebFrame* web_frame, NSString** cookies);

// Stores a given `key`, `value` in local storage on `web_frame`. If
// `error_message` is provided, then if an error occurs, it will be filled with
// the error message. Returns true on success and false on failure.
bool SetLocalStorage(WebFrame* web_frame,
                     NSString* key,
                     NSString* value,
                     NSString** error_message);

// Reads the value for the given `key` from local storage on `web_frame` and
// places it in `result`. If `error_message` is provided, then if an error
// occurs, it will be filled with the error message. Returns true on success and
// false on failure.
bool GetLocalStorage(WebFrame* web_frame,
                     NSString* key,
                     NSString** result,
                     NSString** error_message);

// Stores a given `key`, `value` in session storage on `web_frame`. If
// `error_message` is provided, then if an error occurs, it will be filled with
// the error message. Returns true on success and false on failure.
bool SetSessionStorage(WebFrame* web_frame,
                       NSString* key,
                       NSString* value,
                       NSString** error_message);

// Reads the value for the given `key` from session storage on `web_frame` and
// places it in `result`. If `error_message` is provided, then if an error
// occurs, it will be filled with the error message. Returns true on success and
// false on failure.
bool GetSessionStorage(WebFrame* web_frame,
                       NSString* key,
                       NSString** result,
                       NSString** error_message);

// Stores a given `key`, `value` in cache storage on `web_frame` + `web_state`.
// If `error_message` is provided, then if an error occurs, it will be filled
// with the error message. Returns true on success and false on failure.
bool SetCache(WebFrame* web_frame,
              WebState* web_state,
              NSString* key,
              NSString* value,
              NSString** error_message);

// Reads the value for the given `key` from session storage on `web_frame` +
// `web_state` and places it in `result`. If `error_message` is provided, then
// if an error occurs, it will be filled with the error message. Returns true on
// success and false on failure.
bool GetCache(WebFrame* web_frame,
              WebState* web_state,
              NSString* key,
              NSString** result,
              NSString** error_message);

// Stores a given `key`, `value` in IndexedDB on `web_frame` + `web_state`.
// If `error_message` is provided, then if an error occurs, it will be filled
// with the error message. Returns true on success and false on failure.
bool SetIndexedDB(WebFrame* web_frame,
                  WebState* web_state,
                  NSString* key,
                  NSString* value,
                  NSString** error_message);

// Reads the value for the given `key` from IndexedDB on `web_frame` +
// `web_state` and places it in `result`. If `error_message` is provided, then
// if an error occurs, it will be filled with the error message. Returns true on
// success and false on failure.
bool GetIndexedDB(WebFrame* web_frame,
                  WebState* web_state,
                  NSString* key,
                  NSString** result,
                  NSString** error_message);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_JS_TEST_STORAGE_UTIL_H_
