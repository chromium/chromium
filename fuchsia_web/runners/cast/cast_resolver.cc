// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/cast_resolver.h"

#include <fidl/fuchsia.component.decl/cpp/fidl.h>
#include <fidl/fuchsia.component/cpp/fidl.h>
#include <fidl/fuchsia.ui.app/cpp/wire_messaging.h>
#include <lib/fidl/cpp/natural_types.h>

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"

namespace {

using fuchsia_component_decl::Ref;

template <typename Protocol>
void DeclareAndExposeProtocol(fuchsia_component_decl::Component& decl) {
  constexpr const char* kProtocolName =
      fidl::DiscoverableProtocolName<Protocol>;
  if (!decl.capabilities()) {
    decl.capabilities().emplace();
  }
  decl.capabilities()->push_back(
      fuchsia_component_decl::Capability::WithProtocol({{
          .name = kProtocolName,
          .source_path = fidl::DiscoverableProtocolDefaultPath<Protocol>,
      }}));
  CHECK(decl.exposes());
  decl.exposes()->push_back(fuchsia_component_decl::Expose::WithProtocol({{
      .source = Ref::WithSelf({}),
      .source_name = kProtocolName,
      .target = Ref::WithParent({}),
      .target_name = kProtocolName,
  }}));
}

uint64_t FetchAbiRevision() {
  constexpr char kPkgAbiRevisionPath[] = "/pkg/meta/fuchsia.abi/abi-revision";

  // Read the Little Endian representation of the unsigned 64-bit integer ABI
  // revision from the file in the metadata directory.
  std::array<uint8_t, 8u> abi_revision_le = {};
  std::optional<uint64_t> read_bytes =
      base::ReadFile(base::FilePath(kPkgAbiRevisionPath), abi_revision_le);
  CHECK_EQ(read_bytes.value(), sizeof(abi_revision_le));
  return base::numerics::U64FromLittleEndian(abi_revision_le);
}

}  // namespace

CastResolver::CastResolver() = default;

CastResolver::~CastResolver() = default;

void CastResolver::Resolve(CastResolver::ResolveRequest& request,
                           CastResolver::ResolveCompleter::Sync& completer) {
  fuchsia_component_decl::Component decl{{
      .program = fuchsia_component_decl::Program{{
          .runner = "cast-runner",
          .info = fuchsia_data::Dictionary{{
              .entries = {},
          }},
      }},

      // TODO(crbug.com/40875550): Replace with attributed-capability expose
      // rules for each protocol, when supported by the framework.
      .uses =
          std::vector{
              fuchsia_component_decl::Use::WithDirectory({{
                  .source = Ref::WithParent({}),
                  .source_name = "svc",
                  .target_path = "/svc",
                  .rights = fuchsia_io::kRwStarDir,
                  .dependency_type =
                      fuchsia_component_decl::DependencyType::kStrong,
              }}),
          },

      // Expose the Binder, from the framework, to allow callers to explicitly
      // start the component.
      .exposes = std::vector{fuchsia_component_decl::Expose::WithProtocol({{
          .source = Ref::WithFramework({}),
          .source_name =
              fidl::DiscoverableProtocolName<fuchsia_component::Binder>,
          .target = Ref::WithParent({}),
          .target_name =
              fidl::DiscoverableProtocolName<fuchsia_component::Binder>,
      }})},
  }};

  // Declare and expose capabilities implemented by the component.
  DeclareAndExposeProtocol<fuchsia_ui_app::ViewProvider>(decl);

  fit::result<fidl::Error, std::vector<uint8_t>> persisted_decl =
      fidl::Persist(decl);
  if (persisted_decl.is_error()) {
    ZX_DLOG(ERROR, persisted_decl.error_value().status())
        << "Error creating persisted decl";
    completer.Reply(
        fit::error(fuchsia_component_resolution::ResolverError::kInternal));
    return;
  }

  // Encode the component manifest into the resolver result.
  static const uint64_t abi_revision = FetchAbiRevision();
  fuchsia_component_resolution::ResolverResolveResponse result{{
      .component = fuchsia_component_resolution::Component{{
          .url = std::move(request.component_url()),
          .decl =
              fuchsia_mem::Data::WithBytes(std::move(persisted_decl.value())),
          .abi_revision = abi_revision,
      }},
  }};

  completer.Reply(fit::ok(std::move(result)));
}

void CastResolver::ResolveWithContext(
    CastResolver::ResolveWithContextRequest& request,
    CastResolver::ResolveWithContextCompleter::Sync& completer) {
  NOTIMPLEMENTED();

  completer.Reply(
      fit::error(fuchsia_component_resolution::ResolverError::kNotSupported));
}
