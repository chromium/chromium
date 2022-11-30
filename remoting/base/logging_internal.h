// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_LOGGING_INTERNAL_H_
#define REMOTING_BASE_LOGGING_INTERNAL_H_

namespace remoting {

// Common initialization for all platforms, called by InitHostLogging().
void InitHostLoggingCommon();

}  // namespace remoting

#endif  // REMOTING_BASE_LOGGING_INTERNAL_H_
