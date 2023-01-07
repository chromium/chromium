// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_TUTORIAL_DLOPEN_EIGHTBALL_H_
#define EXAMPLES_TUTORIAL_DLOPEN_EIGHTBALL_H_

/* Return an answer. Question not required */
typedef char* (*TYPE_eightball)(void);
extern "C" const char* Magic8Ball();

#endif  // EXAMPLES_TUTORIAL_DLOPEN_EIGHTBALL_H_
