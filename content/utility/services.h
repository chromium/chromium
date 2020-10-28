// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_SERVICES_H_
#define CONTENT_UTILITY_SERVICES_H_

namespace mojo {
class ServiceFactory;
}

namespace content {

void RegisterIOThreadServices(mojo::ServiceFactory& services);
void RegisterMainThreadServices(mojo::ServiceFactory& services);

}  // namespace content

#endif  // CONTENT_UTILITY_SERVICES_H_
