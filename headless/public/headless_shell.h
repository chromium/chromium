// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_SHELL_H_
#define HEADLESS_PUBLIC_HEADLESS_SHELL_H_

#include "content/public/app/content_main.h"

namespace headless {

// Start the headless shell applications.
// Note that the |ContentMainDelegate| is ignored and
// |HeadlessContentMainDelegate| is used instead.
int HeadlessShellMain(const content::ContentMainParams params);

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_SHELL_H_
