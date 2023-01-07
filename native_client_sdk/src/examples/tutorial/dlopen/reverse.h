// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_TUTORIAL_DLOPEN_REVERSE_H_
#define EXAMPLES_TUTORIAL_DLOPEN_REVERSE_H_

/* Allocate a new string that is the reverse of the given string. */
typedef char* (*TYPE_reverse)(const char*);
extern "C" char* Reverse(const char*);

#endif  // EXAMPLES_TUTORIAL_DLOPEN_REVERSE_H_
