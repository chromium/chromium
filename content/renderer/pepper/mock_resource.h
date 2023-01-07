// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_MOCK_RESOURCE_H_
#define CONTENT_RENDERER_PEPPER_MOCK_RESOURCE_H_

#include "ppapi/shared_impl/resource.h"

namespace content {

// Tests can derive from this to implement special test-specific resources.
// It's assumed that a test will only need one mock resource, so it can
// static_cast to get its own implementation.
class MockResource : public ppapi::Resource {
 public:
  MockResource(PP_Instance instance)
      : Resource(ppapi::OBJECT_IS_IMPL, instance) {}

 private:
  ~MockResource() override {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_MOCK_RESOURCE_H_
