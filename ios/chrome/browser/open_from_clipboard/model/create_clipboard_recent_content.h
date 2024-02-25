// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPEN_FROM_CLIPBOARD_MODEL_CREATE_CLIPBOARD_RECENT_CONTENT_H_
#define IOS_CHROME_BROWSER_OPEN_FROM_CLIPBOARD_MODEL_CREATE_CLIPBOARD_RECENT_CONTENT_H_

#include <memory>

class ClipboardRecentContent;

// Creates a new instance of ClipboardRecentContentIOS. The returned object
// is fully initialized and can be registered as global singleton.
//
// This helper function allow the construction of ClipboardRecentContentIOS
// from a pure C++ (ClipboardRecentContentIOS is an Objective-C++).
std::unique_ptr<ClipboardRecentContent> CreateClipboardRecentContentIOS();

#endif  // IOS_CHROME_BROWSER_OPEN_FROM_CLIPBOARD_MODEL_CREATE_CLIPBOARD_RECENT_CONTENT_H_
