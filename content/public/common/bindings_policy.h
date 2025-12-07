// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_
#define CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_

#include "base/containers/enum_set.h"

namespace content {

// This enum specifies values for the types of JavaScript bindings exposed to
// renderers.
enum class BindingsPolicyValue {
  kFirstValue,

  // ----- WebUI Bindings -----

  // HTML-based UI bindings that allows the JS content to send JSON-encoded
  // data back to the browser process.
  // These bindings must not be exposed to normal web content.
  kWebUi = kFirstValue,
  // HTML-based UI bindings that allows access to Mojo system API. The Mojo
  // system API provides the ability to create Mojo primitives such as message
  // and data pipes, as well as connecting to named services exposed by the
  // browser.
  // These bindings must not be exposed to normal web content.
  kMojoWebUi,

  // Other types of bindings in the future can go here.

  kLastValue = kMojoWebUi,
};

using BindingsPolicySet = base::EnumSet<BindingsPolicyValue,
                                        BindingsPolicyValue::kFirstValue,
                                        BindingsPolicyValue::kLastValue>;

// The set of WebUI bindings.
inline constexpr BindingsPolicySet kWebUIBindingsPolicySet =
    BindingsPolicySet::FromRange(BindingsPolicyValue::kWebUi,
                                 BindingsPolicyValue::kMojoWebUi);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_BINDINGS_POLICY_H_
