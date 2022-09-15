// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_THREE_D_API_TYPES_H_
#define CONTENT_PUBLIC_COMMON_THREE_D_API_TYPES_H_

// This file describes the kinds of 3D APIs exposed to client code. It
// is mainly used to provide more precise messages when access to
// these APIs is restricted for some reason.

namespace content {

enum ThreeDAPIType {
  THREE_D_API_TYPE_WEBGL,
  THREE_D_API_TYPE_PEPPER_3D,
  THREE_D_API_TYPE_LAST = THREE_D_API_TYPE_PEPPER_3D

};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_THREE_D_API_TYPES_H_
