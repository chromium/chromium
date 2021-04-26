// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_FONT_MANAGER_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_FONT_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "gpu/command_buffer/client/client_discardable_manager.h"
#include "gpu/raster_export.h"
#include "third_party/skia/src/core/SkRemoteGlyphCache.h"

namespace gpu {
class CommandBuffer;

namespace raster {
class RASTER_EXPORT ClientFontManager
    : public SkStrikeServer::DiscardableHandleManager {
 public:
  class RASTER_EXPORT Client {
   public:
    virtual ~Client() {}

    virtual void* MapFontBuffer(uint32_t size) = 0;
  };

  ClientFontManager(Client* client, CommandBuffer* command_buffer);
  ~ClientFontManager() override;

  // SkStrikeServer::DiscardableHandleManager implementation.
  SkDiscardableHandleId createHandle() override;
  bool lockHandle(SkDiscardableHandleId handle_id) override;
  bool isHandleDeleted(SkDiscardableHandleId handle_id) override;

  void Serialize();
  SkStrikeServer* strike_server() { return &strike_server_; }

 private:
  static constexpr SkDiscardableHandleId kInvalidSkDiscardableHandleId = -1;

  Client* client_;
  CommandBuffer* command_buffer_;

  SkDiscardableHandleId last_allocated_handle_id_ = 0u;
  SkStrikeServer strike_server_;
  ClientDiscardableManager client_discardable_manager_;
  base::flat_map<SkDiscardableHandleId, ClientDiscardableHandle::Id>
      discardable_handle_map_;

  SkDiscardableHandleId last_serialized_handle_id_ = 0u;
  base::flat_set<SkDiscardableHandleId> locked_handles_;
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_FONT_MANAGER_H_
