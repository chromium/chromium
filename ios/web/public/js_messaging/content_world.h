// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_CONTENT_WORLD_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_CONTENT_WORLD_H_

namespace web {

// Values representing specific JavaScript content worlds.
enum class ContentWorld {
  // Represents any content world. Specifies a feature which can live in any
  // content world. This is most commonly used for features which provide shared
  // functionality that other features may list as dependencies. Thus, these
  // features may exist in multiple content worlds to support larger features.
  kAnyContentWorld = 0,
  // Represents the page content world that is shared by the JavaScript of
  // the webpage. This value should only be used if a feature provides
  // JavaScript that needs to be accessible to the client JavaScript. For
  // example, JavaScript polyfills.
  kPageContentWorld,
  // Represents an isolated world that is not accessible to the JavaScript of
  // the webpage. This value should be used by default when the JavaScript does
  // not need to be exposed to the webpage, but especially when it is important
  // from a security standpoint to make a feature's JavaScript inaccessible to
  // client JavaScript.
  kIsolatedWorld,
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_CONTENT_WORLD_H_
