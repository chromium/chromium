// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/cast_resolver.h"

#include <fuchsia/component/cpp/fidl.h>
#include <fuchsia/component/decl/cpp/fidl.h>

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_piece.h"

namespace {

using fuchsia::component::decl::Capability;
using fuchsia::component::decl::Component;
using fuchsia::component::decl::DependencyType;
using fuchsia::component::decl::Expose;
using fuchsia::component::decl::ExposeProtocol;
using fuchsia::component::decl::Protocol;
using fuchsia::component::decl::Ref;
using fuchsia::component::decl::Use;
using fuchsia::component::decl::UseDirectory;

std::vector<uint8_t> EncodeComponentDecl(Component decl) {
  struct PersistentHeader {
    uint8_t zero = 0u;
    uint8_t magic_number = 1u;    // MAGIC_NUMBER_LITERAL
    uint16_t at_rest_flags = 2u;  // USE_V2_WIRE_FORMAT
    uint32_t reserved = 0u;
  };

  fidl::Encoder encoder;
  encoder.Alloc(fidl::EncodingInlineSize<Component, fidl::Encoder>(&encoder) +
                sizeof(PersistentHeader));

  *encoder.GetPtr<PersistentHeader>(0u) = {};
  decl.Encode(&encoder, sizeof(PersistentHeader));

  return encoder.TakeBytes();
}

void DeclareAndExposeProtocol(Component& decl, base::StringPiece protocol) {
  decl.mutable_capabilities()->push_back(Capability::WithProtocol(
      std::move(Protocol()
                    .set_name(std::string(protocol))
                    .set_source_path("/svc/" + std::string(protocol)))));
  decl.mutable_exposes()->push_back(Expose::WithProtocol(
      std::move(ExposeProtocol()
                    .set_source(Ref::WithSelf({}))
                    .set_source_name(std::string(protocol))
                    .set_target(Ref::WithParent({}))
                    .set_target_name(std::string(protocol)))));
}

}  // namespace

CastResolver::CastResolver() = default;

CastResolver::~CastResolver() = default;

void CastResolver::Resolve(std::string component_url,
                           ResolveCallback callback) {
  Component decl;
  decl.mutable_program()->set_runner("cast-runner");
  decl.mutable_program()->mutable_info()->set_entries({});

  // TODO(crbug.com/1379385): Replace with attributed-capability expose rules
  // for each protocol, when supported by the framework.
  decl.mutable_uses()->push_back(Use::WithDirectory(
      std::move(UseDirectory()
                    .set_source(Ref::WithParent({}))
                    .set_source_name("svc")
                    .set_target_path("/svc")
                    .set_rights(fuchsia::io::RW_STAR_DIR)
                    .set_dependency_type(DependencyType::STRONG))));

  // Declare and expose capabilities implemented by the component.
  DeclareAndExposeProtocol(decl, "fuchsia.ui.app.ViewProvider");
  DeclareAndExposeProtocol(decl, "fuchsia.modular.Lifecycle");

  // TODO(crbug.com/1120914): Remove this with the FrameHost component.
  DeclareAndExposeProtocol(decl, "fuchsia.web.FrameHost");

  // Expose the Binder, from the framework, to allow CastRunnerV1 to start the
  // component.
  decl.mutable_exposes()->push_back(Expose::WithProtocol(
      std::move(ExposeProtocol()
                    .set_source(Ref::WithFramework({}))
                    .set_source_name(fuchsia::component::Binder::Name_)
                    .set_target(Ref::WithParent({}))
                    .set_target_name(fuchsia::component::Binder::Name_))));

  // Encode the component manifest into the resolver result.
  fuchsia::component::resolution::Resolver_Resolve_Response result;
  result.component.set_url(std::move(component_url));
  result.component.mutable_decl()->set_bytes(
      EncodeComponentDecl(std::move(decl)));

  callback(
      fuchsia::component::resolution::Resolver_Resolve_Result::WithResponse(
          std::move(result)));
}

void CastResolver::ResolveWithContext(
    std::string component_url,
    fuchsia::component::resolution::Context context,
    ResolveWithContextCallback callback) {
  NOTIMPLEMENTED();

  callback(
      fuchsia::component::resolution::Resolver_ResolveWithContext_Result::
          WithErr(
              fuchsia::component::resolution::ResolverError::NOT_SUPPORTED));
}
