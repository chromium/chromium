// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_VISIBILITY_H_
#define CONTENT_PUBLIC_BROWSER_VISIBILITY_H_

namespace content {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
enum class Visibility {
  // The view is not part of any window (e.g. a non-active tab) or is part of a
  // window that is minimized or hidden (Cmd+H).
  HIDDEN,
  // The view is not visible on any screen despite not being HIDDEN. This can be
  // because it is covered by other windows and/or because it is outside the
  // bounds of the screen.
  OCCLUDED,
  // The view is visible on the screen.
  VISIBLE,
  // Note: Users may see HIDDEN/OCCLUDED views via capture (e.g. screenshots,
  // mirroring).
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_VISIBILITY_H_
