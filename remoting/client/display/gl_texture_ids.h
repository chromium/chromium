// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_GL_TEXTURE_IDS_H_
#define REMOTING_CLIENT_DISPLAY_GL_TEXTURE_IDS_H_

namespace remoting {

const int kGlCursorTextureId = 0;
const int kGlCursorFeedbackTextureId = 1;

// GlDesktop may occupy more than one texture unit. This should be the last
// texture ID so that GlDesktop can use any id >= kGlDesktopFirstTextureId
// without conflict.
const int kGlDesktopFirstTextureId = 2;

}  // namespace remoting

#endif  // REMOTING_CLIENT_DISPLAY_GL_TEXTURE_IDS_H_
