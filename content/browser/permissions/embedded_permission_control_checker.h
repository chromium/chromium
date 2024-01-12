// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_EMBEDDED_PERMISSION_CONTROL_CHECKER_H_
#define CONTENT_BROWSER_PERMISSIONS_EMBEDDED_PERMISSION_CONTROL_CHECKER_H_

#include <map>
#include <memory>
#include <set>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"

namespace content {

// This class provides utility functions to determine if a page embedded
// permission control (PEPC) is allowed to be inserted to a page, based on the
// situation and policy. If this checker returns no, the corresponding
// permission element is not expected to be rendered on the page. For example,
// there will be at most a limited instance of PEPC for the current page at any
// time, and registering a new one will automatically denied.
class CONTENT_EXPORT EmbeddedPermissionControlChecker
    : public content::PageUserData<EmbeddedPermissionControlChecker> {
 public:
  using RegisterPageEmbeddedPermissionCallback = base::OnceCallback<void(
      bool,
      const mojo::Remote<blink::mojom::EmbeddedPermissionControlClient>&)>;

  explicit EmbeddedPermissionControlChecker(Page& page);

  ~EmbeddedPermissionControlChecker() override;

  EmbeddedPermissionControlChecker(const EmbeddedPermissionControlChecker&) =
      delete;
  EmbeddedPermissionControlChecker& operator=(
      const EmbeddedPermissionControlChecker&) = delete;

  // Checks if the embedded permission identified by given |permissions| is
  // allowed to proceed. If this check returns false, the corresponding
  // permission element will not be rendered.
  void CheckPageEmbeddedPermission(
      std::set<blink::mojom::PermissionName> permissions,
      mojo::PendingRemote<blink::mojom::EmbeddedPermissionControlClient>
          pending_client,
      RegisterPageEmbeddedPermissionCallback callback);

 private:
  friend class content::PageUserData<EmbeddedPermissionControlChecker>;

  PAGE_USER_DATA_KEY_DECL();

  // A wrapper class to bind remote `EmbeddedPermissionControlClient`
  class Client {
   public:
    Client(EmbeddedPermissionControlChecker* checker,
           std::set<blink::mojom::PermissionName> permissions,
           mojo::PendingRemote<blink::mojom::EmbeddedPermissionControlClient>
               client,
           RegisterPageEmbeddedPermissionCallback callback);
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    void OnMojoDisconnect();

    const std::set<blink::mojom::PermissionName>& permissions() const {
      return permissions_;
    }

    // Notify the remote client of whether or not the embedded permission
    // registration is allowed. Ignore if we are notifying multiple times.
    void OnEmbeddedPermissionControlRegistered(bool allow);

   private:
    // This client is owned by `EmbeddedPermissionControlChecker`, it is safe to
    // use raw_ptr here.
    const raw_ptr<EmbeddedPermissionControlChecker> checker_;
    std::set<blink::mojom::PermissionName> permissions_;
    mojo::Remote<blink::mojom::EmbeddedPermissionControlClient> client_;
    RegisterPageEmbeddedPermissionCallback callback_;
  };

  // The given client disconnected, it will be removed from the corresponding
  // queue, then the first `kMaxPEPCPerPage` items in the remaining queue will
  // trigger the notification `OnEmbeddedPermissionControlRegistered`.
  void OnClientDisconnect(Client* client);

  // Records PEPCs per type are associated with this page. At most
  // |kMaxPEPCPerPage| of each type is allowed.
  using ClientKey = std::set<blink::mojom::PermissionName>;
  std::map<ClientKey, base::circular_deque<std::unique_ptr<Client>>>
      client_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_EMBEDDED_PERMISSION_CONTROL_CHECKER_H_
