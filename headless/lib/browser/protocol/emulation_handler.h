// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_EMULATION_HANDLER_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_EMULATION_HANDLER_H_

#include "headless/lib/browser/protocol/domain_handler.h"
#include "headless/lib/browser/protocol/emulation.h"

namespace headless::protocol {

class EmulationHandler : public DomainHandler, public Emulation::Backend {
 public:
  EmulationHandler();

  EmulationHandler(const EmulationHandler&) = delete;
  EmulationHandler& operator=(const EmulationHandler&) = delete;

  ~EmulationHandler() override;

 private:
  // DomainHandler implementation
  void Wire(UberDispatcher* dispatcher) override;
  Response Disable() override;

  // Emulation::Backend implementation
  Response GetScreenInfos(
      std::unique_ptr<protocol::Array<protocol::Emulation::ScreenInfo>>*
          out_screen_infos) override;

  Response AddScreen(
      int left,
      int top,
      int width,
      int height,
      std::unique_ptr<protocol::Emulation::WorkAreaInsets> work_area_insets,
      std::optional<double> device_pixel_ratio,
      std::optional<int> rotation,
      std::optional<int> color_depth,
      std::optional<String> label,
      std::optional<bool> is_internal,
      std::unique_ptr<protocol::Emulation::ScreenInfo>* out_screen_info)
      override;

  Response RemoveScreen(const String& screen_id) override;
};

}  // namespace headless::protocol

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_EMULATION_HANDLER_H_
