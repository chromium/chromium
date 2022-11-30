// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_INJECTOR_CONSTANTS_LINUX_H_
#define REMOTING_HOST_INPUT_INJECTOR_CONSTANTS_LINUX_H_

namespace remoting {

// Enumerates direction of mouse scroll.
enum class ScrollDirection {
  DOWN = -1,
  UP = 1,
  NONE = 0,
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_INJECTOR_CONSTANTS_LINUX_H_
