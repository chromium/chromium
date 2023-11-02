// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_MOCK_RESOURCE_H_
#define PPAPI_PROXY_MOCK_RESOURCE_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/resource.h"

namespace ppapi {
namespace proxy {

class MockResource : public ppapi::Resource {
 public:
  MockResource(const ppapi::HostResource& resource);

  MockResource(const MockResource&) = delete;
  MockResource& operator=(const MockResource&) = delete;

  virtual ~MockResource();
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_MOCK_RESOURCE_H_
