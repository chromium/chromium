// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_STREAM_MANAGER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_STREAM_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

FORWARD_DECLARE_TEST(MimeHandlerViewCrossProcessTest, Basic);

namespace extensions {
class Extension;
class StreamContainer;

// A container for streams that have not yet been claimed by a
// MimeHandlerViewGuest. If the embedding RenderFrameHost is closed or navigates
// away from the resource being streamed, the stream is aborted. This is
// BrowserContext-keyed because mime handlers are extensions, which are
// per-BrowserContext.
class MimeHandlerStreamManager : public KeyedService,
                                 public ExtensionRegistryObserver {
 public:
  MimeHandlerStreamManager();
  ~MimeHandlerStreamManager() override;
  static MimeHandlerStreamManager* Get(content::BrowserContext* context);

  // The |frame_tree_node_id| parameter is used for the top level plugins case
  // (PDF, etc). If this parameter has a valid value then it overrides the
  // |render_process_id| and |render_frame_id| parameters.
  // The |render_process_id| is the id of the renderer process.
  // The |render_frame_id| is the routing id of the RenderFrameHost.
  void AddStream(const std::string& view_id,
                 std::unique_ptr<StreamContainer> stream,
                 int frame_tree_node_id,
                 int render_process_id,
                 int render_frame_id);

  std::unique_ptr<StreamContainer> ReleaseStream(const std::string& view_id);

  // ExtensionRegistryObserver override.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(::MimeHandlerViewCrossProcessTest, Basic);

  class EmbedderObserver;

  // Maps view id->StreamContainer to maintain their lifetime until they are
  // used or removed.
  std::map<std::string, std::unique_ptr<StreamContainer>> streams_;

  // Maps extension id->view id for removing the associated streams when an
  // extension is unloaded.
  std::map<std::string, std::set<std::string>> streams_by_extension_id_;

  // Maps view id->EmbedderObserver for maintaining the lifetime of the
  // EmbedderObserver until it is removed.
  std::map<std::string, std::unique_ptr<EmbedderObserver>> embedder_observers_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerStreamManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_STREAM_MANAGER_H_
