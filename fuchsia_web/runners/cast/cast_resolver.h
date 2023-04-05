// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_CAST_RESOLVER_H_
#define FUCHSIA_WEB_RUNNERS_CAST_CAST_RESOLVER_H_

#include <fidl/fuchsia.component.resolution/cpp/fidl.h>

// fuchsia.component.resolution.Resolver implementation for Cast applications.
class CastResolver final
    : public fidl::Server<fuchsia_component_resolution::Resolver> {
 public:
  CastResolver();
  ~CastResolver() override;

  CastResolver(const CastResolver&) = delete;
  CastResolver& operator=(const CastResolver&) = delete;

  // fuchsia_component_resolution::Resolver implementation.
  void Resolve(ResolveRequest& request,
               ResolveCompleter::Sync& completer) override;
  void ResolveWithContext(
      ResolveWithContextRequest& request,
      ResolveWithContextCompleter::Sync& completer) override;
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_CAST_RESOLVER_H_
