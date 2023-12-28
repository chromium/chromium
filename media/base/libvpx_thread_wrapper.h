// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_LIBVPX_THREAD_WRAPPER_H_
#define MEDIA_BASE_LIBVPX_THREAD_WRAPPER_H_

namespace media {

// Initializes Media's libvpx thread wrapper. Should only be called once per
// process, and in isolation.
void InitLibVpxThreadWrapper();

}  // namespace media

#endif  // MEDIA_BASE_LIBVPX_THREAD_WRAPPER_H_
