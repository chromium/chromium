// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/embedded_permission_control_checker.h"

#include <cstddef>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "content/browser/log_console_message.h"
#include "third_party/blink/public/common/features_generated.h"

using blink::mojom::EmbeddedPermissionControlClient;
using blink::mojom::PermissionName;

namespace content {

namespace {

// Limits the number of page embedded permission control (PEPC) of a given
// type, so as to limit the potential for abuse/misuse, for example: embedded
// iframes can be intentionally disruptive by appending too many embedded
// permission elements.
constexpr static int kMaxPEPCPerPage = 3;
constexpr static int kMaxInstallPerPage = 24;

size_t GetMaxElementsPerPageForSource(
    EmbeddedPermissionControlChecker::Source source) {
  if (source == EmbeddedPermissionControlChecker::Source::kInstallElement) {
    return kMaxInstallPerPage;
  }
  return kMaxPEPCPerPage;
}

}  // namespace

EmbeddedPermissionControlChecker::EmbeddedPermissionControlChecker(Page& page)
    : PageUserData<EmbeddedPermissionControlChecker>(page) {}

EmbeddedPermissionControlChecker::~EmbeddedPermissionControlChecker() = default;

void EmbeddedPermissionControlChecker::CheckPageEmbeddedPermission(
    Source source,
    std::set<PermissionName> permissions,
    mojo::PendingRemote<EmbeddedPermissionControlClient> pending_client,
    RegisterPageEmbeddedPermissionCallback callback) {
  auto client =
      std::make_unique<Client>(this, source, std::move(permissions),
                               std::move(pending_client), std::move(callback));
  ClientKey key(client->source(), client->permissions());
  auto& queue = client_map_[key];

  const size_t max_elements = GetMaxElementsPerPageForSource(source);
  const bool should_allow = queue.size() < max_elements ||
                            base::FeatureList::IsEnabled(
                                blink::features::kBypassPepcSecurityForTesting);

  queue.push_back(std::move(client));
  if (queue.size() == max_elements) {
    page().GetMainDocument().AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "Maximum limit of " + base::NumberToString(max_elements) +
            " permission elements has been reached. More permission"
            " elements can be added but they will not be clickable");
  }

  if (should_allow) {
    queue.back()->OnEmbeddedPermissionControlRegistered(/*allow=*/true);
  }
}

bool EmbeddedPermissionControlChecker::HasPageEmbeddedPermission(
    Source source,
    const std::set<PermissionName>& permissions) const {
  if (base::FeatureList::IsEnabled(
          blink::features::kBypassPepcSecurityForTesting)) {
    return true;
  }
  ClientKey key(source, permissions);
  auto client_map_it = client_map_.find(key);
  if (client_map_it == client_map_.end() || client_map_it->second.empty()) {
    return false;
  }
  return true;
}

PAGE_USER_DATA_KEY_IMPL(EmbeddedPermissionControlChecker);

void EmbeddedPermissionControlChecker::OnClientDisconnect(
    Client* disconnected_client) {
  // Store the max elements for the source type before erasing the client.
  const size_t max_elements =
      GetMaxElementsPerPageForSource(disconnected_client->source());

  ClientKey key(disconnected_client->source(),
                disconnected_client->permissions());
  auto client_map_it = client_map_.find(key);
  CHECK(client_map_it != client_map_.end() && !client_map_it->second.empty());
  auto& queue = client_map_it->second;
  base::EraseIf(queue, [disconnected_client](const auto& client) {
    return client.get() == disconnected_client;
  });

  if (queue.empty()) {
    client_map_.erase(client_map_it);
    return;
  }

  // Gather weak pointers to the clients to notify before invoking callbacks,
  // ensuring iterator and callback safety if the queue is modified reentrantly.
  std::vector<base::WeakPtr<Client>> clients_to_notify;
  clients_to_notify.reserve(max_elements);
  for (size_t i = 0; i < queue.size() && i < max_elements; ++i) {
    clients_to_notify.push_back(queue[i]->AsWeakPtr());
  }

  for (auto& weak_client : clients_to_notify) {
    if (weak_client) {
      weak_client->OnEmbeddedPermissionControlRegistered(/*allow=*/true);
    }
  }
}

EmbeddedPermissionControlChecker::Client::Client(
    EmbeddedPermissionControlChecker* checker,
    Source source,
    std::set<PermissionName> permissions,
    mojo::PendingRemote<EmbeddedPermissionControlClient> client,
    RegisterPageEmbeddedPermissionCallback callback)
    : checker_(checker),
      source_(source),
      permissions_(std::move(permissions)),
      client_(std::move(client)),
      callback_(std::move(callback)) {
  client_.set_disconnect_handler(
      base::BindOnce(&Client::OnMojoDisconnect, base::Unretained(this)));
}

EmbeddedPermissionControlChecker::Client::~Client() = default;

EmbeddedPermissionControlChecker::ClientKey::ClientKey(
    Source source,
    std::set<PermissionName> permissions)
    : source(source), permissions(std::move(permissions)) {}

EmbeddedPermissionControlChecker::ClientKey::ClientKey(const ClientKey& other) =
    default;
EmbeddedPermissionControlChecker::ClientKey::ClientKey(ClientKey&& other) =
    default;
EmbeddedPermissionControlChecker::ClientKey::~ClientKey() = default;

bool EmbeddedPermissionControlChecker::ClientKey::operator<(
    const ClientKey& other) const {
  if (source != other.source) {
    return source < other.source;
  }
  return permissions < other.permissions;
}

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
