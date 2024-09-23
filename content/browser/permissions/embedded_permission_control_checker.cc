// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/embedded_permission_control_checker.h"

using blink::mojom::EmbeddedPermissionControlClient;
using blink::mojom::PermissionName;

namespace content {

namespace {

// Limits the number of page embedded permission control (PEPC) of a given
// type, so as to limit the potential for abuse/misuse, for example: embedded
// iframes can be intentionally disruptive by appending too many embedded
// permission elements.
// TODO(crbug.com/40275129): Add a command line switch to disable the check
// policy and other security measures.
constexpr static int kMaxPEPCPerPage = 2;

}  // namespace

EmbeddedPermissionControlChecker::EmbeddedPermissionControlChecker(Page& page)
    : PageUserData<EmbeddedPermissionControlChecker>(page) {}

EmbeddedPermissionControlChecker::~EmbeddedPermissionControlChecker() = default;

void EmbeddedPermissionControlChecker::CheckPageEmbeddedPermission(
    std::set<PermissionName> permissions,
    mojo::PendingRemote<EmbeddedPermissionControlClient> pending_client,
    RegisterPageEmbeddedPermissionCallback callback) {
  auto client =
      std::make_unique<Client>(this, std::move(permissions),
                               std::move(pending_client), std::move(callback));
  auto& queue = client_map_[client->permissions()];
  if (queue.size() < kMaxPEPCPerPage) {
    client->OnEmbeddedPermissionControlRegistered(/*allow=*/true);
  }

  queue.push_back(std::move(client));
}

PAGE_USER_DATA_KEY_IMPL(EmbeddedPermissionControlChecker);

void EmbeddedPermissionControlChecker::OnClientDisconnect(
    Client* disconnected_client) {
  auto client_map_it = client_map_.find(disconnected_client->permissions());
  CHECK(client_map_it != client_map_.end() && !client_map_it->second.empty());
  auto& queue = client_map_it->second;
  for (auto& client : queue) {
    if (client.get() == disconnected_client) {
      base::Erase(queue, client);
      break;
    }
  }

  for (auto it = queue.begin();
       it != queue.end() && std::distance(queue.begin(), it) < kMaxPEPCPerPage;
       ++it) {
    (*it)->OnEmbeddedPermissionControlRegistered(/*allow=*/true);
  }
}

EmbeddedPermissionControlChecker::Client::Client(
    EmbeddedPermissionControlChecker* checker,
    std::set<PermissionName> permissions,
    mojo::PendingRemote<EmbeddedPermissionControlClient> client,
    RegisterPageEmbeddedPermissionCallback callback)
    : checker_(checker),
      permissions_(std::move(permissions)),
      client_(std::move(client)),
      callback_(std::move(callback)) {
  client_.set_disconnect_handler(
      base::BindOnce(&Client::OnMojoDisconnect, base::Unretained(this)));
}

EmbeddedPermissionControlChecker::Client::~Client() = default;

void EmbeddedPermissionControlChecker::Client::OnMojoDisconnect() {
  checker_->OnClientDisconnect(this);
  // The previous call has destroyed the object, do not add code after this.
}

void EmbeddedPermissionControlChecker::Client::
    OnEmbeddedPermissionControlRegistered(bool allow) {
  if (callback_) {
    std::move(callback_).Run(allow, client_);
  }
}

}  // namespace content
