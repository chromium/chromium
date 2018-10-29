// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_REMOTE_VIEW_HOST_SERVER_REMOTE_VIEW_HOST_H_
#define SERVICES_WS_REMOTE_VIEW_HOST_SERVER_REMOTE_VIEW_HOST_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "ui/views/controls/native/native_view_host.h"

namespace aura {
class Window;
}

namespace ws {

class WindowService;

// A view the owner of the WindowService may use for embedding. This class
// is specifically designed to work with clients that have scheduled an
// embedding (ScheduleEmbedForExistingClient()) and then passed the token
// to the owner of the WindowService.
//
// Typical usage:
// 1. remote client creates views::RemoteViewProvider with window.
// 2. remote client calls RemoveViewProvider::GetEmbedToken().
// 3. remote client passes token to owner of WindowService over another mojo
//    connection.
// 4. owner of WindowService creates ServerRemoveViewHost and calls
//    EmbedUsingToken() with the token supplied in step 3.
class ServerRemoteViewHost : public views::NativeViewHost {
 public:
  explicit ServerRemoteViewHost(WindowService* window_service);
  ~ServerRemoteViewHost() override;

  // Embeds the remote contents after this view is added to a widget.
  // |embed_token| is the token obtained from the WindowTree embed API
  // (ScheduleEmbed/ForExistingClient). |embed_flags| are the embedding flags
  // (see window_tree_constants.mojom). |callback| is an optional callback
  // invoked with the embed result.
  // Note that |callback| should not be used to add the view to a widget because
  // the actual embedding only happens after the view is added.
  using EmbedCallback = base::OnceCallback<void(bool success)>;
  void EmbedUsingToken(const base::UnguessableToken& embed_token,
                       int embed_flags,
                       EmbedCallback callback);

  aura::Window* embedding_root() { return embedding_root_.get(); }

 private:
  bool IsEmbedPending() const { return !embed_token_.is_empty(); }

  void EmbedImpl();

  // views::NativeViewHost:
  void AddedToWidget() override;

  WindowService* window_service_;
  base::UnguessableToken embed_token_;
  int embed_flags_ = 0;
  EmbedCallback embed_callback_;
  const std::unique_ptr<aura::Window> embedding_root_;

  DISALLOW_COPY_AND_ASSIGN(ServerRemoteViewHost);
};

}  // namespace ws

#endif  // SERVICES_WS_REMOTE_VIEW_HOST_SERVER_REMOTE_VIEW_HOST_H_
