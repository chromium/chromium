// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_PROXY_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_PROXY_IMPL_H_

struct PPB_Proxy_Private;

namespace content {

class PPB_Proxy_Impl {
 public:
  static const PPB_Proxy_Private* GetInterface();
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_PROXY_IMPL_H_
