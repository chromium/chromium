// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_STREAM_MANAGER_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_STREAM_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

FORWARD_DECLARE_TEST(ChromeMimeHandlerViewTest, Basic);

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

  MimeHandlerStreamManager(const MimeHandlerStreamManager&) = delete;
  MimeHandlerStreamManager& operator=(const MimeHandlerStreamManager&) = delete;

  ~MimeHandlerStreamManager() override;
  static MimeHandlerStreamManager* Get(content::BrowserContext* context);

  // The |frame_tree_node_id| parameter is used for the top level plugins case
  // (PDF, etc).
  void AddStream(const std::string& stream_id,
                 std::unique_ptr<StreamContainer> stream,
                 content::FrameTreeNodeId frame_tree_node_id);

  std::unique_ptr<StreamContainer> ReleaseStream(const std::string& stream_id);

  // ExtensionRegistryObserver override.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  static void EnsureFactoryBuilt();

 private:
  FRIEND_TEST_ALL_PREFIXES(::ChromeMimeHandlerViewTest, Basic);

  class EmbedderObserver;

  // Maps stream id->StreamContainer to maintain their lifetime until they are
  // used or removed.
  std::map<std::string, std::unique_ptr<StreamContainer>> streams_;

  // Maps extension id->stream id for removing the associated streams when an
  // extension is unloaded.
  std::map<std::string, std::set<std::string>> streams_by_extension_id_;

  // Maps stream id->EmbedderObserver for maintaining the lifetime of the
  // EmbedderObserver until it is removed.
  std::map<std::string, std::unique_ptr<EmbedderObserver>> embedder_observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_MIME_HANDLER_STREAM_MANAGER_H_
