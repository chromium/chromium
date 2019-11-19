// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTOR_KEYS_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTOR_KEYS_H_

namespace content {

// This is a list of global descriptor keys to be used with the
// base::FileDescriptorStore object (see base/file_descriptor_store.h)

extern const char kV8SnapshotDataDescriptor[];
extern const char kV8Snapshot32DataDescriptor[];
extern const char kV8Snapshot64DataDescriptor[];
extern const char kV8ContextSnapshotDataDescriptor[];

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_DESCRIPTOR_KEYS_H_
