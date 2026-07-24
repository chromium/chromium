// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_COMMAND_SERIALIZERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_COMMAND_SERIALIZERS_H_

#include <array>
#include <cstddef>
#include <optional>
#include <span>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gl/buildflags.h"

#if BUILDFLAG(USE_DAWN)
#include <dawn/wire/Wire.h>
#endif

namespace blink {

#if BUILDFLAG(USE_DAWN)
class PLATFORM_EXPORT DawnNoopCommandSerializer
    : public dawn::wire::CommandSerializer {
 public:
  DawnNoopCommandSerializer();
  ~DawnNoopCommandSerializer() override;

  size_t GetMaximumAllocationSize() const override;
  std::optional<std::span<volatile std::byte>> GetCommandSpace(
      size_t size) override;
  bool Flush() override;

 private:
  alignas(8) std::array<std::byte, 1024> buf_;
};

class PLATFORM_EXPORT DawnTestingCommandSerializer
    : public dawn::wire::CommandSerializer {
 public:
  DawnTestingCommandSerializer();
  ~DawnTestingCommandSerializer() override;

  size_t GetMaximumAllocationSize() const override;
  void SetHandler(dawn::wire::CommandHandler* handler);
  std::optional<std::span<volatile std::byte>> GetCommandSpace(
      size_t size) override;
  bool Flush() override;

 private:
  alignas(8) std::array<std::byte, 1024 * 1024> backing_buf_;
  base::raw_span<std::byte> buf_ = backing_buf_;
  raw_ptr<dawn::wire::CommandHandler> handler_ = nullptr;
};
#endif  // BUILDFLAG(USE_DAWN)

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DAWN_COMMAND_SERIALIZERS_H_
