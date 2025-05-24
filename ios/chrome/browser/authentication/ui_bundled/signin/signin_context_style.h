// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_CONTEXT_STYLE_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_CONTEXT_STYLE_H_

// Enum to customize content on sign-in and sync screens.
// Add entries only when the visual content or layout (e.g. string, image,
// margin) differ from the default.
enum class SigninContextStyle {
  // Default content.
  kDefault,
  // Shown when joining a shared collaboration group while signed out or not
  // synced.
  kCollaborationJoinTabGroup,
  // Shown when sharing a collaboration group while signed out or not synced.
  kCollaborationShareTabGroup,
};

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_CONTEXT_STYLE_H_
