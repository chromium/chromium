// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the AppView API.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_CONSTANTS_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_CONSTANTS_H_

namespace appview {

// API namespace for the *embedder*. The embedder and guest use different APIs.
extern const char kEmbedderAPINamespace[];

// Create parameters.
extern const char kAppID[];
extern const char kData[];

// Parameters/properties on events.
extern const char kEmbedderID[];
extern const char kGuestInstanceID[];

}  // namespace appview

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_APP_VIEW_APP_VIEW_CONSTANTS_H_
