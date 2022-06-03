// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_INSPECT_H_
#define FUCHSIA_BASE_INSPECT_H_

namespace sys {
class ComponentInspector;
}  // namespace sys

namespace cr_fuchsia {

// Publish the Chromium version via the Inspect API. The lifetime of
// |inspector| has to be the same as the component it belongs to.
void PublishVersionInfoToInspect(sys::ComponentInspector* inspector);

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_INSPECT_H_