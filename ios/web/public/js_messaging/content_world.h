// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_CONTENT_WORLD_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_CONTENT_WORLD_H_

namespace web {

// Values representing specific JavaScript content worlds.
enum class ContentWorld {
  // Represents any content world.
  kAnyContentWorld = 0,
  // Represents the page content world which is shared by the JavaScript of
  // the webpage. This value should only be used if a feature provides
  // JavaScript which needs to be accessible to the client JavaScript. For
  // example, JavaScript polyfills.
  kPageContentWorld,
  // Represents an isolated world that is not accessible to the JavaScript of
  // the webpage. This value should be used when it is important from a
  // security standpoint to make a feature's JavaScript inaccessible to
  // client JavaScript. Isolated worlds are supported only on iOS 14+, so
  // using the value on earlier iOS versions will trigger a DCHECK.
  kIsolatedWorldOnly,
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_CONTENT_WORLD_H_
